#pragma once

#include "IAudioRenderer.hpp"

#include <SDL.h>
#include <SDL_audio.h>
#include <opus/opus_multistream.h>

#define MAX_CHANNEL_COUNT 6
#define FRAME_SIZE 240
#define FRAME_BUFFER 12

class SDLAudioRenderer : public IAudioRenderer {
  public:
    SDLAudioRenderer(){};
    ~SDLAudioRenderer(){};

    int init(int audio_configuration,
             const POPUS_MULTISTREAM_CONFIGURATION opus_config, void* context,
             int ar_flags) override;
    void cleanup() override;
    void decode_and_play_sample(char* sample_data, int sample_length) override;
    int capabilities() override;

  private:
    OpusMSDecoder* decoder;
    short pcmBuffer[FRAME_SIZE * MAX_CHANNEL_COUNT];
    SDL_AudioDeviceID dev;
    int channelCount;
};
