"""Exercise the production PCM seam repair without console dependencies."""
from pathlib import Path
import subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
CODE = r'''
#include <cassert>
#include <vector>
#include <cstring>
#include "AudioSeam.hpp"
int main(){
 for(int channels : {1,2,6,8})for(int frames : {1,48,240,480,960}){
  AudioSeam seam;
  std::vector<int16_t> pcm(frames*channels+2, 30000);
  pcm.front()=1234;pcm.back()=5678;
  assert(seam.apply(pcm.data()+1,frames,channels,48000));
  assert(pcm.front()==1234&&pcm.back()==5678);
  for(int n=1;n<=frames*channels;++n)assert(pcm[n]>=0&&pcm[n]<=30000);
  std::fill(pcm.begin()+1,pcm.end()-1,30000);
  auto identical=pcm;
  assert(!seam.apply(pcm.data()+1,frames,channels,48000));
  assert(pcm==identical); // continuous audio is bit-exact
  seam.markGap();
  std::fill(pcm.begin()+1,pcm.end()-1,-30000);
  assert(seam.apply(pcm.data()+1,frames,channels,48000));
  if(frames>=96){assert(pcm[1]>28000);assert(pcm[1+95*channels]==-30000);}
  assert(pcm.front()==1234&&pcm.back()==5678);
  seam.reset();
  std::fill(pcm.begin()+1,pcm.end()-1,-32768);
  assert(seam.apply(pcm.data()+1,frames,channels,48000));
  for(int n=1;n<=frames*channels;++n)assert(pcm[n]<=0);
  assert(pcm[frames*channels]==-32768);
 }
 AudioSeam seam;int16_t sample=111;
 assert(!seam.apply(nullptr,1,2,48000));
 assert(!seam.apply(&sample,0,2,48000));
 assert(!seam.apply(&sample,1,9,48000));assert(sample==111);
}
'''
with tempfile.TemporaryDirectory() as temporary:
    source=Path(temporary)/'seam.cpp'; executable=Path(temporary)/'seam.exe'
    source.write_text(CODE)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I'+str(ROOT/'app/src/streaming/audio'),str(source),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print('PASS: seam ramp, channel isolation, bit-exact contiguous PCM, reset, clipping boundaries and memory guards')
