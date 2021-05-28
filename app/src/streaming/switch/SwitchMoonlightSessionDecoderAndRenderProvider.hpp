#include "MoonlightSession.hpp"

class SwitchMoonlightSessionDecoderAndRenderProvider: public MoonlightSessionDecoderAndRenderProvider {
public:
    SwitchMoonlightSessionDecoderAndRenderProvider() {}
    
    IFFmpegVideoDecoder* video_decoder();
    IVideoRenderer* video_renderer();
    IAudioRenderer* audio_renderer();
};
