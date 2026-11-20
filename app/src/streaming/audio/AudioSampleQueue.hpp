#pragma once
#include <cstdint>
// Counts source samples per channel, not whole output buffers. Driver updates
// may report partial playback; unsigned subtraction handles the 32-bit wrap.
class AudioSampleQueue {
public:
    void reset(uint32_t raw = 0) { pending = 0; previous = raw; rebases = 0; }
    void accepted(uint32_t samples) { pending += samples; }
    void observe(uint32_t raw, uint64_t fallbackSamples) {
        const uint32_t delta = raw - previous;
        previous = raw;
        if (delta <= pending) pending -= delta;
        else { pending = fallbackSamples; ++rebases; }
    }
    int milliseconds(int rate) const { return rate > 0 ? static_cast<int>((pending * 1000 + rate - 1) / rate) : 0; }
    uint32_t raw() const { return previous; }
    uint64_t resyncs() const { return rebases; }
private:
    uint64_t pending = 0, rebases = 0;
    uint32_t previous = 0;
};
