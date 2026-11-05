#ifdef __SWITCH__

#include "AudrenAudioRenderer.hpp"
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

// Audio waiting in moonlight-common-c or in audren beyond these limits is
// dropped, so a Wi-Fi stall cannot leave a permanent delay behind it. The
// pending limit matches the desktop Moonlight clients.
constexpr int MAX_PENDING_MS = 30;
constexpr int MAX_QUEUED_MS = 40;

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
    m_channel_count = opus_config->channelCount;
    m_sample_rate = opus_config->sampleRate;
    m_frame_samples = opus_config->samplesPerFrame > 0
                          ? opus_config->samplesPerFrame
                          : AUDREN_SAMPLES_PER_FRAME_48KHZ;
    m_buffer_size = m_frame_samples * m_channel_count * sizeof(s16);
    m_dropped_packets = 0;

    brls::Logger::info("Audren: Init with channels: {}, sample rate: {}, frame: {} samples",
                       m_channel_count, m_sample_rate, m_frame_samples);

    m_decoded_buffer = (s16*)malloc(m_buffer_size);

    int error;
    m_decoder = opus_multistream_decoder_create(
        opus_config->sampleRate, opus_config->channelCount,
        opus_config->streams, opus_config->coupledStreams, opus_config->mapping,
        &error);

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

    for (int i = 0; i < BUFFER_COUNT; i++) {
        m_wavebufs[i].data_raw = mempool_ptr;
        m_wavebufs[i].size = mempool_size;
        m_wavebufs[i].start_sample_offset = i * m_frame_samples;
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
    }

    audrvVoiceInit(&m_driver, 0, m_channel_count, PcmFormat_Int16,
                   m_sample_rate);
    audrvVoiceSetDestinationMix(&m_driver, 0, AUDREN_FINAL_MIX_ID);

    for (int i = 0; i < m_channel_count; i++) {
        for (int j = 0; j < m_channel_count; j++) {
            audrvVoiceSetMixFactor(&m_driver, 0, i == j ? 1.0f : 0.0f, i, j);
        }
    }

    audrvVoiceStart(&m_driver, 0);

    m_inited_driver = true;

    brls::Logger::info("Audren: Init done!");

    return DR_OK;
}

void AudrenAudioRenderer::cleanup() {
    brls::Logger::info("Audren: Cleanup... dropped {} late packets",
                       m_dropped_packets);

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

    if (mempool_ptr) {
        free(mempool_ptr);
        mempool_ptr = nullptr;
    }

    brls::Logger::info("Audren: Cleanup done!");
}

// Audio handed to audren and not played yet. Counted from the wave buffer
// states rather than the voice's played-sample counter, which restarts when
// the voice runs dry and restarts.
int AudrenAudioRenderer::queued_ms() {
    int pending = 0;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (m_wavebufs[i].state == AudioDriverWaveBufState_Waiting ||
            m_wavebufs[i].state == AudioDriverWaveBufState_Queued ||
            m_wavebufs[i].state == AudioDriverWaveBufState_Playing)
            pending++;
    }
    return pending * m_frame_samples * 1000 / m_sample_rate;
}

void AudrenAudioRenderer::decode_and_play_sample(char* data, int length) {
    if (!m_inited_driver || !m_decoder || !m_decoded_buffer) {
        brls::Logger::error("Audren: Invalid call of decode_and_play_sample");
        return;
    }

    // Always decode, even packets we drop, so the Opus decoder state stays
    // continuous. A NULL packet asks Opus for packet loss concealment.
    const int decoded = opus_multistream_decode(
        m_decoder, (const unsigned char*)data, data ? length : 0,
        m_decoded_buffer, m_frame_samples, 0);
    if (decoded <= 0)
        return;

    // Refresh the wave buffer states once per packet.
    audrvUpdate(&m_driver);

    ssize_t index = -1;
    if (LiGetPendingAudioDuration() > MAX_PENDING_MS ||
        queued_ms() > MAX_QUEUED_MS ||
        (index = free_wavebuf_index()) < 0) {
        m_dropped_packets++;
        return;
    }

    applyVolume(m_decoded_buffer, static_cast<size_t>(decoded) * m_channel_count,
                Settings::instance().get_volume());

    AudioDriverWaveBuf* wavebuf = &m_wavebufs[index];
    const size_t bytes = static_cast<size_t>(decoded) * m_channel_count * sizeof(s16);
    void* dst = static_cast<char*>(mempool_ptr) + index * m_buffer_size;
    memcpy(dst, m_decoded_buffer, bytes);
    armDCacheFlush(dst, bytes);

    wavebuf->end_sample_offset = wavebuf->start_sample_offset + decoded;

    audrvVoiceAddWaveBuf(&m_driver, 0, wavebuf);
    if (!audrvVoiceIsPlaying(&m_driver, 0))
        audrvVoiceStart(&m_driver, 0);
    audrvUpdate(&m_driver);
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
