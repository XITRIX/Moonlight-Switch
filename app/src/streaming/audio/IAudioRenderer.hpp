#include <Limelight.h>
#pragma once

class IAudioRenderer {
  public:
    virtual ~IAudioRenderer(){};
    virtual int init(int audio_configuration,
                     const POPUS_MULTISTREAM_CONFIGURATION opus_config,
                     void* context, int ar_flags) = 0;
    virtual void start(){};
    virtual void stop(){};
    virtual void cleanup() = 0;
    virtual void decode_and_play_sample(char* sample_data,
                                        int sample_length) = 0;
    virtual int capabilities() = 0;
};
