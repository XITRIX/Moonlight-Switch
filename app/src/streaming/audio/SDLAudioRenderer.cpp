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
    want.samples = 4096;// std::max(480, opus_config->samplesPerFrame); //1024;

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

    for (short & i : pcmBuffer) {
        int scale = (int)((double)i * (Settings::instance().get_volume() / 100.0));
        i = (short) std::min(SHRT_MAX, std::max(SHRT_MIN, scale));
    }

    if (SDL_GetQueuedAudioSize(dev) > 28000) {
        // clear audio queue to avoid big audio delay
        // average values are close to 28000 bytes
        SDL_ClearQueuedAudio(this->dev);
    }

    SDL_QueueAudio(dev, pcmBuffer,
                    decodeLen * channelCount * sizeof(short));
}

int SDLAudioRenderer::capabilities() { return CAPABILITY_DIRECT_SUBMIT; }
