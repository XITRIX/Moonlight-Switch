#pragma once
#include <algorithm>
#include <cstdint>

// Smooth only a known discontinuity, never alter normal contiguous blocks.
// Operates on newly decoded PCM, never on buffers owned by the driver.
class AudioSeam {
public:
    void reset() { *this = AudioSeam(); }
    void markGap() { gap = true; }
    bool apply(int16_t* pcm, int frames, int channels, int sampleRate) {
        if (!pcm || frames <= 0 || channels <= 0 || channels > 8) return false;
        const bool repaired = gap;
        if (gap) {
            const int ramp = std::min(frames, std::max(1, sampleRate / 500));
            for (int frame = 0; frame < ramp; ++frame) {
                for (int ch = 0; ch < channels; ++ch) {
                    const int index = frame * channels + ch;
                    const int64_t value = int64_t(tail[ch]) * (ramp - frame - 1)
                                        + int64_t(pcm[index]) * (frame + 1);
                    pcm[index] = static_cast<int16_t>(value / ramp);
                }
            }
        }
        for (int ch = 0; ch < channels; ++ch) tail[ch] = pcm[(frames - 1) * channels + ch];
        gap = false;
        return repaired;
    }
private:
    int16_t tail[8]{};
    bool gap = true;
};
