#include "AudrenAudioRenderer.hpp"
#include <borealis.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>

static const uint8_t m_sink_channels[] = { 0, 1 };

static const AudioRendererConfig m_ar_config =
{
    .output_rate     = AudioRendererOutputRate_48kHz,
    .num_voices      = 24,
    .num_effects     = 0,
    .num_sinks       = 1,
    .num_mix_objs    = 1,
    .num_mix_buffers = 2,
};

int AudrenAudioRenderer::init(int audio_configuration, const POPUS_MULTISTREAM_CONFIGURATION opus_config, void *context, int ar_flags) {
    m_channel_count = opus_config->channelCount;
    m_sample_rate = opus_config->sampleRate;
    m_buffer_size = m_latency * m_samples_per_frame * sizeof(s16);
    m_samples = m_buffer_size / m_channel_count / sizeof(s16);
    m_current_size = 0;
    
    brls::Logger::info("Audren: Init with channels: {}, sample rate: {}", m_channel_count, m_sample_rate);
    
    mutexInit(&m_update_lock);
    
    m_decoded_buffer = (s16 *)malloc(m_channel_count * m_samples_per_frame * sizeof(s16));
    
    int error;
    m_decoder = opus_multistream_decoder_create(opus_config->sampleRate, opus_config->channelCount, opus_config->streams, opus_config->coupledStreams, opus_config->mapping, &error);
    
    memset(&m_driver, 0, sizeof(m_driver));
    memset(m_wavebufs, 0, sizeof(m_wavebufs));
    
    int mempool_size = (m_buffer_size * BUFFER_COUNT + (AUDREN_MEMPOOL_ALIGNMENT - 1)) &~ (AUDREN_MEMPOOL_ALIGNMENT - 1);
    mempool_ptr = memalign(AUDREN_MEMPOOL_ALIGNMENT, mempool_size);
    
    if (!mempool_ptr) {
        brls::Logger::error("Audren: mempool alloc failed");
        return -1;
    }
    
    Result rc = audrenInitialize(&m_ar_config);
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audrenInitialize: %x", rc);
        return -1;
    }
    
    rc = audrvCreate(&m_driver, &m_ar_config, m_channel_count);
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audrvCreate: %x", rc);
        return -1;
    }
    
    for (int i = 0; i < BUFFER_COUNT; i++) {
        m_wavebufs[i].data_raw = mempool_ptr;
        m_wavebufs[i].size = mempool_size;
        m_wavebufs[i].start_sample_offset = i * m_samples;
        m_wavebufs[i].end_sample_offset = m_wavebufs[i].start_sample_offset + m_samples;
    }
    
    m_current_wavebuf = NULL;
    
    int mpid = audrvMemPoolAdd(&m_driver, mempool_ptr, mempool_size);
    audrvMemPoolAttach(&m_driver, mpid);
    
    audrvDeviceSinkAdd(&m_driver, AUDREN_DEFAULT_DEVICE_NAME, m_channel_count, m_sink_channels);
    
    rc = audrenStartAudioRenderer();
    if (R_FAILED(rc)) {
        brls::Logger::error("Audren: audrenStartAudioRenderer: %x", rc);
    }
    
    audrvVoiceInit(&m_driver, 0, m_channel_count, PcmFormat_Int16, m_sample_rate);
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
    brls::Logger::info("Audren: Cleanup...");
    
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
    
    if (m_inited_driver) {
        m_inited_driver = false;
        audrvVoiceStop(&m_driver, 0);
        audrvClose(&m_driver);
        audrenExit();
    }
    
    brls::Logger::info("Audren: Cleanup done!");
}

void AudrenAudioRenderer::decode_and_play_sample(char *data, int length) {
    if (m_decoder && m_decoded_buffer) {
        if (data != NULL && length > 0) {
            int decoded_samples = opus_multistream_decode(m_decoder, (const unsigned char *)data, length, m_decoded_buffer, m_samples_per_frame, 0);
            
            if (decoded_samples > 0) {
                write_audio(m_decoded_buffer, decoded_samples * m_channel_count * sizeof(s16));
            }
        }
    } else {
        brls::Logger::error("Audren: Invalid call of decode_and_play_sample");
    }
}

int AudrenAudioRenderer::capabilities() {
    return CAPABILITY_DIRECT_SUBMIT;
}

ssize_t AudrenAudioRenderer::free_wavebuf_index() {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (m_wavebufs[i].state == AudioDriverWaveBufState_Free || m_wavebufs[i].state == AudioDriverWaveBufState_Done) {
            return i;
        }
    }
    return -1;
}

size_t AudrenAudioRenderer::append_audio(const void *buf, size_t size) {
    ssize_t index = -1;
    
    if (!m_current_wavebuf) {
        index = free_wavebuf_index();
        if (index == -1) {
            return 0;
        }
        
        m_current_wavebuf = &m_wavebufs[index];
        current_pool_ptr = mempool_ptr + (index * m_buffer_size);
        m_current_size = 0;
    }
    
    if (size > m_buffer_size - m_current_size) {
        size = m_buffer_size - m_current_size;
    }
    
    void *dstbuf = current_pool_ptr + m_current_size;
    memcpy(dstbuf, buf, size);
    armDCacheFlush(dstbuf, size);
    
    m_current_size += size;
    
    if (m_current_size == m_buffer_size) {
        audrvVoiceAddWaveBuf(&m_driver, 0, m_current_wavebuf);
        
        mutexLock(&m_update_lock);
        audrvUpdate(&m_driver);
        mutexUnlock(&m_update_lock);
        
        if (!audrvVoiceIsPlaying(&m_driver, 0)) {
            audrvVoiceStart(&m_driver, 0);
        }
        
        m_current_wavebuf = NULL;
    }
    
    return size;
}

void AudrenAudioRenderer::write_audio(const void *buf, size_t size) {
    if (!m_inited_driver) {
        brls::Logger::error("Audren: Call write_audio without init driver!");
        return;
    }
    
    size_t written = 0;
    
    while (written < size) {
        written += append_audio(buf + written, size - written);
        
        if (written != size) {
            mutexLock(&m_update_lock);
            audrvUpdate(&m_driver);
            mutexUnlock(&m_update_lock);
            audrenWaitFrame();
        }
    }
}
