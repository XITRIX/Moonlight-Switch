#!/usr/bin/env python3
"""Exercise VRR presentation policy and the real native frame queue on the host."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for presentation tests")
    with tempfile.TemporaryDirectory(prefix="moonlight-presentation-") as directory:
        tmp = Path(directory)
        (tmp / "libavcodec").mkdir()
        (tmp / "libavcodec/avcodec.h").write_text(
            "#pragma once\n#include <stdint.h>\n"
            "typedef struct {int64_t pts;} AVFrame;\n"
            "AVFrame* av_frame_alloc(void); void av_frame_free(AVFrame**);\n"
            "void av_frame_unref(AVFrame*); int av_frame_ref(AVFrame*,const AVFrame*);\n"
        )
        (tmp / "Singleton.hpp").write_text(
            "#pragma once\ntemplate<class T> class Singleton {public: "
            "static T& instance(){static T value;return value;}};\n"
        )
        (tmp / "Settings.hpp").write_text(
            '#pragma once\n#include "Singleton.hpp"\n'
            "class Settings:public Singleton<Settings>{public:int frames_queue_size(){return 1;}};\n"
        )
        (tmp / "test.cpp").write_text(
            r"""
#include <borealis/core/frame_presentation.hpp>
#include "AVFrameHolder.hpp"
#include <cassert>

static int live = 0;
extern "C" {
AVFrame* av_frame_alloc(){++live;return new AVFrame{};}
void av_frame_free(AVFrame** frame){if(*frame){--live;delete *frame;*frame=nullptr;}}
void av_frame_unref(AVFrame* frame){frame->pts=0;}
int av_frame_ref(AVFrame* out,const AVFrame* in){*out=*in;return 0;}
}
int main(){
    brls::FramePresentation policy;
    const uint64_t start=1000000000;
    assert(policy.shouldPresent(true,false,false,start));
    policy.setFrameDriven(true);
    assert(policy.shouldPresent(true,false,false,start));
    policy.presented(start);
    assert(!policy.shouldPresent(true,false,false,start+16666667));
    assert(policy.shouldPresent(true,true,false,start+1000));
    assert(policy.shouldPresent(true,false,true,start+1000));
    assert(policy.shouldPresent(false,false,false,start+1000));
    assert(policy.shouldPresent(true,false,false,start+34000000));
    assert(policy.waitNs(start+33000000)==1000000);
    assert(policy.waitNs(start+34000000)==0);
    assert(policy.waitNs(start)==8000000);
    policy.setFrameDriven(false);
    assert(policy.shouldPresent(true,false,false,start+1000));
    {
        AVFrameQueue queue;
        queue.configure(3,120,true,4);
        auto push=[&](int64_t pts){auto* f=queue.acquireWriteFrame();f->pts=pts;assert(queue.pushTransferred(f));};
        bool consumed=true;
        push(1);push(2);
        assert(!queue.pop(&consumed) && !consumed); // startup jitter reserve
        push(3);
        assert(queue.pop(&consumed)->pts==1 && consumed);
        assert(queue.pop(&consumed)->pts==2 && consumed); // no second clock gate
        assert(queue.pop(&consumed)->pts==3 && consumed);
        assert(queue.pop(&consumed)->pts==3 && !consumed); // held frame isn't new
        push(4);
        assert(queue.pop(&consumed)->pts==4 && consumed);
        for(int i=5;i<30;++i)push(i);
        assert(queue.size()<=queue.capacity());
    }
    assert(live==0);
}
"""
        )
        binary = tmp / "presentation-test"
        subprocess.run(
            [
                cxx,
                "-std=c++20",
                "-D__SWITCH2__",
                "-pthread",
                "-I" + str(tmp),
                "-I" + str(ROOT / "app/src/streaming"),
                "-I" + str(ROOT / "extern/borealis/library/include"),
                str(tmp / "test.cpp"),
                str(ROOT / "app/src/streaming/AVFrameHolder.cpp"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Native queue arrival pacing and VRR presentation policy passed")


if __name__ == "__main__":
    main()
