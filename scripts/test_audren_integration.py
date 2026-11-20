"""Production Audren renderer against a deterministic four-slot SDK fixture.

This models scheduling and ownership, not the real DSP or analog output.
"""
from pathlib import Path
import argparse, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <sys/types.h>
#include "AudioReserve.hpp"
#include "AudioSampleQueue.hpp"
#include "AudioQueuePolicy.hpp"
#include "AudioSeam.hpp"
#include "AudioTrace.h"
using s16=int16_t;using Result=uint32_t;
#define R_FAILED(x) ((x)!=0)
#define DR_OK 0
#define OPUS_OK 0
#define AUDREN_SAMPLES_PER_FRAME_48KHZ 240
#define AUDREN_MEMPOOL_ALIGNMENT 4096
#define AUDREN_DEFAULT_DEVICE_NAME "test"
#define AUDREN_FINAL_MIX_ID 0
enum {AudioRendererOutputRate_48kHz,PcmFormat_Int16};
struct AudioRendererConfig {int output_rate,num_voices,num_effects,num_sinks,num_mix_objs,num_mix_buffers;};
enum AudioDriverWaveBufState {AudioDriverWaveBufState_Free,AudioDriverWaveBufState_Waiting,AudioDriverWaveBufState_Queued,AudioDriverWaveBufState_Playing,AudioDriverWaveBufState_Done};
struct AudioDriverWaveBuf {const void* data_raw;uint64_t size;int start_sample_offset,end_sample_offset;AudioDriverWaveBufState state;};
struct AudioDriver {int initialized;};
struct OpusConfig {int channelCount=2,sampleRate=48000,samplesPerFrame=240,streams=1,coupledStreams=1;unsigned char mapping[2]{0,1};};
using POPUS_MULTISTREAM_CONFIGURATION=OpusConfig*;
struct OpusMSDecoder {};
struct IAudioRenderer {virtual ~IAudioRenderer(){};virtual int init(int,const POPUS_MULTISTREAM_CONFIGURATION,void*,int)=0;virtual void cleanup()=0;virtual void decode_and_play_sample(char*,int)=0;virtual int capabilities()=0;};
struct Settings {int reserve=30,volume=100;bool log=false;static Settings& instance(){static Settings s;return s;}int audio_reserve_ms(){return reserve;}int get_volume(){return volume;}bool save_diagnostic_log(){return log;}std::string launch_path(){return "test.nro";}};
struct AudioDiagnostics {static AudioDiagnostics& instance(){static AudioDiagnostics d;return d;}bool start(const std::string&){return false;}void stop(){}};
static thread_local bool insidePumpThread=false;
static std::atomic<int> slowLogMs=0, activeLogs=0;
namespace brls {struct Logger {template<class... T>static void info(T...){assert(!insidePumpThread);if(int delay=slowLogMs.load()){++activeLogs;std::this_thread::sleep_for(std::chrono::milliseconds(delay));--activeLogs;}}template<class... T>static void error(T...){assert(!insidePumpThread);};};}
static uint64_t nowUs=0,silenceSamples=0;
static uint32_t played=0;
static int failStage=0,failUpdates=0,failAdds=0,decodeResult=240,pendingMs=0,opusInstances=0;
using Mutex=std::mutex;
using CondVar=std::condition_variable;
struct Thread {void(*entry)(void*)=nullptr;void* arg=nullptr;std::thread handle;bool created=false,started=false;};
static bool realThreads=false;
static int liveThreadHandles=0;
static std::atomic<int> liveEntries=0;
static std::thread::id pumpId;
void mutexInit(Mutex*){}
void mutexLock(Mutex*m){m->lock();}
void mutexUnlock(Mutex*m){m->unlock();}
void condvarInit(CondVar*){}
void condvarWakeAll(CondVar*c){c->notify_all();}
void condvarWakeOne(CondVar*c){c->notify_one();}
Result condvarWaitTimeout(CondVar*c,Mutex*m,uint64_t n){std::unique_lock<std::mutex> l(*m,std::adopt_lock);c->wait_for(l,std::chrono::nanoseconds(n));l.release();return 0;}
Result condvarWait(CondVar*c,Mutex*m){std::unique_lock<std::mutex> l(*m,std::adopt_lock);c->wait(l);l.release();return 0;}
Result threadCreate(Thread*t,void(*entry)(void*),void*arg,void*,size_t,int,int){if(failStage==5)return 1;assert(!t->created);t->created=true;t->entry=entry;t->arg=arg;++liveThreadHandles;return 0;}
Result threadStart(Thread*t){if(failStage==6)return 1;t->started=true;if(realThreads)t->handle=std::thread([t]{pumpId=std::this_thread::get_id();++liveEntries;insidePumpThread=true;t->entry(t->arg);insidePumpThread=false;--liveEntries;});return 0;}
Result threadWaitForExit(Thread*t){assert(t->started);if(t->handle.joinable())t->handle.join();t->started=false;return 0;}
Result threadClose(Thread*t){assert(t->created&&!t->started&&!t->handle.joinable());t->created=false;--liveThreadHandles;return 0;}
static bool rendererOpen=false,driverOpen=false,playing=false;
static double pitch=1,deviceSpeed=1,fraction=0;
static std::deque<AudioDriverWaveBuf*> buffers,hardware;
static std::map<AudioDriverWaveBuf*,int> remaining;
uint64_t LiGetMicroseconds(){return realThreads?std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count():nowUs;}
uint64_t LiGetMillis(){return LiGetMicroseconds()/1000;}
static std::atomic<AudioTraceCallback> traceHook{nullptr};
void LiSetAudioTraceCallback(AudioTraceCallback cb){traceHook=cb;}
void LiAudioTraceEvent(int stage,uint64_t seq,int64_t a,int64_t b,int64_t c,int64_t d){if(auto cb=traceHook.load())cb(stage,LiGetMicroseconds(),seq,a,b,c,d);}
uint64_t LiAudioTraceCurrentSequence(){return 99;}
int LiGetPendingAudioDuration(){return pendingMs;}
void* memalign(size_t,size_t n){return std::malloc(n);}
OpusMSDecoder* opus_multistream_decoder_create(int,int,int,int,const unsigned char*,int* e){*e=failStage==4?-1:0;if(*e)return nullptr;++opusInstances;return new OpusMSDecoder;}
void opus_multistream_decoder_destroy(OpusMSDecoder*p){--opusInstances;delete p;}
int opus_multistream_decode(OpusMSDecoder*,const unsigned char*,int,s16* out,int capacity,int){if(decodeResult<0)return decodeResult;int n=std::min(capacity,decodeResult);std::fill(out,out+n*2,1000);return n;}
Result audrenInitialize(const AudioRendererConfig*){if(failStage==1)return 1;rendererOpen=true;return 0;}
void audrenExit(){rendererOpen=false;}
Result audrvCreate(AudioDriver*,const AudioRendererConfig*,int){if(failStage==2)return 1;driverOpen=true;buffers.clear();hardware.clear();remaining.clear();return 0;}
void audrvClose(AudioDriver*){assert(liveEntries==0);driverOpen=false;buffers.clear();hardware.clear();remaining.clear();}
void audrvVoiceStop(AudioDriver*,int){playing=false;played=0;for(auto*w:buffers)w->state=AudioDriverWaveBufState_Done;buffers.clear();hardware.clear();remaining.clear();}
bool audrvVoiceInit(AudioDriver*,int,int,int,int){playing=false;played=0;pitch=1;fraction=0;return true;}
void audrvVoiceSetDestinationMix(AudioDriver*,int,int){}
void audrvVoiceSetMixFactor(AudioDriver*,int,float,int,int){}
void audrvVoiceSetPitch(AudioDriver*,int,float v){pitch=v;}
void audrvVoiceStart(AudioDriver*,int){playing=true;}
bool audrvVoiceIsPlaying(AudioDriver*,int){return playing&&!buffers.empty();}
uint32_t audrvVoiceGetPlayedSampleCount(AudioDriver*,int){return played;}
uint32_t audrvVoiceGetVoiceDropsCount(AudioDriver*,int){return 0;}
bool audrvVoiceAddWaveBuf(AudioDriver*,int,AudioDriverWaveBuf*w){if(failAdds){--failAdds;return false;}assert(w->state==AudioDriverWaveBufState_Free||w->state==AudioDriverWaveBufState_Done);w->state=AudioDriverWaveBufState_Waiting;buffers.push_back(w);remaining[w]=w->end_sample_offset-w->start_sample_offset;return true;}
Result audrvUpdate(AudioDriver*){assert(driverOpen);if(realThreads)assert(std::this_thread::get_id()==pumpId);if(failUpdates){--failUpdates;return 7;}for(auto*w:buffers){if(hardware.size()==4)break;if(w->state==AudioDriverWaveBufState_Waiting){w->state=AudioDriverWaveBufState_Queued;hardware.push_back(w);}}if(!hardware.empty())hardware.front()->state=AudioDriverWaveBufState_Playing;return 0;}
int audrvMemPoolAdd(AudioDriver*,void*,int){return 0;}
void audrvMemPoolAttach(AudioDriver*,int){}
void audrvDeviceSinkAdd(AudioDriver*,const char*,int,const uint8_t*){}
Result audrenStartAudioRenderer(){return failStage==3?1:0;}
void armDCacheFlush(void*,size_t){}
static void advance(int us){nowUs+=us;if(!playing)return;double exact=us*.048*pitch*deviceSpeed+fraction;int budget=(int)exact;fraction=exact-budget;while(budget&&!hardware.empty()){auto*w=hardware.front();int n=std::min(budget,remaining[w]);remaining[w]-=n;budget-=n;played+=n;if(!remaining[w]){assert(buffers.front()==w);w->state=AudioDriverWaveBufState_Done;buffers.pop_front();hardware.pop_front();remaining.erase(w);}}silenceSamples+=budget;}
'''

SCENARIOS = r'''
struct Metrics {uint64_t dropped,silence,updates,accepted;};
static Metrics run(bool legacy,int jitterUs,double speed=1,int durationMs=20000,int reserve=30,int packetSamples=240){
 nowUs=0;silenceSamples=0;deviceSpeed=speed;failStage=failUpdates=failAdds=0;decodeResult=240;pendingMs=0;
 Settings::instance().reserve=reserve;Settings::instance().volume=100;
 decodeResult=packetSamples;
 AudrenAudioRenderer audio;OpusConfig config;config.samplesPerFrame=packetSamples;assert(audio.init(0,&config,nullptr,0)==0);
 // Packet-coupled mode intentionally omits periodic servicing.
 int produced=0;char packet=1;
 for(int t=1000;t<=durationMs*1000;t+=1000){
  advance(1000);
  if(!legacy && t%5000==0)audio.pump_once();
  if(jitterUs&&t%500000<jitterUs)continue;
  int packetUs=packetSamples*1000000LL/48000;
  int due=t/packetUs;
  while(produced<due){pendingMs=(due-produced-1)*packetUs/1000;audio.decode_and_play_sample(&packet,1);++produced;}
  if(audio.m_pcm_count)audio.pump_once();
 }
 Metrics result{audio.m_dropped_packets,silenceSamples,audio.m_update_calls,audio.m_accepted_samples};
 assert(audio.m_sample_queue.milliseconds(48000)<=static_cast<unsigned>(reserve+30+packetSamples/48));
 assert(audio.m_decoded_samples==audio.m_accepted_samples+audio.m_discarded_samples+audio.m_staged_samples+audio.m_pcm_samples);
 audio.cleanup();assert(!rendererOpen&&!driverOpen&&opusInstances==0&&liveThreadHandles==0);
 return result;
}
int main(){
 auto steady=run(false,0);assert(steady.dropped==0&&steady.silence==0);
 for(int reserve:{0,10,20,30,40,50,60,70,80,90,100})for(int samples:{240,480,960}){
  auto playback=run(false,0,1,10000,reserve,samples);
  if(playback.dropped||playback.silence)std::cerr<<"steady reserve="<<reserve<<" frame="<<samples<<" dropped="<<playback.dropped<<" silence="<<playback.silence<<"\n";
  assert(playback.dropped==0&&playback.silence==0);
 }
 for(int reserve:{0,10,20,30,40,50,60,70,80,90,100})for(int jitter:{10000,25000,50000,100000})run(false,jitter,1,5000,reserve);
 for(int volume:{0,50,100,200}){s16 samples[]{32767,-32768,1000,-1000};applyVolume(samples,4,volume);assert(samples[2]==std::min(32767,1000*volume/100));assert(samples[0]<=32767&&samples[1]>=-32768);}
 // Prime 60 ms: four 10 ms slots armed, another 20 ms waiting in software.
 auto serviceGap=[](bool periodic){
  nowUs=0;silenceSamples=0;deviceSpeed=1;decodeResult=240;pendingMs=0;
  Settings::instance().reserve=30;AudrenAudioRenderer audio;OpusConfig c;char p=1;
  assert(audio.init(0,&c,nullptr,0)==0);
  for(int i=0;i<12;++i){audio.decode_and_play_sample(&p,1);if(audio.m_pcm_count)audio.pump_once();}
  assert(audio.m_accepted_samples==2880);
  for(int i=0;i<10;++i){advance(5000);if(periodic)audio.pump_once();}
  auto silence=silenceSamples;audio.cleanup();return silence;
 };
 auto coupled=serviceGap(false),independent=serviceGap(true);
 std::cout<<"50 ms callback gap with queued PCM: coupled silence_samples="<<coupled<<"; independent="<<independent<<"\n";
 assert(coupled>0&&independent==0);
 for(double speed:{.995,1.005})run(false,0,speed,60000);
 run(false,0,1,1800000); // thirty minutes of simulated continuous playback
 for(int session=0;session<100;++session)run(false,0,1,100);
 run(false,100000); // sustained stall remains bounded and cleans up
 for(int stage:{1,2,3,4,5,6}){failStage=stage;AudrenAudioRenderer a;OpusConfig c;assert(a.init(0,&c,nullptr,0)!=0);a.cleanup();assert(!rendererOpen&&!driverOpen&&opusInstances==0&&liveThreadHandles==0);}
 failStage=0;deviceSpeed=1;nowUs=0;Settings::instance().reserve=10;
 AudrenAudioRenderer a;OpusConfig c;assert(a.init(0,&c,nullptr,0)==0);char p=1;
 decodeResult=-1;a.decode_and_play_sample(&p,1);assert(a.m_decode_errors==1);decodeResult=240;
 failUpdates=1;a.decode_and_play_sample(&p,1);a.pump_once();assert(a.m_update_errors==1);
 a.decode_and_play_sample(nullptr,0);assert(a.m_concealed==1);
 failAdds=1;a.decode_and_play_sample(&p,1);a.pump_once();assert(a.m_drop_capacity==1);
 for(int i=0;i<30;++i){advance(5000);a.decode_and_play_sample(&p,1);a.pump_once();}
 a.cleanup();a.cleanup();assert(!rendererOpen&&!driverOpen&&opusInstances==0&&liveThreadHandles==0);
}
'''

def strip(text):
    return '\n'.join(l for l in text.splitlines() if not l.lstrip().startswith('#'))

def run_scenarios(scenarios, cxx='g++'):
    audio=ROOT/'app/src/streaming/audio'
    header=strip((audio/'AudrenAudioRenderer.hpp').read_text())
    source=strip((audio/'AudrenAudioRenderer.cpp').read_text())
    # BUFFER_COUNT is a production preprocessor constant stripped with includes.
    import re
    count=re.search(r'#define BUFFER_COUNT (\d+)',(audio/'AudrenAudioRenderer.hpp').read_text())[1]
    code=FIXTURE+f'\n#define BUFFER_COUNT {count}\n#define private public\n'+header+'\n#undef private\n'+source+scenarios
    with tempfile.TemporaryDirectory() as t:
        src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
        subprocess.run([cxx,'-std=c++20','-O2','-pthread','-I'+str(audio),'-I'+str(ROOT/'extern/moonlight-common-c/src'),str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=60)

def main():
    p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');args=p.parse_args()
    run_scenarios(SCENARIOS,args.cxx)
    print('PASS: production renderer steady playback, four-slot jitter, drift, stalls, Opus errors/PLC, driver errors, buffer rejection, init failures and cleanup')

if __name__=='__main__':main()
