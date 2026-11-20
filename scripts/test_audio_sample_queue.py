from pathlib import Path
import argparse,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
code='''
#include <cassert>
#include <cstdint>
#include "AudioSampleQueue.hpp"
int main(){
 AudioSampleQueue q;q.reset();q.accepted(240);
 assert(q.milliseconds(48000)==5);
 q.observe(120,240);assert(q.milliseconds(48000)==3); // partial playing buffer
 q.observe(120,240);assert(q.milliseconds(48000)==3); // repeated driver snapshot
 q.accepted(240);q.observe(240,480);assert(q.milliseconds(48000)==5);
 q.observe(480,0);assert(q.milliseconds(48000)==0);
 q.reset(UINT32_MAX-119);q.accepted(240);q.observe(120,0);
 assert(q.milliseconds(48000)==0&&q.resyncs()==0); // normal 32-bit wrap
 q.reset(1000);q.accepted(240);q.observe(5,240);
 assert(q.resyncs()==1&&q.milliseconds(48000)==5); // driver restarted counter
 q.observe(125,240);assert(q.milliseconds(48000)==3);
 q.reset();uint32_t raw=0;
 for(int i=0;i<20000;++i){q.accepted(240);raw+=240;q.observe(raw,0);assert(q.milliseconds(48000)==0);}
 q.observe(raw+1000,0);assert(q.milliseconds(48000)==0); // no negative/unsigned backlog
}
'''
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([a.cxx,'-std=c++17','-I'+str(r/'app/src/streaming/audio'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: partial playback, accepted samples, repeated snapshots, 32-bit wrap, driver reset and long-run accounting')
