#include "SwitchMoonlightSessionDecoderAndRenderProvider.hpp"
#include "FFmpegVideoDecoder.hpp"
#ifdef __SWITCH__
#include "AudrenAudioRenderer.hpp"
//#else
//#include "GLVideoRenderer.hpp"
#endif
#include "DKVideoRenderer.hpp"
#include "DebugFileRecorderAudioRenderer.hpp"

IFFmpegVideoDecoder* SwitchMoonlightSessionDecoderAndRenderProvider::video_decoder() {
    return new FFmpegVideoDecoder();
}

IVideoRenderer* SwitchMoonlightSessionDecoderAndRenderProvider::video_renderer() {
//    #ifdef __SWITCH__
    return new DKVideoRenderer();
//    #else
//    return new GLVideoRenderer();
//    #endif
}

IAudioRenderer* SwitchMoonlightSessionDecoderAndRenderProvider::audio_renderer() {
    #ifdef __SWITCH__
    return new AudrenAudioRenderer();
    #else
    return new DebugFileRecorderAudioRenderer(false);
    #endif
}
