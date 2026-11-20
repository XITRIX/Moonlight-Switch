from pathlib import Path
import argparse,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
code='''
#include <cassert>
#include "AudioReserve.hpp"
int main(){
 for(int target : {0,10,20,30,40,50,60,70,80,90,100}) {
  AudioReserve reserve;reserve.reset(target);
  assert(!reserve.underrun(0));
  for(int queued=5;queued<=105;queued+=5)
   assert(reserve.ready(queued)==(queued>=target));
  assert(!reserve.underrun(5));
  assert(reserve.underrun(0)==(target>0));
  assert(!reserve.underrun(0));
  assert(reserve.ready(5)==(target<=5));
  assert(reserve.ready(target));
  reserve.reset(0);assert(reserve.ready(5));assert(!reserve.underrun(0));
 }
}
'''
code='#include <initializer_list>\n'+code
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([a.cxx,'-std=c++17','-I'+str(r/'app/src/streaming/audio'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: 0..100 ms startup reserve, no repeated underrun, recovery threshold, disabled mode')
