"""Compile the production C trace bridge and callback wrapper with host threads."""
from pathlib import Path
import subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'extern/moonlight-common-c/src/AudioStream.c').read_bytes().decode('utf-8')
bridge='\n'.join(line for line in source[source.index('static _Atomic(AudioTraceCallback)'):source.index('static SOCKET')].splitlines() if not line.startswith('#'))
callback=source[source.index('static void traceDecodeCallback'):source.index('static bool queuePacketToLbq')]
C='''#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include "AudioTrace.h"
uint64_t LiGetMicroseconds(void);
int LiGetPendingAudioDuration(void){return 5;}
static struct {void(*decodeAndPlaySample)(char*,int);} AudioCallbacks;
'''+bridge+callback+'''
void fixtureSetDecode(void(*fn)(char*,int)){AudioCallbacks.decodeAndPlaySample=fn;}
void fixtureCallback(uint64_t seq){traceDecodeCallback(NULL,1,seq);}
'''
CPP=r'''
#include <cassert>
#include <atomic>
#include <thread>
#include "AudioTrace.h"
extern "C" void fixtureSetDecode(void(*)(char*,int));
extern "C" void fixtureCallback(uint64_t);
static std::atomic<uint64_t> clockUs=0,events=0,decodes=0;
extern "C" uint64_t LiGetMicroseconds(){return ++clockUs;}
static void observe(int stage,uint64_t time,uint64_t seq,int64_t a,int64_t b,int64_t,int64_t){
 assert(time>0&&LiAudioTraceCurrentSequence()==seq);
 if(stage==AT_CALLBACK_BEGIN)assert(a==1&&b==5);else assert(stage==AT_CALLBACK_END);
 ++events;
}
static void decode(char*,int length){assert(length==1&&LiAudioTraceCurrentSequence()!=UINT64_MAX);++decodes;}
int main(){
 fixtureSetDecode(decode);LiSetAudioTraceCallback(observe);
 std::thread a([]{for(int i=0;i<1000;++i){fixtureCallback(i);assert(LiAudioTraceCurrentSequence()==UINT64_MAX);}});
 std::thread b([]{for(int i=0;i<1000;++i){fixtureCallback(50000+i);assert(LiAudioTraceCurrentSequence()==UINT64_MAX);}});
 a.join();b.join();assert(events==4000&&decodes==2000);
 LiSetAudioTraceCallback(nullptr);auto before=clockUs.load();fixtureCallback(99);
 assert(events==4000&&decodes==2001&&clockUs==before);
}
'''
with tempfile.TemporaryDirectory() as temporary:
    d=Path(temporary); c=d/'bridge.c';cpp=d/'check.cpp';obj=d/'bridge.o';exe=d/'check.exe'
    c.write_text(C);cpp.write_text(CPP)
    include='-I'+str(ROOT/'extern/moonlight-common-c/src')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',include,'-c',str(c),'-o',str(obj)],check=True)
    subprocess.run(['g++','-std=c++20','-pthread',include,str(cpp),str(obj),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
print('PASS: production C bridge atomic hook, callback timing, thread-local sequence and disabled tracing without clock reads')
