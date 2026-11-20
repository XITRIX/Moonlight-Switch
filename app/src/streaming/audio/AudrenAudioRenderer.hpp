#ifdef __SWITCH__

#include "IAudioRenderer.hpp"
#include "AudioReserve.hpp"
#include "AudioSampleQueue.hpp"
#include "AudioSeam.hpp"
#include <opus/opus_multistream.h>
#include <switch.h>
#pragma once

// Software pool; the driver exposes only four hardware wave slots.
// Short Opus packets are coalesced into 10 ms hardware blocks.
#define BUFFER_COUNT 16

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
    int queued_buffers_ms();
    Result update_driver();
    static void pump_entry(void* context);
    void pump_once(); // called only with m_audio_mutex held
    void trace_audio();
    void decode_packet(char* data, int length);
    void flush_trace();

    OpusMSDecoder* m_decoder = nullptr;
    s16* m_decoded_buffer = nullptr;
    s16* m_staging_buffer = nullptr;
    int m_batch_target = 0, m_staged_samples = 0;
    void* mempool_ptr = nullptr;

    AudioDriver m_driver;
    AudioDriverWaveBuf m_wavebufs[BUFFER_COUNT];

    bool m_inited_driver = false;
    int m_channel_count = 0;
    int m_sample_rate = 0;
    int m_frame_samples = 0;  // samples per channel in one Opus packet
    int m_buffer_size = 0;    // bytes in one wave buffer
    size_t m_dropped_packets = 0;
    AudioReserve m_reserve;
    AudioSampleQueue m_sample_queue;
    size_t m_rebuffers = 0, m_concealed = 0, m_decode_errors = 0;
    int m_reserve_ms = 10;
    size_t m_drop_input = 0, m_drop_output = 0, m_drop_capacity = 0;
    uint64_t m_audio_trace_ms = 0;
    uint64_t m_update_calls = 0, m_update_us = 0, m_update_max_us = 0, m_update_errors = 0;
    uint64_t m_audio_callbacks = 0, m_decoded_samples = 0, m_accepted_samples = 0;
    uint64_t m_discarded_samples = 0;
    uint64_t m_last_callback_us = 0, m_callback_gap_max_us = 0;
    uint64_t m_callback_gap_window_us = 0, m_seam_repairs = 0;
    float m_playback_pitch = 1.0f;
    AudioSeam m_seam;
    Mutex m_audio_mutex{};
    CondVar m_audio_wakeup{};
    Thread m_pump_thread{};
    bool m_thread_created = false, m_thread_started = false;
    bool m_stop_requested = false, m_accepting = false;
    s16* m_pcm_buffer = nullptr;
    int m_pcm_frames[BUFFER_COUNT]{};
    bool m_pcm_gap[BUFFER_COUNT]{};
    int m_pcm_read = 0, m_pcm_write = 0, m_pcm_count = 0;
    uint64_t m_pcm_samples = 0;
    bool m_input_gap = true;
    int m_cached_driver_ms = 0, m_pending_ms = 0;
    uint64_t m_pump_calls = 0, m_last_pump_us = 0, m_pump_gap_max_us = 0;
    uint64_t m_pitch_time_ms = 0;
    struct TraceSnapshot {
        int pending = 0, driverMs = 0, buffersMs = 0;
        uint32_t played = 0, hardwareDrops = 0;
        uint64_t resyncs = 0, dropsInput = 0, dropsOutput = 0, dropsCapacity = 0, rebuffers = 0;
        uint64_t callbacks = 0, decoded = 0, accepted = 0, updates = 0;
        uint64_t updateUs = 0, updateMaxUs = 0, updateErrors = 0, gapMaxUs = 0;
        uint64_t gapWindowUs = 0, seamRepairs = 0, pumpCalls = 0, pumpGapUs = 0;
        uint64_t pcmSamples = 0;
        int pcmBlocks = 0;
        float pitch = 1.0f;
        Result lastError = 0;
    } m_trace;
    bool m_trace_ready = false;
    bool m_log_enabled = false;
    Result m_last_update_error = 0;
};

#endif // __SWITCH__
