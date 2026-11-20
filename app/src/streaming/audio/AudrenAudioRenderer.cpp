#ifdef __SWITCH__

#include "AudrenAudioRenderer.hpp"
#include "AudioQueuePolicy.hpp"
#include <Settings.hpp>
#include <borealis.hpp>
#include <algorithm>
#include <climits>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t m_sink_channels[] = {0, 1};

namespace {

class AudioLock {
public:
    explicit AudioLock(Mutex* mutex) : mutex(mutex) { mutexLock(mutex); }
    ~AudioLock() { mutexUnlock(mutex); }
    AudioLock(const AudioLock&) = delete;
    AudioLock& operator=(const AudioLock&) = delete;
private:
    Mutex* mutex;
};

// Audio waiting in moonlight-common-c or in audren beyond these limits is
// dropped, so a Wi-Fi stall cannot leave a permanent delay behind it. The
// pending limit matches the desktop Moonlight clients.

void applyVolume(s16* buffer, size_t sampleCount, int volume) {
    if (volume == 100)
        return;

    for (size_t i = 0; i < sampleCount; i++) {
        const int scaled = (static_cast<int>(buffer[i]) * volume) / 100;
        buffer[i] = static_cast<s16>(std::min<int>(SHRT_MAX, std::max<int>(SHRT_MIN, scaled)));
    }
}

} // namespace

static const AudioRendererConfig m_ar_config = {
    .output_rate = AudioRendererOutputRate_48kHz,
    .num_voices = 24,
    .num_effects = 0,
    .num_sinks = 1,
    .num_mix_objs = 1,
    .num_mix_buffers = 2,
};

int AudrenAudioRenderer::init(int audio_configuration,
                              const POPUS_MULTISTREAM_CONFIGURATION opus_config,
                              void* context, int ar_flags) {
    mutexInit(&m_audio_mutex);
    condvarInit(&m_audio_wakeup);
    m_stop_requested = m_accepting = false;
    m_trace_ready = false;
    m_log_enabled = Settings::instance().save_diagnostic_log();
    m_last_update_error = 0;
    m_pcm_read = m_pcm_write = m_pcm_count = 0;
    m_pcm_samples = 0;
    m_input_gap = true;
    m_cached_driver_ms = m_pending_ms = 0;
    m_pump_calls = m_last_pump_us = m_pump_gap_max_us = m_pitch_time_ms = 0;
    m_channel_count = opus_config->channelCount;
    m_sample_rate = opus_config->sampleRate;
    m_frame_samples = opus_config->samplesPerFrame > 0
                          ? opus_config->samplesPerFrame
                          : AUDREN_SAMPLES_PER_FRAME_48KHZ;
    // Audren exposes four hardware wave slots. Coalesce short Opus packets
    // into at least 10 ms blocks so a 30 ms reserve fits on the hardware.
    m_batch_target = m_frame_samples * std::max(1, (m_sample_rate / 100 + m_frame_samples - 1) / m_frame_samples);
    m_staged_samples = 0;
    m_buffer_size = (m_batch_target + m_frame_samples) * m_channel_count * sizeof(s16);
    m_dropped_packets = 0;
    m_rebuffers = m_concealed = m_decode_errors = 0;
    m_reserve_ms = Settings::instance().audio_reserve_ms();
    m_reserve.reset(m_reserve_ms);
    m_drop_input = m_drop_output = m_drop_capacity = 0;
    m_audio_trace_ms = 0;
    m_sample_queue.reset();
    m_update_calls = m_update_us = m_update_max_us = m_update_errors = 0;
    m_audio_callbacks = m_decoded_samples = m_accepted_samples = 0;
    m_discarded_samples = 0;
    m_last_callback_us = m_callback_gap_max_us = 0;
    m_callback_gap_window_us = m_seam_repairs = 0;
    m_playback_pitch = 1.0f;
    m_seam.reset();
    brls::Logger::info("Audren: reserve={} ms", Settings::instance().audio_reserve_ms());

    brls::Logger::info("Audren: Init with channels: {}, sample rate: {}, frame: {} samples",
                       m_channel_count, m_sample_rate, m_frame_samples);

    m_decoded_buffer = (s16*)malloc(m_buffer_size);
    if (!m_decoded_buffer)
        return -1;
    m_staging_buffer = (s16*)malloc(m_buffer_size);
    if (!m_staging_buffer) { cleanup(); return -1; }
    m_pcm_buffer = (s16*)malloc(static_cast<size_t>(m_buffer_size) * BUFFER_COUNT);
    if (!m_pcm_buffer) { cleanup(); return -1; }
    brls::Logger::info("Audren: batch_target={} samples; hardware_wave_slots=4", m_batch_target);

    int error;
    m_decoder = opus_multistream_decoder_create(
        opus_config->sampleRate, opus_config->channelCount,
        opus_config->streams, opus_config->coupledStreams, opus_config->mapping,
        &error);
    if (!m_decoder || error != OPUS_OK) {
        cleanup();
        return -1;
    }

    memset(&m_driver, 0, sizeof(m_driver));
    memset(m_wavebufs, 0, sizeof(m_wavebufs));

    int mempool_size =
        (m_buffer_size * BUFFER_COUNT + (AUDREN_MEMPOOL_ALIGNMENT - 1)) &
        ~(AUDREN_MEMPOOL_ALIGNMENT - 1);
    mempool_ptr = memalign(AUDREN_MEMPOOL_ALIGNMENT, mempool_size);

    // moonlight-common-c does not call cleanup() when init() fails, so free
    // what was allocated here on every failure path.
    if (!mempool_ptr) {
        brls::Logger::error("Audren: mempool alloc failed");
        cleanup();
        return -1;
    }

    Result rc = audrenInitialize(&m_ar_config);
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audrenInitialize: {:x}", rc);
        cleanup();
        return -1;
    }

    rc = audrvCreate(&m_driver, &m_ar_config, m_channel_count);
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audrvCreate: {:x}", rc);
        audrenExit();
        cleanup();
        return -1;
    }
    m_inited_driver = true;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        m_wavebufs[i].data_raw = mempool_ptr;
        m_wavebufs[i].size = mempool_size;
        m_wavebufs[i].start_sample_offset = i * (m_batch_target + m_frame_samples);
        m_wavebufs[i].end_sample_offset =
            m_wavebufs[i].start_sample_offset + m_frame_samples;
    }

    int mpid = audrvMemPoolAdd(&m_driver, mempool_ptr, mempool_size);
    audrvMemPoolAttach(&m_driver, mpid);

    audrvDeviceSinkAdd(&m_driver, AUDREN_DEFAULT_DEVICE_NAME, m_channel_count,
                       m_sink_channels);

    rc = audrenStartAudioRenderer();
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audrenStartAudioRenderer: {:x}", rc);
        cleanup();
        return -1;
    }

    audrvVoiceInit(&m_driver, 0, m_channel_count, PcmFormat_Int16,
                   m_sample_rate);
    audrvVoiceSetDestinationMix(&m_driver, 0, AUDREN_FINAL_MIX_ID);

    for (int i = 0; i < m_channel_count; i++) {
        for (int j = 0; j < m_channel_count; j++) {
            audrvVoiceSetMixFactor(&m_driver, 0, i == j ? 1.0f : 0.0f, i, j);
        }
    }

    // Queue the configured reserve before starting the voice.

    m_inited_driver = true;

    // The pump owns every driver operation during playback. It sleeps when
    // idle, wakes on ready PCM, and refreshes active buffers at most 5 ms apart
    // unless delayed by OS scheduling. Never busy-wait or allocate per packet.
    rc = threadCreate(&m_pump_thread, pump_entry, this, nullptr, 0x10000, 0x2c, -2);
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audio thread creation failed: {:x}", rc);
        cleanup(); return -1;
    }
    m_thread_created = true;
    rc = threadStart(&m_pump_thread);
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audio thread start failed: {:x}", rc);
        cleanup(); return -1;
    }
    m_thread_started = true;
    {
        AudioLock lock(&m_audio_mutex);
        m_accepting = true;
    }
    brls::Logger::info("Audren: dedicated pump; active_wait=5 ms; thread_stack=64 KiB; preallocated PCM pool={} bytes", m_buffer_size * BUFFER_COUNT);
    brls::Logger::info("Audren: Init done!");

    return DR_OK;
}

void AudrenAudioRenderer::cleanup() {
    {
        AudioLock lock(&m_audio_mutex);
        m_accepting = false;
        m_stop_requested = true;
        condvarWakeAll(&m_audio_wakeup);
    }
    // Join without holding the mutex, before closing the service or freeing
    // either PCM pool. A failed threadStart must not be joined.
    if (m_thread_started) {
        threadWaitForExit(&m_pump_thread);
        m_thread_started = false;
    }
    if (m_thread_created) {
        threadClose(&m_pump_thread);
        m_thread_created = false;
    }
    AudioLock lock(&m_audio_mutex);
    brls::Logger::info("Audren: Cleanup... {} discard events",
                       m_dropped_packets);
    brls::Logger::info("Audren totals: decoded_samples={} accepted_samples={} discarded_samples={} staged_samples={} update_calls={} update_total_us={} update_max_us={} update_errors={} callback_gap_max_us={}",
        m_decoded_samples, m_accepted_samples, m_discarded_samples, m_staged_samples, m_update_calls, m_update_us, m_update_max_us, m_update_errors, m_callback_gap_max_us);
    brls::Logger::info("Audren: rebuffers={} concealed_packets={} decode_errors={}", m_rebuffers, m_concealed, m_decode_errors);
    brls::Logger::info("Audren: drops_input={} drops_output={} drops_capacity={}", m_drop_input, m_drop_output, m_drop_capacity);
    brls::Logger::info("Audren pump totals: calls={} gap_max_us={} queued_pcm_samples={}", m_pump_calls, m_pump_gap_max_us, m_pcm_samples);

    // Stop and close the driver before freeing the memory pool it still uses.
    if (m_inited_driver) {
        m_inited_driver = false;
        audrvVoiceStop(&m_driver, 0);
        audrvClose(&m_driver);
        audrenExit();
    }

    if (m_decoder) {
        opus_multistream_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }

    if (m_decoded_buffer) {
        free(m_decoded_buffer);
        m_decoded_buffer = nullptr;
    }
    if (m_staging_buffer) { free(m_staging_buffer); m_staging_buffer = nullptr; }
    m_staged_samples = 0;
    if (m_pcm_buffer) { free(m_pcm_buffer); m_pcm_buffer = nullptr; }
    m_pcm_count = 0;
    m_pcm_samples = 0;

    if (mempool_ptr) {
        free(mempool_ptr);
        mempool_ptr = nullptr;
    }

    brls::Logger::info("Audren: Cleanup done!");
}

// Coarse fallback: includes the unplayed part and played prefix of the current
// buffer. The sample counter below is the normal, more precise measurement.
int AudrenAudioRenderer::queued_buffers_ms() {
    int pending = 0;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (m_wavebufs[i].state == AudioDriverWaveBufState_Waiting ||
            m_wavebufs[i].state == AudioDriverWaveBufState_Queued ||
            m_wavebufs[i].state == AudioDriverWaveBufState_Playing)
            pending += m_wavebufs[i].end_sample_offset - m_wavebufs[i].start_sample_offset;
    }
    return pending * 1000 / m_sample_rate;
}

int AudrenAudioRenderer::queued_ms() {
    const uint32_t played = audrvVoiceGetPlayedSampleCount(&m_driver, 0);
    const uint64_t fallback = static_cast<uint64_t>(queued_buffers_ms()) * m_sample_rate / 1000;
    m_sample_queue.observe(played, fallback);
    return m_sample_queue.milliseconds(m_sample_rate);
}

Result AudrenAudioRenderer::update_driver() {
    const uint64_t before = m_log_enabled ? LiGetMicroseconds() : 0;
    const Result result = audrvUpdate(&m_driver);
    const uint64_t elapsed = m_log_enabled ? LiGetMicroseconds() - before : 0;
    ++m_update_calls;
    m_update_us += elapsed;
    m_update_max_us = std::max(m_update_max_us, elapsed);
    if (R_FAILED(result)) {
        ++m_update_errors;
        m_last_update_error = result;
        if (m_update_errors <= 3) m_audio_trace_ms = 0;
    }
    return result;
}

void AudrenAudioRenderer::decode_and_play_sample(char* data, int length) {
    decode_packet(data, length);
    // File output/formatting must never hold the audio lock or run on the
    // pump. The next receiver callback consumes the diagnostic snapshot.
    if (m_log_enabled) flush_trace();
}

void AudrenAudioRenderer::decode_packet(char* data, int length) {
    // Short bounded critical section: Opus decode + copy, never driver IPC.
    // Cleanup first disables acceptance under this same lock.
    AudioLock lock(&m_audio_mutex);
    if (!m_accepting || !m_decoder || !m_decoded_buffer) return;
    const uint64_t callbackTime = m_log_enabled ? LiGetMicroseconds() : 0;
    ++m_audio_callbacks;
    if (m_last_callback_us) {
        const uint64_t gap = callbackTime - m_last_callback_us;
        m_callback_gap_max_us = std::max(m_callback_gap_max_us, gap);
        m_callback_gap_window_us = std::max(m_callback_gap_window_us, gap);
    }
    m_last_callback_us = callbackTime;
    // Preserve Opus state, including packets that cannot be queued.
    if (!data) ++m_concealed;
    const int decoded = opus_multistream_decode(m_decoder,
        (const unsigned char*)data, data ? length : 0,
        m_decoded_buffer, m_frame_samples, 0);
    if (decoded <= 0) { ++m_decode_errors; m_input_gap = true; return; }
    m_decoded_samples += decoded;
    memcpy(m_staging_buffer + m_staged_samples * m_channel_count,
        m_decoded_buffer, static_cast<size_t>(decoded) * m_channel_count * sizeof(s16));
    m_staged_samples += decoded;
    if (m_staged_samples < m_batch_target) return;
    const int samples = m_staged_samples;
    m_staged_samples = 0;
    m_pending_ms = LiGetPendingAudioDuration();
    const int totalMs = m_cached_driver_ms +
        static_cast<int>((m_pcm_samples * 1000 + m_sample_rate - 1) / m_sample_rate);
    const auto reason = AudioQueuePolicy::decide(m_reserve_ms, m_pending_ms,
        totalMs, m_pcm_count < BUFFER_COUNT);
    if (reason != AudioQueuePolicy::None) {
        if (reason == AudioQueuePolicy::InputBacklog) ++m_drop_input;
        if (reason == AudioQueuePolicy::OutputBacklog) ++m_drop_output;
        if (reason == AudioQueuePolicy::NoBuffer) ++m_drop_capacity;
        ++m_dropped_packets;
        m_discarded_samples += samples;
        m_input_gap = true;
        return;
    }
    applyVolume(m_staging_buffer, static_cast<size_t>(samples) * m_channel_count,
                Settings::instance().get_volume());
    void* dst = reinterpret_cast<char*>(m_pcm_buffer) + m_pcm_write * m_buffer_size;
    memcpy(dst, m_staging_buffer, static_cast<size_t>(samples) * m_channel_count * sizeof(s16));
    m_pcm_frames[m_pcm_write] = samples;
    m_pcm_gap[m_pcm_write] = m_input_gap;
    m_input_gap = false;
    m_pcm_write = (m_pcm_write + 1) % BUFFER_COUNT;
    ++m_pcm_count;
    m_pcm_samples += samples;
    condvarWakeOne(&m_audio_wakeup);
}

void AudrenAudioRenderer::pump_entry(void* context) {
    auto* self = static_cast<AudrenAudioRenderer*>(context);
    AudioLock lock(&self->m_audio_mutex);
    while (!self->m_stop_requested) {
        if (self->m_pcm_count || self->m_cached_driver_ms > 0) self->pump_once();
        if (self->m_stop_requested) break;
        if (self->m_pcm_count || self->m_cached_driver_ms > 0)
            condvarWaitTimeout(&self->m_audio_wakeup, &self->m_audio_mutex, 5000000);
        else
            condvarWait(&self->m_audio_wakeup, &self->m_audio_mutex);
    }
}

void AudrenAudioRenderer::pump_once() {
    const uint64_t now = LiGetMicroseconds();
    if (m_last_pump_us && m_cached_driver_ms > 0)
        m_pump_gap_max_us = std::max(m_pump_gap_max_us, now - m_last_pump_us);
    m_last_pump_us = now;
    ++m_pump_calls;
    if (R_FAILED(update_driver())) return; // retain PCM and retry on next wake
    m_cached_driver_ms = queued_ms();
    if (m_reserve.underrun(m_cached_driver_ms)) {
        ++m_rebuffers;
        audrvVoiceStop(&m_driver, 0);
        if (R_FAILED(update_driver())) return;
        m_sample_queue.reset(audrvVoiceGetPlayedSampleCount(&m_driver, 0));
        m_seam.reset();
        m_playback_pitch = 1.0f;
        m_audio_trace_ms = 0;
    }
    const int totalMs = m_cached_driver_ms +
        static_cast<int>((m_pcm_samples * 1000 + m_sample_rate - 1) / m_sample_rate);
    const uint64_t clockMs = now / 1000;
    if (clockMs - m_pitch_time_ms >= 10) {
        m_pitch_time_ms = clockMs;
        const int correctionTarget = std::max(m_reserve_ms, m_batch_target * 1000 / m_sample_rate);
        m_playback_pitch = AudioQueuePolicy::slew(m_playback_pitch,
            AudioQueuePolicy::pitch(correctionTarget, totalMs));
        audrvVoiceSetPitch(&m_driver, 0, m_playback_pitch);
    }
    bool submitted = false;
    while (m_pcm_count) {
        const ssize_t index = free_wavebuf_index();
        if (index < 0) break; // wait for a completed buffer, never overwrite it
        const int samples = m_pcm_frames[m_pcm_read];
        s16* pcm = reinterpret_cast<s16*>(reinterpret_cast<char*>(m_pcm_buffer) + m_pcm_read * m_buffer_size);
        if (m_pcm_gap[m_pcm_read]) m_seam.markGap();
        AudioSeam nextSeam = m_seam;
        const bool repaired = nextSeam.apply(pcm, samples, m_channel_count, m_sample_rate);
        const size_t bytes = static_cast<size_t>(samples) * m_channel_count * sizeof(s16);
        void* dst = static_cast<char*>(mempool_ptr) + index * m_buffer_size;
        memcpy(dst, pcm, bytes);
        armDCacheFlush(dst, bytes);
        AudioDriverWaveBuf* wavebuf = &m_wavebufs[index];
        wavebuf->end_sample_offset = wavebuf->start_sample_offset + samples;
        m_pcm_samples -= samples;
        m_pcm_read = (m_pcm_read + 1) % BUFFER_COUNT;
        --m_pcm_count;
        const bool added = audrvVoiceAddWaveBuf(&m_driver, 0, wavebuf);
        if (!added) {
            ++m_drop_capacity;
            ++m_dropped_packets;
            m_discarded_samples += samples;
            m_seam.markGap();
            continue;
        }
        m_seam = nextSeam;
        if (repaired) ++m_seam_repairs;
        m_sample_queue.accepted(static_cast<uint32_t>(samples));
        m_accepted_samples += samples;
        submitted = true;
    }
    m_cached_driver_ms = queued_ms();
    if (m_cached_driver_ms > 0 && m_reserve.ready(m_cached_driver_ms) && !audrvVoiceIsPlaying(&m_driver, 0)) {
        audrvVoiceStart(&m_driver, 0);
        submitted = true;
    }
    if (submitted) update_driver();
    trace_audio();
}

void AudrenAudioRenderer::trace_audio() {
    if (!m_log_enabled) return;
    const uint64_t now = LiGetMillis();
    if (now - m_audio_trace_ms < 1000) return;
    m_audio_trace_ms = now;
    m_trace = {m_pending_ms, m_cached_driver_ms, queued_buffers_ms(),
        m_sample_queue.raw(), audrvVoiceGetVoiceDropsCount(&m_driver, 0),
        m_sample_queue.resyncs(), m_drop_input, m_drop_output, m_drop_capacity, m_rebuffers,
        m_audio_callbacks, m_decoded_samples, m_accepted_samples, m_update_calls,
        m_update_us, m_update_max_us, m_update_errors, m_callback_gap_max_us,
        m_callback_gap_window_us, m_seam_repairs, m_pump_calls, m_pump_gap_max_us,
        m_pcm_samples, m_pcm_count, m_playback_pitch, m_last_update_error};
    m_trace_ready = true;
    m_callback_gap_window_us = 0;
}

void AudrenAudioRenderer::flush_trace() {
    TraceSnapshot t;
    {
        AudioLock lock(&m_audio_mutex);
        if (!m_accepting || !m_trace_ready) return;
        t = m_trace;
        m_trace_ready = false;
    }
    brls::Logger::info("Audren queue: reserve={} pending={} queued_samples_ms={} queued_buffers_ms={} played_raw={} counter_resyncs={} pitch={} drops_input={} drops_output={} drops_capacity={} rebuffers={}",
        m_reserve_ms, t.pending, t.driverMs, t.buffersMs, t.played, t.resyncs, t.pitch, t.dropsInput, t.dropsOutput, t.dropsCapacity, t.rebuffers);
    brls::Logger::info("Audren timing: callbacks={} decoded_samples={} accepted_samples={} update_calls={} update_total_us={} update_max_us={} update_errors={} callback_gap_max_us={} hardware_voice_drops={}",
        t.callbacks, t.decoded, t.accepted, t.updates, t.updateUs, t.updateMaxUs, t.updateErrors, t.gapMaxUs, t.hardwareDrops);
    brls::Logger::info("Audren recovery: callback_gap_window_us={} seam_repairs={} playback_pitch={}", t.gapWindowUs, t.seamRepairs, t.pitch);
    brls::Logger::info("Audren pump: calls={} gap_max_us={} software_samples={} software_blocks={} driver_ms={} last_update_error={:x}", t.pumpCalls, t.pumpGapUs, t.pcmSamples, t.pcmBlocks, t.driverMs, t.lastError);
}

int AudrenAudioRenderer::capabilities() {
    return 0;
}

ssize_t AudrenAudioRenderer::free_wavebuf_index() {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (m_wavebufs[i].state == AudioDriverWaveBufState_Free ||
            m_wavebufs[i].state == AudioDriverWaveBufState_Done) {
            return i;
        }
    }
    return -1;
}

#endif //__SWITCH__
