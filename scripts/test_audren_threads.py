"""Run the production audio pump with real host threads and SDK substitutes."""
from test_audren_integration import run_scenarios

SCENARIOS = r'''
template<class Predicate>void eventually(Predicate predicate){
 for(int i=0;i<200;++i){if(predicate())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
 assert(false && "worker did not make progress");
}
void resourcesClosed(){assert(!rendererOpen&&!driverOpen&&opusInstances==0&&liveThreadHandles==0&&liveEntries==0);}
int main(){
 realThreads=true;Settings::instance().reserve=30;Settings::instance().log=true;OpusConfig config;char packet=1;
 {
  AudrenAudioRenderer audio;assert(audio.init(0,&config,nullptr,0)==0);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  {AudioLock lock(&audio.m_audio_mutex);assert(audio.m_pump_calls==0&&audio.m_update_calls==0);}
  // The SDK fixture asserts every playback update runs on the real pump thread.
  for(int i=0;i<12;++i)audio.decode_and_play_sample(&packet,1);
  eventually([&]{AudioLock lock(&audio.m_audio_mutex);return audio.m_accepted_samples>=480;});
  uint64_t before;
  {AudioLock lock(&audio.m_audio_mutex);before=audio.m_update_calls;}
  std::this_thread::sleep_for(std::chrono::milliseconds(35));
  {AudioLock lock(&audio.m_audio_mutex);auto calls=audio.m_update_calls-before;assert(calls>=2&&calls<100);}
  // Slow diagnostic output on the receiver must not stall the pump or hold
  // the PCM lock. Force a fresh snapshot, then block its output for 80 ms.
  {AudioLock lock(&audio.m_audio_mutex);audio.m_audio_trace_ms=0;}
  eventually([&]{AudioLock lock(&audio.m_audio_mutex);return audio.m_trace_ready;});
  slowLogMs=80;
  std::thread diagnosticReceiver([&]{audio.decode_and_play_sample(&packet,1);});
  eventually([&]{return activeLogs.load()>0;});
  {AudioLock lock(&audio.m_audio_mutex);before=audio.m_update_calls;}
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  {AudioLock lock(&audio.m_audio_mutex);assert(audio.m_update_calls>before);}
  slowLogMs=0;diagnosticReceiver.join();
  auto start=std::chrono::steady_clock::now();audio.cleanup();
  assert(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(250));
  audio.cleanup();resourcesClosed();
 }
 // Cleanup disables acceptance, then joins before freeing pools or decoder.
 for(int session=0;session<30;++session){
  AudrenAudioRenderer audio;assert(audio.init(0,&config,nullptr,0)==0);
  std::atomic<bool> begin=false;
  std::thread producer([&]{begin=true;for(int i=0;i<2000;++i)audio.decode_and_play_sample(&packet,1);});
  while(!begin.load())std::this_thread::yield();
  if(session%2)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  audio.cleanup();producer.join();audio.cleanup();resourcesClosed();
 }
 for(int session=0;session<30;++session){AudrenAudioRenderer audio;assert(audio.init(0,&config,nullptr,0)==0);audio.cleanup();resourcesClosed();}
 // Temporary IPC errors retain queued PCM and are retried by the worker.
 {
  failUpdates=3;AudrenAudioRenderer audio;assert(audio.init(0,&config,nullptr,0)==0);
  for(int i=0;i<8;++i)audio.decode_and_play_sample(&packet,1);
  eventually([&]{AudioLock lock(&audio.m_audio_mutex);return audio.m_update_errors==3&&audio.m_accepted_samples>0;});
  {AudioLock lock(&audio.m_audio_mutex);assert(audio.m_discarded_samples==0);}
  audio.cleanup();resourcesClosed();
 }
 std::cout<<"real-thread fixture: idle sleep, periodic playback updates, 60 restarts, concurrent producer/shutdown, retry and ownership passed\n";
}
'''

if __name__ == '__main__':
    run_scenarios(SCENARIOS)
    print('PASS: production pump synchronization and teardown with real host threads')
