#!/usr/bin/env python3
"""Run production lifecycle, input, and decoder methods with host fixtures.

Extract methods to avoid linking UI, network, and console SDKs. Class
declarations come from the real headers; external services are stubbed.
This tests ordering/lifetime behavior, not console sleep services.
"""

import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def method(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


FIXTURE = r"""
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
static const auto mainThread = std::this_thread::get_id();
static std::vector<std::string> events;
static int starts=0, draws=0;
namespace brls {
struct Logger { template<class... T> static void info(T...) {} };
static std::mutex queueMutex;
static std::vector<std::function<void()>> queue;
void sync(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(queueMutex);queue.push_back(fn);
}
void drain() {
    std::vector<std::function<void()>> pending;
    {std::lock_guard<std::mutex> lock(queueMutex);pending.swap(queue);}
    for(auto& fn:pending)fn();
}
}
struct VideoDecodeStats {};
struct VideoRenderStats {};
struct NVGcontext {};
struct AVFrame {};
struct STREAM_CONFIGURATION {};
struct CONNECTION_LISTENER_CALLBACKS {};
struct DECODER_RENDERER_CALLBACKS {};
struct AUDIO_RENDERER_CALLBACKS {};
struct OPUS_MULTISTREAM_CONFIGURATION {};
using POPUS_MULTISTREAM_CONFIGURATION=OPUS_MULTISTREAM_CONFIGURATION*;
using PDECODE_UNIT=void*;
template<class T> struct GSResult {
    bool ok; std::string message;
    static GSResult success(T){return {true,""};}
    static GSResult failure(std::string m){return {false,m};}
    bool isSuccess()const{return ok;}
    std::string error()const{return message;}
};
template<class T> using ServerCallback=const std::function<void(GSResult<T>)>;
struct IFFmpegVideoDecoder {
    virtual ~IFFmpegVideoDecoder()=default;
    VideoDecodeStats stats;
    VideoDecodeStats* video_decode_stats(){return &stats;}
};
struct IVideoRenderer {
    virtual ~IVideoRenderer()=default;
    VideoRenderStats stats;
    void invalidateHardwareResources(){
        assert(std::this_thread::get_id()==mainThread);events.push_back("invalidate");
    }
    void draw(NVGcontext*,int,int,AVFrame*,int){++draws;}
    VideoRenderStats* video_render_stats(){return &stats;}
};
struct IAudioRenderer {virtual ~IAudioRenderer()=default;};
struct MoonlightSessionDecoderAndRenderProvider {
    IFFmpegVideoDecoder* video_decoder(){return new IFFmpegVideoDecoder;}
    IVideoRenderer* video_renderer(){return new IVideoRenderer;}
    IAudioRenderer* audio_renderer(){return new IAudioRenderer;}
};
struct GameStreamClient {
    static GameStreamClient& instance(){static GameStreamClient c;return c;}
    template<class F> void quit(const std::string&,F){}
};
struct MoonlightInputManager {
    static MoonlightInputManager& instance(){static MoonlightInputManager i;return i;}
    void resetControllerState(){}
};
struct AVFrameHolder {
    static AVFrameHolder& instance(){static AVFrameHolder h;return h;}
    template<class F> void get(F fn,bool* consumed){
        AVFrame f;*consumed=true;fn(&f);
    }
};
struct Switch2VulkanContext {
    static Switch2VulkanContext* current(){static Switch2VulkanContext c;return &c;}
    void videoFrameChanged(bool consumed){assert(consumed);}
};
void LiStopConnection(){
    assert(std::this_thread::get_id()==mainThread);events.push_back("stop");
}
uint64_t LiGetMillis(){return 1000;}
#define ML_ERROR_NO_VIDEO_TRAFFIC -100
#define ML_ERROR_NO_VIDEO_FRAME -101
#define ML_ERROR_UNEXPECTED_EARLY_TERMINATION -102
#define ML_ERROR_PROTECTED_CONTENT -103
#define ML_ERROR_FRAME_CONVERSION -104
static bool startFails=false;
"""

SCENARIOS = r"""
void MoonlightSession::start(ServerCallback<bool> cb,bool sunshine) {
    ++starts;events.push_back("start");m_is_sunshine=sunshine;
    if(startFails){cb(GSResult<bool>::failure("refused"));return;}
    connection_started();cb(GSResult<bool>::success(true));
}
int main() {
    MoonlightSessionDecoderAndRenderProvider provider;
    MoonlightSession::set_provider(&provider);
    auto* old=new MoonlightSession("host",1);
    std::thread([]{MoonlightSession::connection_terminated(-1);}).join();
    assert(events.empty()); // worker only posts work
    delete old;
    auto* s=new MoonlightSession("host",1);
    MoonlightSession::connection_started();
    brls::drain();assert(events.empty()); // stale termination cannot reach s

    std::thread([]{
        MoonlightSession::requestRestart();MoonlightSession::requestRestart();
    }).join();
    brls::drain();
    assert((events==std::vector<std::string>{"invalidate","stop","start"}));
    assert(starts==1); // duplicate queued work belongs to the old connection

    events.clear();s->set_suspended(true);
    std::thread([]{MoonlightSession::connection_terminated(-1);}).join();
    brls::drain();s->draw(nullptr,1280,720);
    assert(events.empty()&&draws==0); // no GPU or teardown while asleep
    s->set_suspended(false);s->draw(nullptr,1280,720);
    assert((events==std::vector<std::string>{"invalidate","stop","start"}));
    assert(starts==2&&draws==1);

    events.clear();s->set_suspended(true);s->set_suspended(false);
    s->draw(nullptr,1280,720);s->draw(nullptr,1280,720);
    assert((events==std::vector<std::string>{"invalidate"}));
    assert(draws==3); // resume without a drop invalidates once, without reconnect

    events.clear();MoonlightSession::requestRestart();s->stop(false);
    brls::drain();
    assert((events==std::vector<std::string>{"invalidate","stop"}));
    assert(starts==2);s->stop(false);assert(events.size()==2);
    s->draw(nullptr,1280,720);assert(draws==3);
    delete s;

    old=new MoonlightSession("host",1);
    MoonlightSession::requestRestart();delete old;
    s=new MoonlightSession("host",1);events.clear();brls::drain();
    assert(events.empty()); // stale decoder reconnect cannot reach a new session

    MoonlightSession::connection_started();
    std::thread([]{
        MoonlightSession::connection_terminated(ML_ERROR_NO_VIDEO_TRAFFIC);
    }).join();
    brls::drain();
    assert(events.empty()&&starts==2); // no video at all is reported, not retried
    assert(s->is_terminated()&&s->termination_error()==ML_ERROR_NO_VIDEO_TRAFFIC);
    delete s;

    s=new MoonlightSession("host",1);
    MoonlightSession::connection_started();
    std::thread([]{
        MoonlightSession::connection_terminated(ML_ERROR_PROTECTED_CONTENT);
    }).join();
    brls::drain();
    assert(starts==2&&s->termination_error()==ML_ERROR_PROTECTED_CONTENT);
    delete s;

    s=new MoonlightSession("host",1);
    MoonlightSession::connection_started();startFails=true;
    std::thread([]{MoonlightSession::connection_terminated(-1);}).join();
    brls::drain();brls::drain();
    assert(starts==3&&s->is_terminated()); // a cut-off stream retries once
    assert(s->m_reconnect_failed&&s->m_reconnect_error=="refused");
    startFails=false;
    delete s;
}
"""

CONTROLLERS = r"""
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
constexpr int GAMEPADS_MAX=5,LI_CTYPE_UNKNOWN=0;
constexpr int LI_CCAP_RUMBLE=1,LI_CCAP_ACCEL=2,LI_CCAP_GYRO=4;
static int controllerCount=1,arrivals=0,states=0,failArrival=0;
namespace brls {
struct Point {float x=0,y=0;};
struct PanGestureStatus {};
enum ControllerButton {_BUTTON_MAX=32};
enum BrlsKeyboardScancode {};
struct ControllerState {};
struct Logger {
    template<class... T> static void info(T...){}
    template<class... T> static void debug(T...){}
    template<class... T> static void warning(T...){}
};
struct Input {int getControllersConnectedCount(){return controllerCount;}};
struct Platform {Input input;Input* getInputManager(){return &input;}};
struct Application {static Platform* getPlatform(){static Platform p;return &p;}};
}
using namespace brls;
template<class T> class Singleton {};
template<class... T> int LiSendControllerArrivalEvent(T...){
    ++arrivals;return failArrival ? --failArrival,-1 : 0;
}
template<class... T> int LiSendMultiControllerEvent(T...){++states;return 0;}
"""

CONTROLLER_SCENARIOS = r"""
MoonlightInputManager::MoonlightInputManager(){}
GamepadState MoonlightInputManager::getControllerState(int,bool){return {};}
short MoonlightInputManager::controllersToMap(){return (1<<controllerCount)-1;}
int main(){
    MoonlightInputManager input;
    input.handleControllers(false);assert(arrivals==1&&states==1);
    input.handleControllers(false);assert(arrivals==1&&states==1);
    input.resetControllerState();input.handleControllers(false);
    assert(arrivals==2&&states==2); // same count and neutral buttons, new stream
    input.resetControllerState();failArrival=1;input.handleControllers(false);
    assert(arrivals==3);input.handleControllers(false);assert(arrivals==4);
    controllerCount=2;input.handleControllers(false);assert(arrivals==6);
    controllerCount=GAMEPADS_MAX+2;input.handleControllers(false);
    assert(arrivals==6+GAMEPADS_MAX); // no out-of-bounds controller state access
}
"""

TERMINATION = r"""
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
using std::memory_order_acq_rel;
using ThreadEntry=void(*)(void*);
struct PLT_THREAD {std::thread thread;};
static std::atomic<bool> alreadyTerminated{false},ConnectionInterrupted{false};
static PLT_THREAD terminationCallbackThread;
static int terminationCallbackErrorCode,joins=0,detaches=0,failCreate=0;
static std::atomic<int> callbacks{0};
static void (*originalTerminationCallback)(int)=[](int err){assert(err==-7);++callbacks;};
int PltCreateThread(const char*,ThreadEntry fn,void* ctx,PLT_THREAD* thread){
    if(failCreate)return -1;
    thread->thread=std::thread(fn,ctx);return 0;
}
void PltJoinThread(PLT_THREAD* thread){++joins;thread->thread.join();}
void PltDetachThread(PLT_THREAD* thread){++detaches;thread->thread.join();}
void Limelog(const char*,...){}
#define LC_ASSERT(x) ((void)0)
"""

TERMINATION_SCENARIOS = r"""
int main(){
    std::vector<std::thread> workers;
    for(int i=0;i<16;++i)workers.emplace_back([]{ClInternalConnectionTerminated(-7);});
    for(auto& thread:workers)thread.join();
    assert(callbacks==1&&joins==1&&detaches==0);
    alreadyTerminated=false;ConnectionInterrupted=true;
    ClInternalConnectionTerminated(-7);assert(callbacks==1);
    alreadyTerminated=false;ConnectionInterrupted=false;failCreate=1;
    ClInternalConnectionTerminated(-7);assert(joins==1&&detaches==0);
}
"""

FFMPEG = r"""
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
constexpr int DR_OK=0,DR_NEED_IDR=-1,AV_INPUT_BUFFER_PADDING_SIZE=64;
struct AVFrame {};struct AVPacket {};struct AVBufferRef {};struct AVCodec {};
struct AVCodecContext {};
enum AVCodecID {AV_CODEC_ID_NONE};
struct LENTRY {LENTRY* next;char* data;int length;};
using PLENTRY=LENTRY*;
struct DECODE_UNIT {
    int fullLength;PLENTRY bufferList;uint32_t frameNumber;uint64_t receiveTimeUs;
};
using PDECODE_UNIT=DECODE_UNIT*;
static int decodedResult=1,decodeCalls=0,flushes=0,restarts=0;
static uint64_t nowUs=1000000;
uint64_t LiGetMillis(){return nowUs/1000;}
uint64_t LiGetMicroseconds(){return nowUs;}
void avcodec_flush_buffers(AVCodecContext*){++flushes;}
struct MoonlightSession {static void requestRestart(){++restarts;}};
namespace brls {struct Logger {
    template<class... T> static void info(T...){}
    template<class... T> static void error(T...){}
};}
"""

FFMPEG_SCENARIOS = r"""
FFmpegVideoDecoder::FFmpegVideoDecoder(){
    m_decoder_ready=true;m_stream_fps=60;
    static AVCodecContext context;m_decoder_context=&context;
}
FFmpegVideoDecoder::~FFmpegVideoDecoder(){free(m_ffmpeg_buffer);}
int FFmpegVideoDecoder::setup(int,int,int,int,void*,int){return 0;}
void FFmpegVideoDecoder::cleanup(){}
int FFmpegVideoDecoder::capabilities()const{return 0;}
VideoDecodeStats* FFmpegVideoDecoder::video_decode_stats(){return &m_video_decode_stats_cache;}
int FFmpegVideoDecoder::decode(char* data,int length){
    ++decodeCalls;nowUs+=250;
    for(int i=0;i<AV_INPUT_BUFFER_PADDING_SIZE;++i)assert(data[length+i]==0);
    return decodedResult;
}
int main(){
    FFmpegVideoDecoder d;
    assert(d.submit_decode_unit(nullptr)==DR_NEED_IDR);
    std::vector<char> bytes(2*1024*1024,42);
    LENTRY entry{nullptr,bytes.data(),int(bytes.size())};
    DECODE_UNIT unit{entry.length,&entry,1,LiGetMicroseconds()};
    assert(d.submit_decode_unit(&unit)==DR_OK&&decodeCalls==1);
    assert(d.m_ffmpeg_buffer_capacity>=bytes.size());
    assert(std::memcmp(d.m_ffmpeg_buffer,bytes.data(),bytes.size())==0);
    entry.length++;assert(d.submit_decode_unit(&unit)==DR_NEED_IDR&&decodeCalls==1);
    entry.length-=2;assert(d.submit_decode_unit(&unit)==DR_NEED_IDR&&decodeCalls==1);
    entry.length++;decodedResult=-1;
    for(int i=0;i<9;++i){++unit.frameNumber;assert(d.submit_decode_unit(&unit)==DR_NEED_IDR);}
    assert(flushes==9&&restarts==0);++unit.frameNumber;
    assert(d.submit_decode_unit(&unit)==DR_NEED_IDR&&restarts==1);
    for(int i=0;i<5;++i){++unit.frameNumber;d.submit_decode_unit(&unit);}
    assert(restarts==1); // persistent errors queue only one reconnect per setup
    decodedResult=1;++unit.frameNumber;assert(d.submit_decode_unit(&unit)==DR_OK);
    assert(d.m_consecutive_decode_errors==0);
}
"""


def stripped_header(path):
    return re.sub(r"^#(?:include|pragma)[^\n]*\n", "", path.read_text(), flags=re.M)


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for lifecycle tests")
    source = (ROOT / "app/src/streaming/MoonlightSession.cpp").read_text()
    header = stripped_header(ROOT / "app/src/streaming/MoonlightSession.hpp")
    globals_ = source[source.index("int m_video_format;"):source.index("MoonlightSession* MoonlightSession::activeSession()")]
    signatures = (
        "MoonlightSession* MoonlightSession::activeSession()",
        "void MoonlightSession::requestRestart()",
        "void MoonlightSession::set_provider(",
        "MoonlightSession::MoonlightSession(",
        "MoonlightSession::~MoonlightSession()",
        "void MoonlightSession::connection_started()",
        "void MoonlightSession::connection_terminated(",
        "void MoonlightSession::handle_connection_terminated(",
        "void MoonlightSession::stop(",
        "void MoonlightSession::restart()",
        "void MoonlightSession::set_suspended(",
        "void MoonlightSession::draw(",
    )
    harness = FIXTURE + "\n#define private public\n" + header
    harness += "\n#undef private\n" + globals_
    harness += "\n".join(method(source, signature) for signature in signatures)
    harness += SCENARIOS
    input_source = (ROOT / "app/src/streaming/InputManager.cpp").read_text()
    controllers = CONTROLLERS + "\n#define private public\n"
    controllers += stripped_header(ROOT / "app/src/streaming/InputManager.hpp")
    controllers += "\n#undef private\n"
    controllers += method(input_source, "void MoonlightInputManager::handleControllers(")
    controllers += CONTROLLER_SCENARIOS
    library = (ROOT / "extern/moonlight-common-c/src/Connection.c").read_text()
    termination = TERMINATION
    termination += method(library, "static void terminationCallbackThreadFunc(")
    termination += method(library, "static void ClInternalConnectionTerminated(")
    termination += TERMINATION_SCENARIOS
    ffmpeg_source = (ROOT / "app/src/streaming/ffmpeg/FFmpegVideoDecoder.cpp").read_text()
    ffmpeg = FFMPEG + stripped_header(ROOT / "app/src/streaming/ffmpeg/IFFmpegVideoDecoder.hpp")
    ffmpeg += "\n#define private public\n"
    ffmpeg += stripped_header(ROOT / "app/src/streaming/ffmpeg/FFmpegVideoDecoder.hpp")
    ffmpeg += "\n#undef private\n"
    ffmpeg += method(ffmpeg_source, "int FFmpegVideoDecoder::submit_decode_unit(")
    ffmpeg += FFMPEG_SCENARIOS
    with tempfile.TemporaryDirectory(prefix="moonlight-lifecycle-") as directory:
        tmp = Path(directory)
        for name, code in (("session", harness), ("controllers", controllers),
                           ("termination", termination), ("ffmpeg", ffmpeg)):
            path = tmp / (name + ".cpp")
            path.write_text(code)
            for platform in ("switch1", "switch2"):
                binary = tmp / (name + platform)
                subprocess.run(
                    [cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                     "-Wno-unused-parameter", "-pthread",
                     "-D__SWITCH__",
                     *(["-D__SWITCH2__"] if platform == "switch2" else []),
                     str(path), "-o", str(binary)], check=True,
                )
                subprocess.run([str(binary)], check=True)
    print("Session sleep/lifetime, controller announcements, termination threads, and FFmpeg recovery passed")


if __name__ == "__main__":
    main()
