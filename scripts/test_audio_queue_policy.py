from pathlib import Path
import argparse,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
code='''
#include <cassert>
#include <initializer_list>
#include "AudioQueuePolicy.hpp"
int main(){
 for(int reserve : {0,10,20,30,40,50,60,70,80,90,100}) {
  assert(AudioQueuePolicy::decide(reserve,100,0,true)==AudioQueuePolicy::None);
  assert(AudioQueuePolicy::decide(reserve,0,reserve,true)==AudioQueuePolicy::None);
  assert(AudioQueuePolicy::decide(reserve,0,reserve+30,true)==AudioQueuePolicy::OutputBacklog);
  assert(AudioQueuePolicy::decide(reserve,100,reserve+10,true)==AudioQueuePolicy::InputBacklog);
  assert(AudioQueuePolicy::decide(reserve,0,0,false)==AudioQueuePolicy::NoBuffer);
  assert(AudioQueuePolicy::pitch(reserve,reserve)==1.0f);
  int queued=0;
  for(int packet=0;packet<200;++packet) {
   if(AudioQueuePolicy::decide(reserve,0,queued,true)==AudioQueuePolicy::None) queued+=5;
   assert(queued<=reserve+30);
  }
  for(int q=0;q<200;++q) {
   const float rate=AudioQueuePolicy::pitch(reserve,q);
   assert(rate>=1.0f&&rate<=1.02001f);
   if(q>0)assert(rate>=AudioQueuePolicy::pitch(reserve,q-1));
  }
  float rate=1.0f;
  for(int step=0;step<100;++step){float next=AudioQueuePolicy::slew(rate,1.02f);assert(next>=rate&&next-rate<=0.000501f);rate=next;}
  assert(rate==1.02f);
  for(int step=0;step<100;++step){float next=AudioQueuePolicy::slew(rate,1.0f);assert(next<=rate&&rate-next<=0.000501f);rate=next;}
  assert(rate==1.0f);
 }
 // A 30 ms intentional reserve retains burst headroom that the old 40 ms cap lacked.
 assert(AudioQueuePolicy::decide(30,0,45,true)==AudioQueuePolicy::None);
}
'''
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([a.cxx,'-std=c++17','-I'+str(r/'app/src/streaming/audio'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: starvation protection, intentional reserve, bounded output queue, separate drop reasons, bounded catch-up rate')
