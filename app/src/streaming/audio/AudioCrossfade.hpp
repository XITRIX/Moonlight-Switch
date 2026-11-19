#pragma once

#include <cstddef>
#include <cstdint>

// Blends the start of `to` from `from`, frame by frame, so audio picked up
// after dropped packets doesn't click. `from` is the start of the first
// packet that was dropped, which follows on from what already played.
inline void crossfadeAudio(const int16_t* from, int16_t* to, size_t frames,
                           int channels) {
    for (size_t frame = 0; frame < frames; frame++) {
        const float weight = float(frame + 1) / float(frames + 1);
        for (int channel = 0; channel < channels; channel++) {
            const size_t i = frame * channels + channel;
            const float start = from[i];
            const float blended = start + (to[i] - start) * weight;
            to[i] = int16_t(blended < 0 ? blended - 0.5f : blended + 0.5f);
        }
    }
}
