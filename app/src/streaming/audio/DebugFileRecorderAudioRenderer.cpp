#include "DebugFileRecorderAudioRenderer.hpp"
#include <cstdlib>

#define MAX_CHANNEL_COUNT 6
#define FRAME_SIZE 240

DebugFileRecorderAudioRenderer::~DebugFileRecorderAudioRenderer() { cleanup(); }

int DebugFileRecorderAudioRenderer::init(
    int audio_configuration, const POPUS_MULTISTREAM_CONFIGURATION opus_config,
    void* context, int ar_flags) {
    int error;
    m_decoder = opus_multistream_decoder_create(
        opus_config->sampleRate, opus_config->channelCount,
        opus_config->streams, opus_config->coupledStreams, opus_config->mapping,
        &error);
    m_buffer = (short*)malloc(FRAME_SIZE * MAX_CHANNEL_COUNT * sizeof(short));
    return DR_OK;
}

void DebugFileRecorderAudioRenderer::cleanup() {
    if (m_decoder) {
        opus_multistream_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }

    if (m_buffer) {
        free(m_buffer);
        m_buffer = nullptr;
    }

    if (m_enable) {
        m_data.write_to_file(
            "/Users/rock88/Documents/Projects/RetroArch/audio.raw");
    }
}

void DebugFileRecorderAudioRenderer::decode_and_play_sample(char* data,
                                                            int length) {
    int decode_len = opus_multistream_decode(
        m_decoder, (const unsigned char*)data, length, m_buffer, FRAME_SIZE, 0);
    if (decode_len > 0 && m_enable) {
        m_data = m_data.append(
            Data((char*)m_buffer, FRAME_SIZE * 2 * sizeof(short)));
    }
}

int DebugFileRecorderAudioRenderer::capabilities() {
    return CAPABILITY_DIRECT_SUBMIT;
}
