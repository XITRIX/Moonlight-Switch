#include "SwitchMoonlightSessionDecoderAndRenderProvider.hpp"
#include "FFmpegVideoDecoder.hpp"
#include "GLVideoRenderer.hpp"
//#ifdef __SWITCH__
//#include "AudrenAudioRenderer.hpp"
//#else
#include "SDLAudiorenderer.hpp"
//#endif

IFFmpegVideoDecoder*
SwitchMoonlightSessionDecoderAndRenderProvider::video_decoder() {
    return new FFmpegVideoDecoder();
}

IVideoRenderer*
SwitchMoonlightSessionDecoderAndRenderProvider::video_renderer() {
    //    #ifdef __SWITCH__
    // return new DKVideoRenderer();
    //    #else
    return new GLVideoRenderer();
    //    #endif
}

IAudioRenderer*
SwitchMoonlightSessionDecoderAndRenderProvider::audio_renderer() {
    //#ifdef __SWITCH__
    //    return new AudrenAudioRenderer();
    //#else
    return new SDLAudioRenderer();
    //#endif
}
