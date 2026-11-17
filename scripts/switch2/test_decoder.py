#!/usr/bin/env python3
"""Compile the real native backend against host SDK/queue mocks and exercise it."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for decoder tests")
    with tempfile.TemporaryDirectory(prefix="moonlight-decoder-") as name:
        tmp = Path(name)

        def write(name, text):
            p = tmp / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text)

        write(
            "vulkan/vulkan.h",
            "#pragma once\n#include <stdint.h>\ntypedef uint64_t VkBuffer;\ntypedef uint64_t VkBufferView;\n#define VK_NULL_HANDLE 0\n",
        )
        write(
            "switch2/services/gsv.h",
            "#pragma once\n#include <stdint.h>\n#include <stddef.h>\ntypedef uint32_t Result;\ntypedef struct {uint32_t memory_id;void* address;uint64_t size;bool has_attribute;} GsvNativeMemory;\n",
        )
        write(
            "switch2/video/decoder.h",
            r"""
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint32_t Result;
#define R_VALUE(rc) ((rc)&0x3fffff)
#define R_FAILED(rc) ((rc)!=0)
#define R_SUCCEEDED(rc) ((rc)==0)
#define KERNELRESULT(value) 0xea01u
#define MAKERESULT(m,d) (((d)<<9)|(m))
#define Module_Libnx 345
#define LibnxError_BadInput 11
#define VIDEO_DECODER_DEFAULT_IMAGES 4
#define VIDDEC_MAX_IMAGES 8
#define VIDEO_DECODER_MAX_FRAME_RATE 60
#define VIDEO_DECODER_PIXEL_RATE 248832000ULL
typedef enum { ViddecCodec_H264=0, ViddecCodec_Av1=3 } ViddecCodec;
typedef struct {
 ViddecCodec codec; uint32_t width,height,image_count,frame_rate,tiling,color_policy;
 uint64_t max_sample_size; const void* codec_config; size_t codec_config_size;
} VideoDecoderConfig;
enum { GsvCpuImageType_Nv12PitchLinear=10, GsvCpuImageType_Nv12PitchLinearAlt=20 };
typedef struct { uint32_t offset,span; uint64_t pitch; } GsvCpuImagePlane;
typedef struct { uint32_t type; GsvCpuImagePlane planes[2]; } GsvCpuImageDescriptor;
typedef struct { void* backing; uint64_t backing_size; GsvCpuImageDescriptor descriptor; } VideoDecoderImage;
typedef struct { int open; } ViddecDecoder;
typedef int GsvSync;
typedef struct {
 int initialized; uint32_t image_count; VideoDecoderImage images[8];
 bool pending_armed,started,decoder_open; GsvSync sync; uint32_t event_slot; ViddecDecoder decoder;
} VideoDecoder;
Result gsvSyncDisarmEvent(GsvSync*,uint32_t);
Result viddecDecoderStop(ViddecDecoder*);
void viddecDecoderClose(ViddecDecoder*);
typedef struct {
 uint32_t width,height; int64_t timestamp_ns; bool end_of_stream;
 uint32_t tiling; const uint8_t* planes[2]; uint32_t pitches[2]; uint32_t slot;
} VideoFrame;
Result videoDecoderCreate(VideoDecoder*,const VideoDecoderConfig*);
Result videoDecoderSubmit(VideoDecoder*,const void*,size_t,int64_t,bool,uint64_t);
Result videoDecoderGetFrame(VideoDecoder*,VideoFrame*,uint64_t);
Result videoDecoderCopyFrame(VideoDecoder*,const VideoFrame*,uint8_t*,size_t,uint8_t*,size_t);
void videoDecoderReleaseFrame(VideoDecoder*,VideoFrame*);
void videoDecoderClose(VideoDecoder*);
""",
        )
        write("libavutil/buffer.h", "#include <libavcodec/avcodec.h>\n")
        write("libavutil/frame.h", "#include <libavcodec/avcodec.h>\n")
        write(
            "libavcodec/avcodec.h",
            r"""
#pragma once
#include <stdint.h>
enum { AV_PIX_FMT_NV12=23, AVCOL_RANGE_MPEG=1, AVCOL_SPC_BT709=1, AVCOL_SPC_SMPTE170M=6 };
#include <stddef.h>
#define AV_BUFFER_FLAG_READONLY 1
typedef struct AVBufferRef { uint8_t* data; int refs; void (*freefn)(void*,uint8_t*); } AVBufferRef;
AVBufferRef* av_buffer_create(uint8_t*,size_t,void (*)(void*,uint8_t*),void*,int);
void av_buffer_unref(AVBufferRef**);
AVBufferRef* av_buffer_ref(const AVBufferRef*);
typedef struct AVFrame {
 int format,width,height,color_range,colorspace; int64_t pts;
 uint8_t* data[8]; int linesize[8]; AVBufferRef* opaque_ref;
} AVFrame;
AVFrame* av_frame_alloc(void);
void av_frame_free(AVFrame**);
int av_frame_get_buffer(AVFrame*,int);
""",
        )
        write(
            "Settings.hpp",
            r"""
#pragma once
class Settings { public:
 bool hw=true;
 static Settings& instance() { static Settings s; return s; }
 bool use_hw_decoding() const { return hw; }
};
""",
        )
        write(
            "borealis.hpp",
            r"""
#pragma once
namespace brls { struct Logger {
 template<class... T> static void info(const char*,T...) {}
 template<class... T> static void error(const char*,T...) {}
 template<class... T> static void warning(const char*,T...) {}
}; }
""",
        )
        write(
            "FFmpegVideoDecoder.hpp",
            r"""
#pragma once
#include "IFFmpegVideoDecoder.hpp"
inline int softwareSetups=0,softwareSubmits=0,softwareCleanups=0;
class FFmpegVideoDecoder : public IFFmpegVideoDecoder { public:
 int setup(int,int,int,int,void*,int) override { ++softwareSetups; return 0; }
 void cleanup() override { ++softwareCleanups; }
 int submit_decode_unit(PDECODE_UNIT) override { ++softwareSubmits; return DR_OK; }
 int capabilities() const override { return 0; }
 VideoDecodeStats* video_decode_stats() override { static VideoDecodeStats s{}; return &s; }
};
""",
        )
        write(
            "AVFrameHolder.hpp",
            r"""
#pragma once
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
}
class AVFrameHolder { public:
 std::vector<AVFrame*> queued; bool transferred=false; int fps=0;
 static AVFrameHolder& instance() { static AVFrameHolder h; return h; }
 void prepare(int rate,bool transfer,size_t=0) { fps=rate;transferred=transfer; }
 AVFrame* acquireWriteFrame() { return av_frame_alloc(); }
 void recycleWriteFrame(AVFrame*& f) { av_frame_free(&f); }
 void pushTransferred(AVFrame* f) {
 if(queued.size()>=4) { auto* old=queued.front();queued.erase(queued.begin());av_frame_free(&old); }
 queued.push_back(f);
 }
 void cleanup() { for(auto f:queued) av_frame_free(&f);queued.clear(); }
};
""",
        )
        write(
            "test.cpp",
            r"""
#include "FFmpegVideoDecoder.hpp"
#include "Switch2VideoDecoder.hpp"
#include "Switch2ImageInterop.hpp"
#include "Switch2FrameReferences.hpp"
#include <borealis/core/frame_resources.hpp>
#include <map>
#include "AVFrameHolder.hpp"
#include "Settings.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <vector>

static uint64_t nowMs=100000;
static uint64_t acquireDelayMs=0;
extern "C" uint64_t LiGetMillis(void) { return nowMs; }
extern "C" uint64_t LiGetMicroseconds(void) { return nowMs * 1000; }
static int creates=0,closes=0,submits=0,copies=0,releases=0,liveFrames=0;
static int activeSessions=0,sessionCloses=0;
static Result createError=0,submitError=0,copyError=0,getError=0;
static bool allocationError=false;
static bool rejectFastRegistration=false,forceCopy=false;
static VideoDecoderConfig captured{};
static std::vector<uint8_t> capturedConfig,lastSample;
void switch2StartImageInterop() {}
void switch2StopImageInterop() {}
void switch2WakeVideoFrame() {}
bool switch2PreimportImage(void*,uint64_t) { return false; }
Switch2ImportedImage switch2FindImportedImage(void*) { return {}; }
void switch2UnimportImage(void*) {}
static std::deque<VideoFrame> pending;
static uint8_t yPlane[32*16],uvPlane[32*8];
extern "C" {
AVBufferRef* av_buffer_create(uint8_t* data,size_t,void (*fn)(void*,uint8_t*),void*,int) {
 if(allocationError)return nullptr;
 return new AVBufferRef{data,1,fn};
}
AVBufferRef* av_buffer_ref(const AVBufferRef* ref) {auto* r=const_cast<AVBufferRef*>(ref);++r->refs;return r;}
void av_buffer_unref(AVBufferRef** ref) { if(*ref && --(*ref)->refs==0) {(*ref)->freefn(nullptr,(*ref)->data);delete *ref;}*ref=nullptr; }
AVFrame* av_frame_alloc(void) { ++liveFrames;return new AVFrame{}; }
void av_frame_free(AVFrame** f) {
 if(!*f)return;
 av_buffer_unref(&(*f)->opaque_ref);std::free((*f)->data[0]);std::free((*f)->data[1]);delete *f;*f=nullptr;--liveFrames;
}
int av_frame_get_buffer(AVFrame* f,int) {
 if(allocationError)return -1;
 f->linesize[0]=f->linesize[1]=32;
 f->data[0]=static_cast<uint8_t*>(std::calloc(f->height,32));
 f->data[1]=static_cast<uint8_t*>(std::calloc(f->height/2,32));return 0;
}
Result videoDecoderCreate(VideoDecoder* d,const VideoDecoderConfig* c) {
 ++creates;captured=*c;capturedConfig.clear();
 if(c->codec_config_size) {
  auto b=static_cast<const uint8_t*>(c->codec_config);
  capturedConfig.assign(b,b+c->codec_config_size);
 }
 if(rejectFastRegistration && (c->frame_rate>60 || uint64_t(c->width)*c->height*c->frame_rate>VIDEO_DECODER_PIXEL_RATE))return MAKERESULT(111,3025);
 if(createError)return createError;
 assert(activeSessions==0);++activeSessions;
 d->initialized=1;d->started=d->decoder_open=true;d->decoder.open=1;return 0;
}
Result videoDecoderSubmit(VideoDecoder* d,const void* bytes,size_t size,int64_t stamp,bool,uint64_t timeout) {
 assert(d->initialized);assert(timeout>0 && timeout<=50000000);++submits;
 auto b=static_cast<const uint8_t*>(bytes);lastSample.assign(b,b+size);
 if(submitError)return submitError;
 for(unsigned row=0;row<16;++row)std::memset(yPlane+row*32,10+row,32);
 std::memset(uvPlane,128,sizeof uvPlane);
 VideoFrame f{};f.width=f.height=16;f.timestamp_ns=stamp;f.planes[0]=yPlane;f.planes[1]=uvPlane;
 f.pitches[0]=f.pitches[1]=32;if(forceCopy)f.planes[0]=f.planes[1]=nullptr;pending.push_back(f);return 0;
}
Result videoDecoderGetFrame(VideoDecoder*,VideoFrame* f,uint64_t) {
 if(getError)return getError;
 if(pending.empty())return KERNELRESULT(TimedOut);
 *f=pending.front();pending.pop_front();nowMs+=acquireDelayMs;return 0;
}
Result videoDecoderCopyFrame(VideoDecoder*,const VideoFrame* f,uint8_t* y,size_t yp,uint8_t* uv,size_t uvp) {
 ++copies;if(copyError)return copyError;
 for(unsigned row=0;row<f->height;++row)std::memcpy(y+row*yp,f->planes[0]+row*f->pitches[0],f->width);
 for(unsigned row=0;row<f->height/2;++row)std::memcpy(uv+row*uvp,f->planes[1]+row*f->pitches[1],f->width);
 return 0;
}
void videoDecoderReleaseFrame(VideoDecoder*,VideoFrame* f) {
 ++releases;std::memset(yPlane,0xee,sizeof yPlane);std::memset(uvPlane,0xee,sizeof uvPlane);
 *f={}; // Ensure the backend does not rely on a released SDK frame.
}
Result gsvSyncDisarmEvent(GsvSync*,uint32_t) {return 0;}
Result viddecDecoderStop(ViddecDecoder*) {return 0;}
void viddecDecoderClose(ViddecDecoder* d) { assert(d->open);d->open=0;--activeSessions;++sessionCloses; }
void videoDecoderClose(VideoDecoder* d) { assert(d->initialized);if(d->started)viddecDecoderStop(&d->decoder);if(d->decoder_open)viddecDecoderClose(&d->decoder);++closes;for(auto& image:d->images)if(image.backing)switch2UnimportImage(image.backing);d->initialized=0;pending.clear(); }
}

static char sps[]={0,0,1,0x67,0x42},pps[]={0,0,1,0x68,0x11},pic[]={0,0,1,0x65,0x22};
static LENTRY picture{nullptr,pic,sizeof pic,BUFFER_TYPE_PICDATA};
static LENTRY parameters2{&picture,pps,sizeof pps,BUFFER_TYPE_PPS};
static LENTRY parameters1{&parameters2,sps,sizeof sps,BUFFER_TYPE_SPS};
static DECODE_UNIT unit() {
 DECODE_UNIT u{};u.frameNumber=1;u.frameType=FRAME_TYPE_IDR;u.fullLength=sizeof sps+sizeof pps+sizeof pic;
 u.bufferList=&parameters1;u.receiveTimeUs=nowMs*1000;u.presentationTimeUs=1000;return u;
}
static void reset() {
 acquireDelayMs=0;
 assert(liveFrames==0 && activeSessions==0);creates=closes=submits=copies=releases=sessionCloses=0;
 createError=submitError=copyError=getError=0;allocationError=false;rejectFastRegistration=false;forceCopy=false;pending.clear();
 Settings::instance().hw=true;softwareSetups=softwareSubmits=softwareCleanups=0;
}

int main() {
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,120,nullptr,0)==0);
  auto u=unit();auto p=u;p.frameType=FRAME_TYPE_PFRAME;
  assert(d.submit_decode_unit(&p)==DR_NEED_IDR && creates==0);
  assert(d.submit_decode_unit(&u)==DR_OK);
  assert(captured.codec==ViddecCodec_H264 && captured.frame_rate==60 && captured.color_policy==3);
  assert(capturedConfig.size()==12 && capturedConfig[3]==1 && capturedConfig[9]==1);
  assert(lastSample.size()==15 && std::memcmp(lastSample.data(),sps,sizeof sps)==0);
  auto& q=AVFrameHolder::instance();assert(q.transferred && q.fps==120 && q.queued.size()==1);
  assert(q.queued[0]->format==kSwitch2NativeFrame && q.queued[0]->pts==1000000);
  auto* lease=switch2NativeFrame(q.queued[0]);assert(lease);
  assert(lease->frame.planes[0][0]==10 && lease->frame.planes[0][15*32+15]==25);
  assert(lease->frame.planes[1][0]==128 && releases==0 && copies==0); // Slot remains owned by the queued frame.
  nowMs+=1000;assert(d.video_decode_stats()->total_decoded_frames==1);
  d.cleanup();d.cleanup();assert(closes==1 && liveFrames==0);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,60,nullptr,0)==0);
  acquireDelayMs=2;auto u=unit();assert(d.submit_decode_unit(&u)==DR_OK);
  nowMs+=1000;const auto* stats=d.video_decode_stats();
  assert(stats->total_decode_time==2000 && stats->current_decode_time==2000);
  assert(stats->current_decoding_time==2 && stats->session_decoding_time==2);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,60,nullptr,0)==0);
  auto u=unit();u.fullLength++;assert(d.submit_decode_unit(&u)==DR_NEED_IDR && creates==0);
  u=unit();u.fullLength=1;assert(d.submit_decode_unit(&u)==DR_NEED_IDR && creates==0);
  u=unit();u.bufferList=&picture;u.fullLength=sizeof pic;
  assert(d.submit_decode_unit(&u)==DR_NEED_IDR && creates==0);
  u=unit();createError=0x99;assert(d.submit_decode_unit(&u)==DR_NEED_IDR && closes==0 && softwareSetups==0);
  assert(d.submit_decode_unit(&u)==DR_NEED_IDR && creates==1); // Rate-limit failed creations.
  nowMs+=1001;createError=0;assert(d.submit_decode_unit(&u)==DR_OK && creates==2);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,120,nullptr,0)==0);
  rejectFastRegistration=true;auto u=unit();assert(d.submit_decode_unit(&u)==DR_OK);
  assert(creates==1 && captured.frame_rate==60 && AVFrameHolder::instance().fps==120);
  for(int i=2;i<=120;++i) {u.frameNumber=i;u.presentationTimeUs+=8333;assert(d.submit_decode_unit(&u)==DR_OK);}
  nowMs+=1000;assert(d.video_decode_stats()->total_decoded_frames==120);
  assert(submits==120 && copies==0 && releases>=115); // Registration fallback must not drop half the stream.
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,120,nullptr,0)==0);
  auto u=unit();submitError=KERNELRESULT(TimedOut);
  assert(d.submit_decode_unit(&u)==DR_NEED_IDR && submits==1 && closes==1);
  u.frameType=FRAME_TYPE_PFRAME;assert(d.submit_decode_unit(&u)==DR_NEED_IDR && submits==1);
  submitError=0;u=unit();assert(d.submit_decode_unit(&u)==DR_OK && creates==2 && submits==2);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,60,nullptr,0)==0);
  forceCopy=true;copyError=0x88;auto u=unit();assert(d.submit_decode_unit(&u)==DR_NEED_IDR);
  assert(releases==1 && closes==1 && liveFrames==0 && AVFrameHolder::instance().queued.empty());
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,60,nullptr,0)==0);
  allocationError=true;auto u=unit();assert(d.submit_decode_unit(&u)==DR_NEED_IDR);
  assert(releases==1 && copies==0 && liveFrames==0);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H265,16,16,120,nullptr,0)==0);
  auto u=unit();assert(d.submit_decode_unit(&u)==DR_OK);
  assert(creates==0 && softwareSetups==1 && softwareSubmits==1);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_AV1_MAIN8,16,16,120,nullptr,0)==0);
  auto u=unit();u.bufferList=&picture;u.fullLength=sizeof pic;
  assert(d.submit_decode_unit(&u)==DR_OK && captured.codec==ViddecCodec_Av1 && capturedConfig.empty());
 }
 reset();
 {
  Settings::instance().hw=false;Switch2VideoDecoder d;
  assert(d.setup(VIDEO_FORMAT_H264,16,16,60,nullptr,0)==0 && softwareSetups==1);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,16,16,60,nullptr,0)==0);
  d.start();d.stop();d.start();d.stop();assert(creates==0);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,3840,2160,60,nullptr,0)==0);
  rejectFastRegistration=true;auto u=unit();assert(d.submit_decode_unit(&u)==DR_OK);
  assert(creates==1 && captured.frame_rate==30 && AVFrameHolder::instance().fps==60);
 }
 reset();
 {
  Switch2VideoDecoder d;assert(d.setup(VIDEO_FORMAT_H264,3840,2160,60,nullptr,0)==0);
  auto u=unit();assert(d.submit_decode_unit(&u)==DR_OK);
  assert(activeSessions==1 && AVFrameHolder::instance().queued.size()==1);
  submitError=KERNELRESULT(TimedOut);u.frameNumber=2;u.frameType=FRAME_TYPE_PFRAME;
  assert(d.submit_decode_unit(&u)==DR_NEED_IDR);
  assert(activeSessions==0 && sessionCloses==1 && closes==0); // Output frame still retains its pool.
  submitError=0;u.frameNumber=3;u.frameType=FRAME_TYPE_IDR;
  assert(d.submit_decode_unit(&u)==DR_OK && activeSessions==1 && creates==2);
  assert(captured.frame_rate==30); // Recreate fits the budget despite the retained old frame.
 }
 {
  auto pool=std::make_shared<Switch2DecoderPool>();pool->open=true;pool->decoder.initialized=1;
  VideoFrame source{};source.width=source.height=16;
  AVFrame* f=av_frame_alloc();assert(switch2AttachNativeFrame(f,pool,source));
  const int beforeRelease=releases,beforeClose=closes;
  brls::FrameResources flightA,flightB;
  flightA.retain(switch2FrameReference(f->opaque_ref));flightB.retain(switch2FrameReference(f->opaque_ref));
  av_frame_free(&f);pool->releaseFrames();assert(releases==beforeRelease && closes==beforeClose);
  flightA.release();pool->releaseFrames();assert(releases==beforeRelease);
  flightB.release();pool->releaseFrames();assert(releases==beforeRelease+1);
  pool.reset();assert(closes==beforeClose+1);
 }
 assert(liveFrames==0);
}
""",
        )
        exe = tmp / "decoder-test.exe"
        subprocess.run(
            [
                cxx,
                "-std=c++17",
                "-pthread",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-D__SWITCH2__",
                "-I",
                str(tmp),
                "-I",
                str(ROOT / "app/src/streaming/switch"),
                "-I",
                str(ROOT / "app/src/streaming/ffmpeg"),
                "-I",
                str(ROOT / "app/src/streaming/video/Vulkan"),
                "-I",
                str(ROOT / "extern/borealis/library/include"),
                "-I",
                str(ROOT / "extern/moonlight-common-c/src"),
                str(ROOT / "app/src/streaming/switch/Switch2VideoDecoder.cpp"),
                str(tmp / "test.cpp"),
                "-o",
                str(exe),
            ],
            check=True,
        )
        subprocess.run([str(exe)], check=True, timeout=20)
    print(
        "Native decoder host mocks: keyframes, 120 FPS, leased NV12, submission ownership, bounded queue, errors, recovery and stop passed"
    )


if __name__ == "__main__":
    main()
