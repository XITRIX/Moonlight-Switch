#include "IAudioRenderer.hpp"
#include "IFFmpegVideoDecoder.hpp"
#include "IVideoRenderer.hpp"
#pragma once

class MoonlightSessionDecoderAndRenderProvider {
  public:
    virtual IFFmpegVideoDecoder* video_decoder() = 0;
    virtual IVideoRenderer* video_renderer() = 0;
    virtual IAudioRenderer* audio_renderer() = 0;
};
