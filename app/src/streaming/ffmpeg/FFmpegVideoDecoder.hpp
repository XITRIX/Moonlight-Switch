#include "IFFmpegVideoDecoder.hpp"
#pragma once

#define m_frames_count 2

class FFmpegVideoDecoder : public IFFmpegVideoDecoder {
  public:
    FFmpegVideoDecoder();
    ~FFmpegVideoDecoder();

    int setup(int video_format, int width, int height, int redraw_rate,
              void* context, int dr_flags) override;
    void cleanup() override;
    int submit_decode_unit(PDECODE_UNIT decode_unit) override;
    int capabilities() const override;
    VideoDecodeStats* video_decode_stats() override;

  private:
    int decode(char* indata, int inlen);
    AVFrame* get_frame(bool native_frame);

    AVPacket* m_packet;
    AVBufferRef *hw_device_ctx = nullptr;
    const AVCodec* m_decoder = nullptr;
    AVCodecContext* m_decoder_context = nullptr;
    AVFrame *tmp_frame = nullptr;
    // AVFrame* m_extra_frames[m_frames_count];
    AVFrame* m_frames[m_frames_count];

    int m_stream_fps = 0;
    int m_frames_in = 0;
    int m_frames_out = 0;
    int m_current_frame = 0, m_next_frame = 0;
    uint32_t m_last_frame = 0;

    VideoDecodeStats m_video_decode_stats_progress = {};
    VideoDecodeStats m_video_decode_stats_cache = {};
    uint64_t timeCount = 0;

    char* m_ffmpeg_buffer = nullptr;
    AVFrame* m_frame = nullptr;
};
