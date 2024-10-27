#include "FFmpegVideoDecoder.hpp"
#include "AVFrameHolder.hpp"
#include "Settings.hpp"
#include "borealis.hpp"

// Disables the deblocking filter at the cost of image quality
#define DISABLE_LOOP_FILTER 0x1
// Uses the low latency decode flag (disables multithreading)
#define LOW_LATENCY_DECODE 0x2

#define DECODER_BUFFER_SIZE 92 * 1024 * 2

#if defined(PLATFORM_ANDROID)
#include <jni.h>
#include <libavcodec/jni.h>
#include <libavutil/hwcontext_mediacodec.h>

static JavaVM *mJavaVM = NULL;
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
//    av_jni_set_java_vm(vm, NULL);
    return JNI_VERSION_1_4;
}
#endif

FFmpegVideoDecoder::FFmpegVideoDecoder() {
//    AVBufferRef* deviceRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
//    AVHWDeviceContext* ctx = (AVHWDeviceContext*)deviceRef->data;
//    AVMediaCodecDeviceContext* hwctx = (AVMediaCodecDeviceContext*)ctx->hwctx;
////    hwctx->surface = ;
//    av_hwdevice_ctx_init(deviceRef);
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {}

void ffmpegLog(void* ptr, int level, const char* fmt, va_list vargs) {
    std::string message;
    va_list ap_copy;
    va_copy(ap_copy, vargs);
    size_t len = vsnprintf(0, 0, fmt, ap_copy);
    message.resize(len + 1);  // need space for NUL
    vsnprintf(&message[0], len + 1,fmt, vargs);
    message.resize(len);  // remove the NUL
    brls::Logger::debug("FFmpeg [LOG]: {}", message.c_str());
}

int FFmpegVideoDecoder::setup(int video_format, int width, int height,
                              int redraw_rate, void* context, int dr_flags) {
    m_stream_fps = redraw_rate;

    brls::Logger::debug("FFMpeg's AVCodec version: {}.{}.{}", AV_VERSION_MAJOR(avcodec_version()), AV_VERSION_MINOR(avcodec_version()), AV_VERSION_MICRO(avcodec_version()));
    brls::Logger::info(
        "FFmpeg: Setup with format: {}, width: {}, height: {}, fps: {}",
        video_format == VIDEO_FORMAT_H264 ? "H264" : "HEVC", width, height,
        redraw_rate);

    av_log_set_level(AV_LOG_WARNING);
    // av_log_set_callback(&ffmpegLog); // Uncomment to see FFMpeg logs
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)
    avcodec_register_all();
#endif

    m_packet = av_packet_alloc();

    int perf_lvl = LOW_LATENCY_DECODE;

#ifdef PLATFORM_ANDROID
    if (video_format & VIDEO_FORMAT_MASK_H264) {
        m_decoder = avcodec_find_decoder_by_name("h264_mediacodec");
    } else if (video_format & VIDEO_FORMAT_MASK_H265) {
        m_decoder = avcodec_find_decoder_by_name("hevc_mediacodec");
    } else {
        // Unsupported decoder type
    }
#else
    if (video_format & VIDEO_FORMAT_MASK_H264) {
        m_decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
//        m_decoder = avcodec_find_decoder_by_name("h264");
    } else if (video_format & VIDEO_FORMAT_MASK_H265) {
        m_decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
//        m_decoder = avcodec_find_decoder_by_name("hevc");
    } else {
        // Unsupported decoder type
    }
#endif

    if (m_decoder == NULL) {
        brls::Logger::error("FFmpeg: Couldn't find decoder");
        return -1;
    }

    m_decoder_context = avcodec_alloc_context3(m_decoder);
    if (m_decoder_context == NULL) {
        brls::Logger::error("FFmpeg: Couldn't allocate context");
        return -1;
    }

    if (perf_lvl & DISABLE_LOOP_FILTER)
        // Skip the loop filter for performance reasons
        m_decoder_context->skip_loop_filter = AVDISCARD_ALL;

    if (perf_lvl & LOW_LATENCY_DECODE)
        // Use low delay single threaded encoding
        m_decoder_context->flags |= AV_CODEC_FLAG_LOW_DELAY;

    m_decoder_context->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    m_decoder_context->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;

    m_decoder_context->flags2 |= AV_CODEC_FLAG2_FAST;

    int decoder_threads = Settings::instance().decoder_threads();

    if (decoder_threads == 0) {
        m_decoder_context->thread_type = FF_THREAD_FRAME;
    } else {
        m_decoder_context->thread_type = FF_THREAD_SLICE;
        m_decoder_context->thread_count = decoder_threads;
    }

    m_decoder_context->width = width;
    m_decoder_context->height = height;
#ifdef PLATFORM_SWITCH
#ifdef BOREALIS_USE_DEKO3D
   m_decoder_context->pix_fmt = AV_PIX_FMT_NVTEGRA;
#else
   m_decoder_context->pix_fmt = AV_PIX_FMT_NV12;
#endif
#else
    m_decoder_context->pix_fmt = AV_PIX_FMT_NV12;
#endif

    int err = avcodec_open2(m_decoder_context, m_decoder, NULL);
    if (err < 0) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Couldn't open codec - {}", error);
        return err;
    }

    tmp_frame = av_frame_alloc();
    for (int i = 0; i < m_frames_count; i++) {
        m_frames[i] = av_frame_alloc();
        if (m_frames[i] == NULL) {
            brls::Logger::error("FFmpeg: Couldn't allocate frame");
            return -1;
        }

#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
        m_frames[i]->format = AV_PIX_FMT_NVTEGRA;
#elif defined(PLATFORM_ANDROID)
        m_frames[i]->format = AV_PIX_FMT_MEDIACODEC;
#else
        m_frames[i]->format = AV_PIX_FMT_NV12;
#endif
        m_frames[i]->width  = width;
        m_frames[i]->height = height;

// Need to align Switch frame to 256, need to de reviewed
#if defined(PLATFORM_SWITCH) && !defined(BOREALIS_USE_DEKO3D)
        // int err = av_frame_get_buffer(m_frames[i], 256);
        int err = av_frame_get_buffer(m_frames[i], 0);
        if (err < 0) {
            char errs[64]; 
            brls::Logger::error("FFmpeg: Couldn't allocate frame buffer: {}", av_make_error_string(errs, 64, err));
            return -1;
        }

//         for (int j = 0; j < 2; j++) {
//             uintptr_t ptr = (uintptr_t)m_frames[i]->data[j];
//             uintptr_t dst = (((ptr)+(256)-1)&~((256)-1));
//             uintptr_t gap = dst - ptr;
//             m_frames[i]->data[j] += gap;
//         }
#endif
    }

#if defined(PLATFORM_SWITCH) && !defined(BOREALIS_USE_DEKO3D)
    for (int j = 0; j < 2; j++) {
        uintptr_t ptr = (uintptr_t)tmp_frame->data[j];
        uintptr_t dst = (((ptr)+(256)-1)&~((256)-1));
        uintptr_t gap = dst - ptr;
        tmp_frame->data[j] += gap;
    }
#endif

    m_ffmpeg_buffer =
        (char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
    if (m_ffmpeg_buffer == NULL) {
        brls::Logger::error("FFmpeg: Not enough memory");
        cleanup();
        return -1;
    }

#if defined(PLATFORM_SWITCH)
        AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NVTEGRA;
#elif defined(PLATFORM_ANDROID)
        AVHWDeviceType hwType = AV_HWDEVICE_TYPE_MEDIACODEC;
#elif defined(PLATFORM_APPLE)
        AVHWDeviceType hwType = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#else
        AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
#endif

    if (Settings::instance().use_hw_decoding() && hwType != AV_HWDEVICE_TYPE_NONE) {
        if ((err = av_hwdevice_ctx_create(&hw_device_ctx, hwType, NULL, NULL, 0)) < 0) {
            brls::Logger::error("FFmpeg: Error initializing hardware decoder - {}", err);
            return -1;
        }
        m_decoder_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    } else {
        brls::Logger::warning("FFmpeg: HW decoding disabled or unsupported by Platform");
    }

    brls::Logger::info("FFmpeg: Setup done!");
    return DR_OK;
}

void FFmpegVideoDecoder::cleanup() {
    brls::Logger::info("FFmpeg: Cleanup...");

    av_packet_free(&m_packet);

    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
    }

    if (m_decoder_context) {
        avcodec_close(m_decoder_context);
        av_free(m_decoder_context);
        m_decoder_context = NULL;
    }

//    if (m_frames) {
       for (int i = 0; i < m_frames_count; i++) {
        //    if (m_extra_frames[i])
               av_frame_free(&m_frames[i]);
       }

//        free(m_frames);
//        m_frames = nullptr;
//    }

    if (tmp_frame) {
        av_frame_free(&tmp_frame);
    }

    if (m_ffmpeg_buffer) {
        free(m_ffmpeg_buffer);
        m_ffmpeg_buffer = nullptr;
    }

    AVFrameHolder::instance().cleanup();

    brls::Logger::info("FFmpeg: Cleanup done!");
}

int FFmpegVideoDecoder::submit_decode_unit(PDECODE_UNIT decode_unit) {
    if (decode_unit->fullLength < DECODER_BUFFER_SIZE) {
        PLENTRY entry = decode_unit->bufferList;

        if (!m_last_frame) {
            m_video_decode_stats.measurement_start_timestamp = LiGetMillis();
            m_last_frame = decode_unit->frameNumber;
        } else {
            // Any frame number greater than m_LastFrameNumber + 1 represents a
            // dropped frame
            m_video_decode_stats.network_dropped_frames +=
                decode_unit->frameNumber - (m_last_frame + 1);
            m_video_decode_stats.total_frames +=
                decode_unit->frameNumber - (m_last_frame + 1);
            m_last_frame = decode_unit->frameNumber;
        }

        m_video_decode_stats.received_frames++;
        m_video_decode_stats.total_frames++;

        int length = 0;
        while (entry != NULL) {
            if (length > DECODER_BUFFER_SIZE) {
                brls::Logger::error("FFmpeg: Big buffer to decode... !");
            }

            memcpy(m_ffmpeg_buffer + length, entry->data, entry->length);
            length += entry->length;
            entry = entry->next;
        }

        m_video_decode_stats.total_reassembly_time +=
            LiGetMillis() - decode_unit->receiveTimeMs;

        m_frames_in++;

        uint64_t before_decode = LiGetMillis();

        if (length > DECODER_BUFFER_SIZE) {
            brls::Logger::error("FFmpeg: Big buffer to decode...");
        }

        if (decode(m_ffmpeg_buffer, length) == 0) {
            m_frames_out++;
            m_video_decode_stats.total_decode_time +=
                LiGetMillis() - before_decode;

            // Also count the frame-to-frame delay if the decoder is delaying
            // frames until a subsequent frame is submitted.
            m_video_decode_stats.total_decode_time +=
                (m_frames_in - m_frames_out) * (1000 / m_stream_fps);
            m_video_decode_stats.decoded_frames++;

            m_frame = get_frame(true);
            if (m_frame != NULL)
                AVFrameHolder::instance().push(m_frame);
        }
    } else {
        brls::Logger::error("FFmpeg: Big buffer to decode... 2");
    }
    return DR_OK;
}

int FFmpegVideoDecoder::capabilities() const {
    return CAPABILITY_SLICES_PER_FRAME(4) | CAPABILITY_DIRECT_SUBMIT;
}

int FFmpegVideoDecoder::decode(char* indata, int inlen) {
    m_packet->data = (uint8_t*)indata;
    m_packet->size = inlen;

//    m_decoder_context->skip_frame = AVDISCARD_ALL;

    int err = avcodec_send_packet(m_decoder_context, m_packet);
    if (err == AVERROR(EAGAIN)) {
        avcodec_flush_buffers(m_decoder_context);
        brls::Logger::error("FFmpeg: Decode failed - Try again");
        return 0;
    }

    if (err != 0) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Decode failed - {}", error);
        return err;
    }

    return 0;
}

AVFrame* FFmpegVideoDecoder::get_frame(bool native_frame) {
    int err;
#if defined(PLATFORM_ANDROID)
    // Android produce software copied Frame, no need in tmp_hardware frame
    auto decodeFrame = m_frames[m_next_frame];
#else
    // Temp hardware frame to copy into software frame later
    auto decodeFrame = tmp_frame;
#endif
    // Software result frame
    AVFrame* resultFrame = m_frames[m_next_frame];

    RECEIVE_RETRY:
    if ((err = avcodec_receive_frame(m_decoder_context, decodeFrame)) < 0) {
        if (err == AVERROR(EAGAIN)) goto RECEIVE_RETRY;

        char a[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        brls::Logger::error("FFmpeg: Error receiving frame with error {}",  av_make_error_string(a, AV_ERROR_MAX_STRING_SIZE, err));
        return NULL;
    }

    if (hw_device_ctx) {
#if defined(BOREALIS_USE_DEKO3D) || defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
        // DEKO decoder will work with hardware frame
        // Android already produce software Frame
        resultFrame = decodeFrame;
#else
        // Copy hardware frame into software frame
        if ((err = av_hwframe_transfer_data(resultFrame, decodeFrame, 0)) < 0) {
            char a[AV_ERROR_MAX_STRING_SIZE] = { 0 };
            brls::Logger::error("FFmpeg: Error transferring the data to system memory with error {}",  av_make_error_string(a, AV_ERROR_MAX_STRING_SIZE, err));
            return NULL;
        }
        
        av_frame_copy_props(resultFrame, decodeFrame);
#endif
    } else {
        resultFrame = decodeFrame;
    }

    if (err == 0) {
        m_current_frame = m_next_frame;
        m_next_frame = (m_current_frame + 1) % m_frames_count;
        if (/*ffmpeg_decoder == SOFTWARE ||*/ native_frame)
            return resultFrame;
    } else {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Receive failed - %d/%s", err, error);
    }
    return NULL;
}

VideoDecodeStats* FFmpegVideoDecoder::video_decode_stats() {
    uint64_t now = LiGetMillis();
    m_video_decode_stats.total_fps =
        (float)m_video_decode_stats.total_frames /
        ((float)(now - m_video_decode_stats.measurement_start_timestamp) /
         1000);
    m_video_decode_stats.received_fps =
        (float)m_video_decode_stats.received_frames /
        ((float)(now - m_video_decode_stats.measurement_start_timestamp) /
         1000);
    m_video_decode_stats.decoded_fps =
        (float)m_video_decode_stats.decoded_frames /
        ((float)(now - m_video_decode_stats.measurement_start_timestamp) /
         1000);
    return (VideoDecodeStats*)&m_video_decode_stats;
}
