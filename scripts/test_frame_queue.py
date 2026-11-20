#!/usr/bin/env python3
"""Exercise the production queue with lightweight AVFrame lifetime fixtures.

No console/FFmpeg SDK is needed. This verifies queue policy and ownership,
not NVDEC, GPU fences, scanout latency, or Wi-Fi performance.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <iostream>
static std::atomic<int> liveFrames{0};
struct AVFrame { int id = 0; };
extern "C" {
AVFrame* av_frame_alloc() { ++liveFrames; return new AVFrame; }
void av_frame_free(AVFrame** f) { if (*f) { --liveFrames; delete *f; *f=nullptr; } }
void av_frame_unref(AVFrame* f) { f->id=0; }
int av_frame_ref(AVFrame* dst, const AVFrame* src) { dst->id=src->id; return 0; }
}
template<class T> class Singleton {
public: static T& instance() { static T t; return t; }
};
struct Settings : Singleton<Settings> {
    int frames_queue_size() const { return 3; }
    bool low_latency_video() const { return true; }
};
'''
SCENARIOS = r'''
static void publish(AVFrameQueue& q, int id, bool transfer) {
    if (transfer) {
        auto* f = q.acquireWriteFrame(); f->id=id;
        assert(q.pushTransferred(f));
    } else {
        AVFrame f; f.id=id; assert(q.push(&f));
    }
}
static void lowLatency(bool transfer) {
    AVFrameQueue q;
    q.configure(3,60,transfer,0,true);
    assert(q.targetDepth()==0);
    bool consumed=true;
    assert(q.pop(&consumed)==nullptr && !consumed);
    publish(q,1,transfer);
    auto* first=q.pop(&consumed);
    assert(first && first->id==1 && consumed && q.size()==0);
    assert(q.pop(&consumed)==first && !consumed);
    // A burst must retain the newest decoded frame and protect the displayed one.
    for(int i=2;i<=100;i++) {
        publish(q,i,transfer);
        assert(q.size()==1 && first->id==1);
    }
    assert(q.pop(&consumed)->id==100 && consumed && q.size()==0);
    assert(q.getFramesDropStat()==98);
    assert(q.getScheduledHoldStat()==0 && q.getRebufferHoldStat()==0);
    // A long rendering pause must not rebuild the two-frame jitter reserve.
    std::this_thread::sleep_for(std::chrono::milliseconds(270));
    publish(q,101,transfer);
    assert(q.pop(&consumed)->id==101 && consumed);
    q.cleanup();
    assert(q.size()==0 && q.pop()==nullptr);
    q.configure(3,60,transfer,0,true);
    publish(q,102,transfer);
    assert(q.pop()->id==102);
}
static void smoothVideo() {
    AVFrameQueue q;
    q.configure(3,60,false);
    assert(q.targetDepth()==2);
    publish(q,1,false); assert(q.pop()==nullptr);
    publish(q,2,false); assert(q.pop()==nullptr);
    publish(q,3,false); assert(q.pop()->id==1 && q.size()==2);
    // Switching policy also discards an existing stale backlog.
    q.configure(3,60,false,0,true);
    assert(q.size()==1 && q.pop()->id==3);
}
static void concurrent() {
    AVFrameQueue q; q.configure(3,60,true,0,true);
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for(int i=1;i<=10000;i++) publish(q,i,true);
        done=true;
    });
    int last=0;
    do {
        bool consumed=false;
        auto* f=q.pop(&consumed);
        if(consumed) { assert(f->id>last); last=f->id; }
        assert(q.size()<=1);
    } while(!done || q.size());
    producer.join();
    if(auto* f=q.pop()) last=f->id;
    assert(last==10000);
}
int main() {
    lowLatency(false); lowLatency(true); smoothVideo(); concurrent();
    assert(liveFrames==0);
    std::cout << "PASS: latest frame, ownership, burst, pause, smooth mode, concurrency, cleanup\n";
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    args = parser.parse_args()
    header = (ROOT / 'app/src/streaming/AVFrameHolder.hpp').read_text()
    source = (ROOT / 'app/src/streaming/AVFrameHolder.cpp').read_text()
    # Keep class declarations and queue methods verbatim; replace external includes.
    header = re.sub(r'^\s*#(?:include|pragma)[^\n]*', '', header, flags=re.M)
    source = source.replace('#include "AVFrameHolder.hpp"', '')
    with tempfile.TemporaryDirectory(prefix='moonlight-queue-') as tmp:
        cpp = Path(tmp) / 'queue.cpp'
        binary = Path(tmp) / ('queue.exe' if os.name == 'nt' else 'queue')
        cpp.write_text(FIXTURE + header + source + SCENARIOS)
        subprocess.run(shlex.split(args.cxx, posix=os.name != 'nt') + [
            '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
            str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
