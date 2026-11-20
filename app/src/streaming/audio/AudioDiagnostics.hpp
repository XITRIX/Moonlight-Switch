#pragma once
#include "../../../../extern/moonlight-common-c/src/AudioTrace.h"
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <string>

// Optional diagnostics only. Audio/network producers copy fixed-size records;
// they never format text, allocate, or perform file I/O here.
class AudioDiagnostics {
    struct Record { uint64_t time, sequence; int64_t a,b,c,d; int stage; };
    static constexpr unsigned Capacity=8192, Batch=256;
    Mutex mutex{};
    CondVar wake{};
    Thread thread{};
    Record* records=nullptr;
    FILE* file=nullptr;
    unsigned read=0,write=0,count=0;
    uint64_t accepted=0,lost=0;
    bool stopping=false, created=false, started=false;

    static void capture(int stage,uint64_t time,uint64_t sequence,int64_t a,int64_t b,int64_t c,int64_t d) {
        auto& self=instance();
        mutexLock(&self.mutex);
        if(self.records&&!self.stopping){
            if(self.count==Capacity) ++self.lost;
            else {
                self.records[self.write]={time,sequence,a,b,c,d,stage};
                self.write=(self.write+1)%Capacity;++self.count;++self.accepted;
                if(self.count==Batch)condvarWakeOne(&self.wake);
            }
        }
        mutexUnlock(&self.mutex);
    }
    static void entry(void* context){static_cast<AudioDiagnostics*>(context)->run();}
    void run(){
        Record batch[Batch];
        uint64_t previousLost=0;
        for(;;){
            mutexLock(&mutex);
            if(!count&&!stopping)condvarWaitTimeout(&wake,&mutex,250000000);
            unsigned n=count<Batch?count:Batch;
            for(unsigned i=0;i<n;++i){batch[i]=records[read];read=(read+1)%Capacity;}
            count-=n;
            const bool done=stopping&&!count;
            const uint64_t dropped=lost;
            mutexUnlock(&mutex);
            if(dropped!=previousLost){
                std::fprintf(file,"# TRACE_OVERFLOW dropped_records=%llu\n",(unsigned long long)dropped);
                previousLost=dropped;
            }
            for(unsigned i=0;i<n;++i){const auto& r=batch[i];
                std::fprintf(file,"%llu,%d,%llu,%lld,%lld,%lld,%lld\n",
                    (unsigned long long)r.time,r.stage,(unsigned long long)r.sequence,
                    (long long)r.a,(long long)r.b,(long long)r.c,(long long)r.d);
            }
            if(n)std::fflush(file);
            if(done)break;
        }
    }
public:
    static AudioDiagnostics& instance(){static AudioDiagnostics self;return self;}
    bool start(const std::string& directory){
        stop();
        mutexInit(&mutex);condvarInit(&wake);
        read=write=count=0;accepted=lost=0;stopping=false;
        records=static_cast<Record*>(std::malloc(sizeof(Record)*Capacity));
        if(!records)return false;
        // Append sessions rather than destroying evidence on reconnect.
        const std::string path=directory+"moonlight-LL26-audio-log.txt";
        file=std::fopen(path.c_str(),"a");
        if(!file){std::free(records);records=nullptr;return false;}
        std::setvbuf(file,nullptr,_IOFBF,64*1024);
        std::fprintf(file,"# LL26 AUDIO session_begin timestamps=monotonic_us; seq=RTP uint16 (wraps); 18446744073709551615=unknown/PLC\n");
        std::fprintf(file,"# time_us,stage,sequence,a,b,c,d\n");
        std::fprintf(file,"# stages: 1=recv_begin(use_select,poll_timeout_ms);2=recv_end(type,bytes,rtp_timestamp,ssrc);3=startup_drop(remaining,type,0,0);4=rtp_enter(bytes,type,0,0);5=rtp_result(status,fec_recovered,fec_failed,out_of_order);6=fec_out(bytes,0,0,0);7=lbq_offer(bytes,queue_items,0,0);8=lbq_result(result,queue_items,0,0);9=lbq_wait;10=lbq_dequeue(result,bytes,queue_items,0);11=decrypt_begin(bytes);12=decrypt_end(result,bytes);13=callback_begin(bytes,pending_ms);14=callback_end;20=lock_begin(bytes);21=lock_acquired;22=opus_begin(bytes,frame_samples,0,0);23=opus_end(decoded_samples,duration_us,0,0);24=pcm_queue(samples,software_samples,driver_ms,last_sequence);25=pcm_drop(reason,samples,queue_ms,pending_ms);30=pump_begin(software_samples,driver_ms,callback_gap_us,0);31=update_begin;32=update_end(result,duration_us,0,0);33=voice_state(played_raw,precise_ms,coarse_ms,voice_drops);34=buffer_submit(index,samples,result,software_samples);35=underrun(software_samples,staged_samples,callback_gap_us,played_raw);36=voice_start(queued_ms,reserve_ms);37=pump_end(software_samples,driver_ms,pcm_blocks,0);38=wave_states(waiting,queued,playing,done);40=summary_log_begin;41=summary_log_end;50=config(rate,channels,frame_samples,reserve_ms);51=end(decoded_samples,accepted_samples,discarded_samples,rebuffers)\n");
        std::fflush(file);
        Result result=threadCreate(&thread,entry,this,nullptr,0x10000,0x30,-2);
        if(R_FAILED(result)){stop();return false;}
        created=true;
        result=threadStart(&thread);
        if(R_FAILED(result)){stop();return false;}
        started=true;
        LiSetAudioTraceCallback(capture);
        return true;
    }
    void stop(){
        LiSetAudioTraceCallback(nullptr);
        mutexLock(&mutex);stopping=true;condvarWakeAll(&wake);mutexUnlock(&mutex);
        if(started){threadWaitForExit(&thread);started=false;}
        if(created){threadClose(&thread);created=false;}
        // A callback already loaded before unregistering serializes here.
        mutexLock(&mutex);
        if(file){std::fprintf(file,"# session_end recorded=%llu lost_diagnostic_records=%llu\n",(unsigned long long)accepted,(unsigned long long)lost);std::fclose(file);file=nullptr;}
        if(records){std::free(records);records=nullptr;}
        count=0;mutexUnlock(&mutex);
    }
};
