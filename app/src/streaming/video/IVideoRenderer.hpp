#pragma once

#include <Limelight.h>
#include <nanovg.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

struct VideoRenderStats {
    uint32_t rendered_frames;
    uint64_t total_render_time;
    float rendered_fps;
    double measurement_start_timestamp;
};

class IVideoRenderer {
  public:
    virtual ~IVideoRenderer(){};
    virtual void draw(NVGcontext* vg, int width, int height,
                      AVFrame* frame, int imageFormat) = 0;
    virtual VideoRenderStats* video_render_stats() = 0;

    // Default implementations
    virtual int getDecoderColorspace() {
        // Rec 601 is default
        return COLORSPACE_REC_601;
    }

    virtual int getDecoderColorRange() {
        // Limited is the default
        return COLOR_RANGE_LIMITED;
    }

    virtual int getFrameColorspace(const AVFrame* frame) {
        // Prefer the colorspace field on the AVFrame itself
        switch (frame->colorspace) {
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            return COLORSPACE_REC_601;
        case AVCOL_SPC_BT709:
            return COLORSPACE_REC_709;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            return COLORSPACE_REC_2020;
        default:
            // If the colorspace is not populated, assume the encoder
            // is sending the colorspace that we requested.
            return getDecoderColorspace();
        }
    }

    virtual bool isFrameFullRange(const AVFrame* frame) {
        // This handles the case where the color range is unknown,
        // so that we use Limited color range which is the default
        // behavior for Moonlight.
        return frame->color_range == AVCOL_RANGE_JPEG;
    }
};
