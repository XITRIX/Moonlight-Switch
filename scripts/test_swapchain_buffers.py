from pathlib import Path
from test_switch_network import method
import subprocess,tempfile,argparse
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
r=Path(__file__).resolve().parents[1]
s=(r/'app/src/streaming/ffmpeg/FFmpegVideoDecoderDekoHelpers.cpp').read_text()
code='''
#include <cassert>
#include <chrono>
#include <thread>
#include <atomic>
void svcSleepThread(long long ns){std::this_thread::sleep_for(std::chrono::nanoseconds(ns));}
using DkQueue=void*;
struct Settings {bool enabled=true;static Settings& instance(){static Settings s;return s;}bool video_vsync(){return enabled;}};
namespace videoTrace {std::atomic<long long> drawnFrame{-1};}
namespace brls {struct Logger {template<class... T>static void debug(T...){}};}
unsigned long long LiGetMicroseconds(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
using DkSwapchain=void*;
struct DkSwapchainMaker {void* device;void* nativeWindow;const void* pImages;unsigned numImages;};
static DkSwapchainMaker seen;
extern "C" DkSwapchain __real_dkSwapchainCreate(const DkSwapchainMaker*m){seen=*m;return m->device;}
extern "C" int __real_dkQueueAcquireImage(DkQueue,DkSwapchain){return 1;}
'''+method(s,'extern "C" DkSwapchain __wrap_dkSwapchainCreate(')+method(s,'extern "C" int __wrap_dkQueueAcquireImage(')+'''
int main(){
 int d,w,i;DkSwapchainMaker m{&d,&w,&i,3};
 assert(__wrap_dkSwapchainCreate(&m)==&d);
 assert(seen.numImages==2&&m.numImages==3&&seen.device==&d&&seen.nativeWindow==&w&&seen.pImages==&i);
 m.numImages=2;__wrap_dkSwapchainCreate(&m);assert(seen.numImages==2);
 m.numImages=1;__wrap_dkSwapchainCreate(&m);assert(seen.numImages==1);
 Settings::instance().enabled=false;
 auto before=LiGetMicroseconds();
 for(int k=0;k<10;++k)assert(__wrap_dkQueueAcquireImage(nullptr,nullptr)==1);
 assert(LiGetMicroseconds()-before>=140000);
 // A long pause must not cause a catch-up burst when drawing resumes.
 std::this_thread::sleep_for(std::chrono::milliseconds(80));
 before=LiGetMicroseconds();
 for(int k=0;k<4;++k)__wrap_dkQueueAcquireImage(nullptr,nullptr);
 assert(LiGetMicroseconds()-before>=45000);
}
'''
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([a.cxx,str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('PASS: two active presentation buffers, original maker/storage untouched, passthrough')
