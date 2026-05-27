#ifdef PLATFORM_ANDROID

#include "FFmpegVideoDecoderPlatformHelpers.hpp"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/hwcontext_mediacodec.h>
}

#include <SDL2/SDL.h>
#include <jni.h>

#include "borealis.hpp"

namespace ffmpeg::decoder {

namespace {

constexpr const char* PLATFORM_UTILS_CLASS_NAME = "org.libsdl.app.PlatformUtils";
constexpr Uint32 MEDIA_CODEC_SURFACE_WAIT_TIMEOUT_MS = 500;
constexpr Uint32 MEDIA_CODEC_SURFACE_WAIT_INTERVAL_MS = 10;

jclass findPlatformUtilsClass(JNIEnv* env) {
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
    if (activity == nullptr) {
        brls::Logger::warning("FFmpeg: SDL didn't provide an Android activity while resolving PlatformUtils");
        return nullptr;
    }

    jclass activityClass = env->GetObjectClass(activity);
    if (activityClass == nullptr) {
        env->DeleteLocalRef(activity);
        brls::Logger::warning("FFmpeg: Couldn't inspect Android activity class while resolving PlatformUtils");
        return nullptr;
    }

    jmethodID getClassLoaderMethod = env->GetMethodID(activityClass,
                                                      "getClassLoader",
                                                      "()Ljava/lang/ClassLoader;");
    if (getClassLoaderMethod == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
        brls::Logger::warning("FFmpeg: Android activity class loader is unavailable while resolving PlatformUtils");
        return nullptr;
    }

    jobject classLoader = env->CallObjectMethod(activity, getClassLoaderMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        classLoader = nullptr;
    }
    if (classLoader == nullptr) {
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
        brls::Logger::warning("FFmpeg: Couldn't obtain Android class loader while resolving PlatformUtils");
        return nullptr;
    }

    // Decoder threads can't reliably use FindClass() for app classes, so resolve
    // PlatformUtils through the activity's class loader instead.
    jclass classLoaderClass = env->GetObjectClass(classLoader);
    if (classLoaderClass == nullptr) {
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
        brls::Logger::warning("FFmpeg: Java ClassLoader class unavailable while resolving PlatformUtils");
        return nullptr;
    }

    jmethodID loadClassMethod = env->GetMethodID(classLoaderClass,
                                                 "loadClass",
                                                 "(Ljava/lang/String;)Ljava/lang/Class;");
    if (loadClassMethod == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(classLoaderClass);
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
        brls::Logger::warning("FFmpeg: Java class loader can't load PlatformUtils");
        return nullptr;
    }

    jstring className = env->NewStringUTF(PLATFORM_UTILS_CLASS_NAME);
    if (className == nullptr) {
        env->DeleteLocalRef(classLoaderClass);
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
        brls::Logger::warning("FFmpeg: Couldn't allocate PlatformUtils class name for JNI lookup");
        return nullptr;
    }

    auto loadedClass = reinterpret_cast<jclass>(env->CallObjectMethod(classLoader,
                                                                      loadClassMethod,
                                                                      className));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        loadedClass = nullptr;
    }

    env->DeleteLocalRef(className);
    env->DeleteLocalRef(classLoaderClass);
    env->DeleteLocalRef(classLoader);
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);

    if (loadedClass == nullptr) {
        brls::Logger::warning("FFmpeg: Android activity class loader couldn't resolve PlatformUtils");
    }

    return loadedClass;
}

void callSurfaceSizeUpdate(JNIEnv* env, int width, int height) {
    jclass utilsClass = findPlatformUtilsClass(env);
    if (utilsClass == nullptr) {
        brls::Logger::warning("FFmpeg: Android PlatformUtils class not found while updating MediaCodec surface size");
        env->ExceptionClear();
        return;
    }

    jmethodID method = env->GetStaticMethodID(utilsClass,
                                              "setMediaCodecVideoSurfaceContentSize",
                                              "(II)V");
    if (method == nullptr) {
        brls::Logger::warning("FFmpeg: Android PlatformUtils.setMediaCodecVideoSurfaceContentSize() not available");
        env->ExceptionClear();
        env->DeleteLocalRef(utilsClass);
        return;
    }

    env->CallStaticVoidMethod(utilsClass, method, width, height);
    if (env->ExceptionCheck()) {
        brls::Logger::warning("FFmpeg: Failed to update Android MediaCodec surface size");
        env->ExceptionClear();
    }

    env->DeleteLocalRef(utilsClass);
}

jobject acquireSurfaceLocalRef(JNIEnv* env) {
    jclass utilsClass = findPlatformUtilsClass(env);
    if (utilsClass == nullptr) {
        brls::Logger::warning("FFmpeg: Android PlatformUtils class not found while acquiring MediaCodec surface");
        env->ExceptionClear();
        return nullptr;
    }

    jmethodID method = env->GetStaticMethodID(utilsClass,
                                              "getMediaCodecVideoSurface",
                                              "()Landroid/view/Surface;");
    if (method == nullptr) {
        brls::Logger::warning("FFmpeg: Android PlatformUtils.getMediaCodecVideoSurface() not available");
        env->ExceptionClear();
        env->DeleteLocalRef(utilsClass);
        return nullptr;
    }

    jobject surface = env->CallStaticObjectMethod(utilsClass, method);
    if (env->ExceptionCheck()) {
        brls::Logger::warning("FFmpeg: Exception while acquiring Android MediaCodec surface");
        env->ExceptionClear();
        surface = nullptr;
    }

    env->DeleteLocalRef(utilsClass);
    return surface;
}

jobject waitForSurfaceLocalRef(JNIEnv* env) {
    jobject surface = acquireSurfaceLocalRef(env);
    if (surface != nullptr) {
        return surface;
    }

    const Uint32 waitStart = SDL_GetTicks();
    while (SDL_GetTicks() - waitStart < MEDIA_CODEC_SURFACE_WAIT_TIMEOUT_MS) {
        SDL_Delay(MEDIA_CODEC_SURFACE_WAIT_INTERVAL_MS);

        surface = acquireSurfaceLocalRef(env);
        if (surface != nullptr) {
            brls::Logger::info("FFmpeg: Android MediaCodec surface became available after {} ms",
                              SDL_GetTicks() - waitStart);
            return surface;
        }
    }

    brls::Logger::error("FFmpeg: Dedicated Android MediaCodec surface did not become available within {} ms",
                        MEDIA_CODEC_SURFACE_WAIT_TIMEOUT_MS);
    return nullptr;
}

} // namespace

int initializeAndroidMediaCodecHardwareDevice(AndroidMediaCodecState& state,
                                              AVBufferRef*& hw_device_ctx,
                                              int width,
                                              int height) {
    cleanupAndroidMediaCodecState(state);

    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (env == nullptr) {
        brls::Logger::error("FFmpeg: SDL didn't provide a JNI environment for MediaCodec setup");
        return AVERROR_EXTERNAL;
    }

    callSurfaceSizeUpdate(env, width, height);

    jobject surfaceLocalRef = waitForSurfaceLocalRef(env);
    if (surfaceLocalRef == nullptr) {
        brls::Logger::error("FFmpeg: Dedicated Android MediaCodec surface is unavailable");
        return AVERROR(EINVAL);
    }

    jobject surfaceGlobalRef = env->NewGlobalRef(surfaceLocalRef);
    env->DeleteLocalRef(surfaceLocalRef);

    if (surfaceGlobalRef == nullptr) {
        brls::Logger::error("FFmpeg: Failed to retain Android MediaCodec surface reference");
        return AVERROR(ENOMEM);
    }

    hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    if (hw_device_ctx == nullptr) {
        env->DeleteGlobalRef(surfaceGlobalRef);
        brls::Logger::error("FFmpeg: Couldn't allocate MediaCodec hardware device context");
        return AVERROR(ENOMEM);
    }

    auto* deviceContext = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
    auto* mediacodecContext = reinterpret_cast<AVMediaCodecDeviceContext*>(deviceContext->hwctx);
    mediacodecContext->surface = surfaceGlobalRef;
    mediacodecContext->native_window = nullptr;
    mediacodecContext->create_window = 0;

    const int err = av_hwdevice_ctx_init(hw_device_ctx);
    if (err < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE] = {0};
        brls::Logger::error("FFmpeg: Couldn't initialize MediaCodec hardware device - {}",
                            av_make_error_string(error, sizeof(error), err));
        env->DeleteGlobalRef(surfaceGlobalRef);
        av_buffer_unref(&hw_device_ctx);
        return err;
    }

    state.surfaceGlobalRef = surfaceGlobalRef;
    return 0;
}

void cleanupAndroidMediaCodecState(AndroidMediaCodecState& state) {
    if (state.surfaceGlobalRef == nullptr) {
        return;
    }

    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (env != nullptr) {
        env->DeleteGlobalRef(reinterpret_cast<jobject>(state.surfaceGlobalRef));
    }

    state.surfaceGlobalRef = nullptr;
}

bool useAndroidDirectHardwareFrames(bool hw_decode_active) {
    return hw_decode_active;
}

} // namespace ffmpeg::decoder

#endif