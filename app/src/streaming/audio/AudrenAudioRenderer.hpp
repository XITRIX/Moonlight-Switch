#include "IAudioRenderer.hpp"
#include <opus/opus_multistream.h>
#include <switch.h>
#pragma once

#define BUFFER_COUNT 5

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
    size_t append_audio(const void* buf, size_t size);
    void write_audio(const void* buf, size_t size);
    bool flush();

    OpusMSDecoder* m_decoder = nullptr;
    s16* m_decoded_buffer = nullptr;
    void* mempool_ptr = nullptr;
    void* current_pool_ptr = nullptr;

    AudioDriver m_driver;
    AudioDriverWaveBuf m_wavebufs[BUFFER_COUNT];
    AudioDriverWaveBuf* m_current_wavebuf;
    Mutex m_update_lock;

    bool m_inited_driver = false;
    int m_channel_count = 0;
    int m_sample_rate = 0;
    int m_buffer_size = 0;
    int m_samples = 0;
    size_t m_total_queued_samples = 0;
    ssize_t m_current_size = 0;

    const int m_samples_per_frame = AUDREN_SAMPLES_PER_FRAME_48KHZ;
    const int m_latency = 5;
};
