#include <Limelight.h>
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

struct VideoDecodeStats {
    uint32_t received_frames;
    uint32_t decoded_frames;
    uint32_t total_frames;
    uint32_t network_dropped_frames;
    uint32_t total_reassembly_time;
    uint32_t total_decode_time;
    float total_fps;
    float received_fps;
    float decoded_fps;
    uint64_t measurement_start_timestamp;
};

class IFFmpegVideoDecoder {
  public:
    virtual ~IFFmpegVideoDecoder(){};
    virtual int setup(int video_format, int width, int height, int redraw_rate,
                      void* context, int dr_flags) = 0;
    virtual void start(){};
    virtual void stop(){};
    virtual void cleanup() = 0;
    virtual int submit_decode_unit(PDECODE_UNIT decode_unit) = 0;
    virtual int capabilities() const = 0;
    virtual VideoDecodeStats* video_decode_stats() = 0;
};
