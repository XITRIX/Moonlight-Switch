from pathlib import Path
import subprocess,tempfile,argparse
from test_switch_network import method
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
s=(r/'app/src/streaming/ffmpeg/FFmpegVideoDecoderDekoHelpers.cpp').read_text()
code=r'''
#include <atomic>
#include <cassert>
struct AVNVTegraChannel {unsigned clock;};
struct AVNVTegraDeviceContext {AVNVTegraChannel nvdec_channel, other;};
struct AVHWDeviceContext {void* hwctx;};
struct Settings {bool maximum=false;static Settings& instance(){static Settings s;return s;}bool nvdec_max_performance(){return maximum;}};
namespace brls {struct Logger {template<class... T>static void info(T...){}};}
static std::atomic<bool> nvdecMaximum{false};
static int initResult=0,updates=0;
extern "C" int __real_av_nvtegra_dfs_init(AVHWDeviceContext*,AVNVTegraChannel*c,int,int,double){c->clock=460800000;return initResult;}
extern "C" int __real_av_nvtegra_dfs_update(AVHWDeviceContext*,AVNVTegraChannel*,int,int){++updates;return -9;}
'''
code+='\n'.join(method(s,sign) for sign in ['extern "C" int __wrap_av_nvtegra_dfs_init(', 'extern "C" int __wrap_av_nvtegra_dfs_update('])
code+=r'''
int main(){
 AVNVTegraDeviceContext d{};AVHWDeviceContext c{&d};auto*n=&d.nvdec_channel;
 __wrap_av_nvtegra_dfs_init(&c,n,1280,720,60);
 assert(__wrap_av_nvtegra_dfs_update(&c,n,1,1)==-9&&updates==1);
 Settings::instance().maximum=true;
 __wrap_av_nvtegra_dfs_init(&c,n,1280,720,60);
 Settings::instance().maximum=false; // next-stream setting must not change current stream
 assert(__wrap_av_nvtegra_dfs_update(&c,n,1,1)==0&&updates==1);
 assert(__wrap_av_nvtegra_dfs_update(&c,&d.other,1,1)==-9&&updates==2);
 __wrap_av_nvtegra_dfs_init(&c,n,1280,720,60);
 assert(__wrap_av_nvtegra_dfs_update(&c,n,1,1)==-9&&updates==3);
 Settings::instance().maximum=true;initResult=-3;
 assert(__wrap_av_nvtegra_dfs_init(&c,n,1280,720,60)==-3);
 assert(__wrap_av_nvtegra_dfs_update(&c,n,1,1)==-9&&updates==4);
}
'''
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([a.cxx,'-std=c++17',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: dynamic/maximum modes, session latch, other engines, failed initialization')
