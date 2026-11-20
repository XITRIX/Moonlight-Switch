from pathlib import Path
import subprocess, tempfile, argparse
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');args=p.parse_args()
code='''
#include <cassert>
#include "VideoRecovery.hpp"
int main(){
 VideoRecovery r;r.begin(100);
 assert(r.update(5099,false,true)==VideoRecovery::None);
 assert(r.update(5100,false,true)==VideoRecovery::Keyframe);
 assert(r.update(6000,false,true)==VideoRecovery::None);
 assert(r.update(8100,false,true)==VideoRecovery::Reconnect);
 r.begin(9000);
 assert(r.update(14000,false,true)==VideoRecovery::Keyframe);
 assert(r.update(17000,false,true)==VideoRecovery::Reconnect);
 r.begin(20000);r.update(25000,false,true);
 assert(r.update(28000,false,true)==VideoRecovery::None);
 VideoRecovery healthy;healthy.begin(0);
 for(unsigned t=16;t<12000;t+=16)assert(healthy.update(t,true,true)==VideoRecovery::None);
 VideoRecovery desktop;desktop.begin(0);
 assert(desktop.update(10000,false,false)==VideoRecovery::None);
 VideoRecovery recovered;recovered.begin(0);recovered.update(5000,false,true);
 for(unsigned t=5016;t<=8000;t+=16)assert(recovered.update(t,true,false)==VideoRecovery::None);
}
'''
with tempfile.TemporaryDirectory() as t:
 src=Path(t)/'test.cpp';exe=Path(t)/'test.exe';src.write_text(code)
 subprocess.run([args.cxx,'-std=c++17','-I'+str(root/'app/src/streaming'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: stalled progress, keyframe grace, bounded reconnects, recovery and static desktop')
