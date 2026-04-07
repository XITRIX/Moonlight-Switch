/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "SDLAudiorenderer.hpp"

#include <Limelight.h>
#include <Settings.hpp>

#include <algorithm>
#include <climits>
#include <cstdio>

namespace {

void applyVolume(short* buffer, size_t sampleCount, int volume) {
    if (volume >= 100) {
        return;
    }

    for (size_t i = 0; i < sampleCount; i++) {
        const int scaled = (static_cast<int>(buffer[i]) * volume) / 100;
        buffer[i] = static_cast<short>(std::min<int>(SHRT_MAX, std::max<int>(SHRT_MIN, scaled)));
    }
}

} // namespace

int SDLAudioRenderer::init(int audio_configuration,
                           const POPUS_MULTISTREAM_CONFIGURATION opus_config,
                           void* context, int ar_flags) {
    int rc;
    decoder = opus_multistream_decoder_create(
        opus_config->sampleRate, opus_config->channelCount,
        opus_config->streams, opus_config->coupledStreams, opus_config->mapping,
        &rc);

    channelCount = opus_config->channelCount;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = opus_config->sampleRate;
    want.format = AUDIO_S16LSB;
    want.channels = opus_config->channelCount;

    // Latest SDL2 Switch port broke audio for lower asmples
#if defined(PLATFORM_SWITCH)
    want.samples = 4096;
#else
    want.samples = std::max(480, opus_config->samplesPerFrame); //1024;
#endif

    dev =
        SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        brls::Logger::error("Failed to open audio: %s\n", SDL_GetError());
        return -1;
    } else {
        if (have.format != want.format) // we let this one thing change.
            brls::Logger::error("We didn't get requested audio format.\n");
        SDL_PauseAudioDevice(dev, 0); // start audio playing.
    }

    return 0;
}

void SDLAudioRenderer::cleanup() {
    if (decoder != nullptr)
        opus_multistream_decoder_destroy(decoder);

    SDL_CloseAudioDevice(dev);
}

void SDLAudioRenderer::decode_and_play_sample(char* sample_data,
                                              int sample_length) {
    int decodeLen =
        opus_multistream_decode(decoder, (const unsigned char*)sample_data,
                                sample_length, pcmBuffer, FRAME_SIZE, 0);

    if (decodeLen <= 0) { 
        printf("Opus error from decode: %d\n", decodeLen);
        return;
    }

    if (LiGetPendingAudioDuration() > 30) {
        return;
    }

    applyVolume(pcmBuffer,
                static_cast<size_t>(decodeLen) * channelCount,
                Settings::instance().get_volume());

#if defined(PLATFORM_SWITCH)
    int bufferOverflow = 24000;
#else
    int bufferOverflow = 16000;
#endif

    if (SDL_GetQueuedAudioSize(dev) > bufferOverflow) {
        // clear audio queue to avoid big audio delay
        // average values are close to bufferOverflow bytes
        SDL_ClearQueuedAudio(this->dev);
    }
    SDL_QueueAudio(dev, pcmBuffer,
                    decodeLen * channelCount * sizeof(short));
}

int SDLAudioRenderer::capabilities() {
#if defined(PLATFORM_SWITCH)
    return 0;
#else
    return CAPABILITY_DIRECT_SUBMIT;
#endif
}
