from pathlib import Path
import argparse,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
code='''
#include <cassert>
#include "StreamProfile.hpp"
int main(){
 StreamProfile handheld{720,60,10000,false},docked{1080,30,30000,true};
 StreamProfileLatch session;
 session.begin(handheld);
 handheld.bitrate=20000; // editing saved settings cannot change current stream
 assert(session.value.bitrate==10000);
 session.begin(docked); // dock/undock or reconnect must not replace snapshot
 assert(session.value.resolution==720&&session.value.fps==60&&!session.value.maximum);
 session.end();session.begin(docked);
 assert(session.value.resolution==1080&&session.value.fps==30&&session.value.bitrate==30000&&session.value.maximum);
 session.begin(handheld);assert(session.value.resolution==1080);
 session.end();session.begin(handheld);assert(session.value.bitrate==20000);
}
'''
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([a.cxx,'-std=c++17','-I'+str(r/'app/src/utils'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: independent profiles, immutable session snapshot, reconnect/dock retention, next-session selection')
