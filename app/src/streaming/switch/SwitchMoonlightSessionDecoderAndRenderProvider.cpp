#include "SwitchMoonlightSessionDecoderAndRenderProvider.hpp"
#include "FFmpegVideoDecoder.hpp"
#ifdef __SWITCH2__
#include "Switch2VideoDecoder.hpp"
#endif

#ifdef __SWITCH__
#include "AudrenAudioRenderer.hpp"
#endif

#if defined(__SDL2__) || defined(__SDL3__)
#include "SDLAudiorenderer.hpp"
#endif

#ifdef USE_VULKAN_RENDERER
#include "VulkanVideoRenderer.hpp"
#elif defined(BOREALIS_USE_DEKO3D)
#include "DKVideoRenderer.hpp"
#elif defined(PLATFORM_ANDROID)
#include "../video/Android/AndroidMediaCodecVideoRenderer.hpp"
#elif defined(USE_D3D11_RENDERER)
#include "D3D11VideoRenderer.hpp"
#elif defined(USE_METAL_RENDERER)
#include "MetalVideoRenderer.hpp"
#elif defined(USE_GL_RENDERER)
#include "GLVideoRenderer.hpp"
#else
#error No renderer selected, enable USE_VULKAN_RENDERER, USE_GL_RENDERER, USE_D3D11_RENDERER, or USE_METAL_RENDERER
#endif

IFFmpegVideoDecoder*
SwitchMoonlightSessionDecoderAndRenderProvider::video_decoder() {
#ifdef __SWITCH2__
    return new Switch2VideoDecoder();
#else
    return new FFmpegVideoDecoder();
#endif
}

IVideoRenderer*
SwitchMoonlightSessionDecoderAndRenderProvider::video_renderer() {
#ifdef USE_VULKAN_RENDERER
    return new VulkanVideoRenderer();
#elif defined(BOREALIS_USE_DEKO3D)
    return new DKVideoRenderer();
#elif defined(PLATFORM_ANDROID)
    return new AndroidMediaCodecVideoRenderer();
#elif defined(USE_D3D11_RENDERER)
    return new D3D11VideoRenderer();
#elif defined(USE_METAL_RENDERER)
    return new MetalVideoRenderer();
#elif defined(USE_GL_RENDERER)
    return new GLVideoRenderer();
#endif
}

IAudioRenderer*
SwitchMoonlightSessionDecoderAndRenderProvider::audio_renderer() {
#ifdef __SWITCH__
    return new AudrenAudioRenderer();
#elif defined(__SDL2__) || defined(__SDL3__)
    return new SDLAudioRenderer();
#else
#error No audio renderer selected
#endif
}
