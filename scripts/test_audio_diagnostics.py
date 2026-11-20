"""Exercise the actual asynchronous recorder with concurrent host producers."""
from pathlib import Path
import subprocess, tempfile
from test_audren_integration import FIXTURE, ROOT, strip

SCENARIOS=r'''
#include <fstream>
#include <sstream>
int main(){
 realThreads=true;
 auto& recorder=AudioDiagnostics::instance();
 assert(traceHook.load()==nullptr);
 assert(recorder.start("./"));
 std::vector<std::thread> producers;
 for(int producer=0;producer<3;++producer)producers.emplace_back([producer]{
  for(int i=0;i<5000;++i)LiAudioTraceEvent(AT_PCM_QUEUE,producer*5000+i,i,producer,123,456);
 });
 for(auto& t:producers)t.join();
 recorder.stop();recorder.stop();
 assert(traceHook.load()==nullptr&&liveThreadHandles==0&&liveEntries==0);
 std::ifstream input("moonlight-LL26-audio-log.txt");std::string line;uint64_t rows=0,recorded=0,lost=0;
 while(std::getline(input,line)){
  if(line.starts_with("# session_end")){std::sscanf(line.c_str(),"# session_end recorded=%llu lost_diagnostic_records=%llu",&recorded,&lost);}
  if(line.empty()||line[0]=='#')continue;
  std::replace(line.begin(),line.end(),',',' ');std::istringstream stream(line);
  uint64_t time,seq;int stage;int64_t a,b,c,d;
  assert(stream>>time>>stage>>seq>>a>>b>>c>>d);assert(stage==AT_PCM_QUEUE&&seq==b*5000+a&&c==123&&d==456);++rows;
 }
 assert(rows==recorded&&recorded+lost==15000);
 input.close();
 assert(recorder.start("./"));LiAudioTraceEvent(AT_CONFIG,99,48000,2,240,30);recorder.stop();
 for(int stage:{5,6}){failStage=stage;assert(!recorder.start("./"));assert(traceHook.load()==nullptr&&liveThreadHandles==0&&liveEntries==0);}
 failStage=0;assert(!recorder.start("missing/directory/"));recorder.stop();
 std::cout<<"recorder concurrent records="<<recorded<<" dropped_diagnostics="<<lost<<"; append, errors and cleanup passed\n";
}
'''

def main():
    fixture='\n'.join(line for line in FIXTURE.splitlines() if not line.startswith('struct AudioDiagnostics '))
    header=strip((ROOT/'app/src/streaming/audio/AudioDiagnostics.hpp').read_bytes().decode('utf-8'))
    with tempfile.TemporaryDirectory() as directory:
        source=Path(directory)/'recorder.cpp'; executable=Path(directory)/'recorder.exe'
        source.write_text(fixture+header+SCENARIOS,encoding='utf-8')
        subprocess.run(['g++','-std=c++20','-O2','-pthread','-I'+str(ROOT/'app/src/streaming/audio'),'-I'+str(ROOT/'extern/moonlight-common-c/src'),str(source),'-o',str(executable)],check=True)
        subprocess.run([str(executable)],cwd=directory,check=True,timeout=30)
    print('PASS: production diagnostic recorder, concurrent producers, exact record accounting, append, disabled hook and failure cleanup')

if __name__=='__main__':main()
