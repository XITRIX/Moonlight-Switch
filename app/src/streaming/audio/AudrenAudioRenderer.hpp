#ifdef __SWITCH__

#include "IAudioRenderer.hpp"
#include <opus/opus_multistream.h>
#include <switch.h>
#pragma once

// One wave buffer holds exactly one Opus packet (5 ms at Sunshine's default),
// so a packet is handed to audren as soon as it is decoded.
#define BUFFER_COUNT 12

class AudrenAudioRenderer : public IAudioRenderer {
  public:
    AudrenAudioRenderer(){};
    ~AudrenAudioRenderer(){};

    int init(int audio_configuration,
             const POPUS_MULTISTREAM_CONFIGURATION opus_config, void* context,
             int ar_flags) override;
    void cleanup() override;
    void decode_and_play_sample(char* sample_data, int sample_length) override;
    int capabilities() override;

  private:
    ssize_t free_wavebuf_index();
    int queued_ms();

    OpusMSDecoder* m_decoder = nullptr;
    s16* m_decoded_buffer = nullptr;
    void* mempool_ptr = nullptr;

    AudioDriver m_driver;
    AudioDriverWaveBuf m_wavebufs[BUFFER_COUNT];

    bool m_inited_driver = false;
    int m_channel_count = 0;
    int m_sample_rate = 0;
    int m_frame_samples = 0;  // samples per channel in one Opus packet
    int m_buffer_size = 0;    // bytes in one wave buffer
    size_t m_dropped_packets = 0;
};

#endif // __SWITCH__
