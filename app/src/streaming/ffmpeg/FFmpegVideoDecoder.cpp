#include "FFmpegVideoDecoder.hpp"
#include "AVFrameHolder.hpp"
#include "Settings.hpp"
#include "borealis.hpp"

#ifdef PLATFORM_APPLE
extern "C" {
#include <libavcodec/videotoolbox.h>
}
#endif

// Disables the deblocking filter at the cost of image quality
#define DISABLE_LOOP_FILTER 0x1
// Uses the low latency decode flag (disables multithreading)
#define LOW_LATENCY_DECODE 0x2

//#if defined(PLATFORM_TVOS)
//#define DECODER_BUFFER_SIZE 92 * 1024 * 4
//#else
//#define DECODER_BUFFER_SIZE 92 * 1024 * 2
//#endif
#define DECODER_BUFFER_SIZE (1024 * 1024)

#if defined(PLATFORM_ANDROID)
#include <jni.h>
#include <libavcodec/jni.h>
#include <libavutil/hwcontext_mediacodec.h>

//static JavaVM *mJavaVM = NULL;
//JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
//{
////    av_jni_set_java_vm(vm, NULL);
//    return JNI_VERSION_1_4;
//}
#endif

FFmpegVideoDecoder::FFmpegVideoDecoder() {
//    AVBufferRef* deviceRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
//    AVHWDeviceContext* ctx = (AVHWDeviceContext*)deviceRef->data;
//    AVMediaCodecDeviceContext* hwctx = (AVMediaCodecDeviceContext*)ctx->hwctx;
////    hwctx->surface = ;
//    av_hwdevice_ctx_init(deviceRef);
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() = default;

void ffmpegLog(void* ptr, int level, const char* fmt, va_list vargs) {
    std::string message;
    va_list ap_copy;
    va_copy(ap_copy, vargs);
    size_t len = vsnprintf(nullptr, 0, fmt, ap_copy);
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
    } else if (video_format & VIDEO_FORMAT_MASK_H265) {
        m_decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    } else {
        // Unsupported decoder type
    }
#endif

    if (m_decoder == nullptr) {
        brls::Logger::error("FFmpeg: Couldn't find decoder");
        return -1;
    }

    m_decoder_context = avcodec_alloc_context3(m_decoder);
    if (m_decoder_context == nullptr) {
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

    if (decoder_threads == 0 || Settings::instance().use_hw_decoding()) {
        m_decoder_context->thread_type = FF_THREAD_FRAME;
        m_decoder_context->thread_count = 1;
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
//    m_decoder_context->pix_fmt = AV_PIX_FMT_NV12;
#endif

    int err = avcodec_open2(m_decoder_context, m_decoder, nullptr);
    if (err < 0) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Couldn't open codec - {}", error);
        return err;
    }

    AVFrameHolder::instance().prepare();

    // One extra frame for decoding processing
    m_frames_size = Settings::instance().frames_queue_size() + 1;
    m_frames = new AVFrame*[m_frames_size];

    tmp_frame = av_frame_alloc();
    for (int i = 0; i < m_frames_size; i++) {
        auto& frame = m_frames[i];
        frame = av_frame_alloc();
        if (frame == nullptr) {
            brls::Logger::error("FFmpeg: Couldn't allocate frame");
            return -1;
        }

#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
        frame->format = AV_PIX_FMT_NVTEGRA;
#elif defined(PLATFORM_ANDROID)
        frame->format = AV_PIX_FMT_MEDIACODEC;
#else
        if (video_format & VIDEO_FORMAT_MASK_10BIT)
            frame->format = AV_PIX_FMT_P010;
        else
            frame->format = AV_PIX_FMT_NV12;
#endif
        frame->width  = width;
        frame->height = height;

// Need to align Switch frame to 256, need to de reviewed
#if defined(PLATFORM_SWITCH) && !defined(BOREALIS_USE_DEKO3D)
        int err = av_frame_get_buffer(frame, 256);
        if (err < 0) {
            char errs[64]; 
            brls::Logger::error("FFmpeg: Couldn't allocate frame buffer: {}", av_make_error_string(errs, 64, err));
            return -1;
        }

        for (int j = 0; j < 2; j++) {
            uintptr_t ptr = (uintptr_t)frame->data[j];
            uintptr_t dst = (((ptr)+(256)-1)&~((256)-1));
            uintptr_t gap = dst - ptr;
            frame->data[j] += gap;
        }
#endif
    }

    m_ffmpeg_buffer =
        (char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
    if (m_ffmpeg_buffer == nullptr) {
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
        if ((err = av_hwdevice_ctx_create(&hw_device_ctx, hwType, nullptr, nullptr, 0)) < 0) {
            char error[512];
            av_strerror(err, error, sizeof(error));
            brls::Logger::error("FFmpeg: Error initializing hardware decoder - {}", error);
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
        m_decoder_context = nullptr;
    }

//    if (m_frames) {
       for (int i = 0; i < m_frames_size; i++) {
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
    delete[] m_frames;

    brls::Logger::info("FFmpeg: Cleanup done!");
}

int FFmpegVideoDecoder::submit_decode_unit(PDECODE_UNIT decode_unit) {
    if (decode_unit->fullLength < DECODER_BUFFER_SIZE) {
        PLENTRY entry = decode_unit->bufferList;

        if (m_video_decode_stats_progress.measurement_start_timestamp == 0) {
            m_video_decode_stats_progress.measurement_start_timestamp = LiGetMillis();
        }

        if (!m_last_frame) {
            m_last_frame = decode_unit->frameNumber;
        } else {
            // Any frame number greater than m_LastFrameNumber + 1 represents a
            // dropped frame
            m_video_decode_stats_progress.network_dropped_frames +=
                decode_unit->frameNumber - (m_last_frame + 1);
            m_video_decode_stats_progress.total_frames +=
                decode_unit->frameNumber - (m_last_frame + 1);
            m_last_frame = decode_unit->frameNumber;
        }

        m_video_decode_stats_progress.current_received_frames++;
        m_video_decode_stats_progress.total_frames++;

        int length = 0;
        while (entry != nullptr) {
            if (length > DECODER_BUFFER_SIZE) {
                brls::Logger::error("FFmpeg: Big buffer to decode... !");
            }

            memcpy(m_ffmpeg_buffer + length, entry->data, entry->length);
            length += entry->length;
            entry = entry->next;
        }

        m_video_decode_stats_progress.current_reassembly_time += LiGetMillis() - decode_unit->receiveTimeMs;
        m_frames_in++;

        uint64_t before_decode = LiGetMillis();

        if (length > DECODER_BUFFER_SIZE) {
            brls::Logger::error("FFmpeg: Big buffer to decode...");
        }

        if (decode(m_ffmpeg_buffer, length) == 0) {
            m_frames_out++;

            auto decodeTime = LiGetMillis() - before_decode;
            m_video_decode_stats_progress.current_decode_time += decodeTime;

            // Also count the frame-to-frame delay if the decoder is delaying
            // frames until a subsequent frame is submitted.
            m_video_decode_stats_progress.current_decode_time +=
                (m_frames_in - m_frames_out) * (1000 / m_stream_fps);
            m_video_decode_stats_progress.current_decoded_frames++;

            const int time_interval = 60;
            timeCount += decodeTime;
            if (timeCount >= time_interval) {
                // brls::Logger::debug("FPS: {}", frames / 5.0f);

                m_video_decode_stats_cache = m_video_decode_stats_progress;
                m_video_decode_stats_progress = {};

                // Preserve dropped frames count
                m_video_decode_stats_progress.total_received_frames = m_video_decode_stats_cache.total_received_frames + m_video_decode_stats_cache.current_received_frames;
                m_video_decode_stats_progress.total_decoded_frames = m_video_decode_stats_cache.total_decoded_frames + m_video_decode_stats_cache.current_decoded_frames;
                m_video_decode_stats_progress.total_reassembly_time = m_video_decode_stats_cache.total_reassembly_time + m_video_decode_stats_cache.current_reassembly_time;
                m_video_decode_stats_progress.total_decode_time = m_video_decode_stats_cache.total_decode_time + m_video_decode_stats_cache.current_decode_time;

                m_video_decode_stats_progress.network_dropped_frames = m_video_decode_stats_cache.network_dropped_frames;

                uint64_t now = LiGetMillis();
                m_video_decode_stats_cache.current_host_fps =
                    (float)m_video_decode_stats_cache.total_frames /
                    ((float)(now - m_video_decode_stats_cache.measurement_start_timestamp) /
                    1000);
                m_video_decode_stats_cache.current_received_fps =
                        (float)m_video_decode_stats_cache.current_received_frames /
                        ((float)(now - m_video_decode_stats_cache.measurement_start_timestamp) /
                    1000);
                m_video_decode_stats_cache.current_decoded_fps =
                        (float)m_video_decode_stats_cache.current_decoded_frames /
                        ((float)(now - m_video_decode_stats_cache.measurement_start_timestamp) /
                    1000);

                m_video_decode_stats_cache.current_receive_time = (float) m_video_decode_stats_cache.current_reassembly_time /
                                                                  (float) m_video_decode_stats_cache.current_received_frames;
                m_video_decode_stats_cache.current_decoding_time = (float) m_video_decode_stats_cache.current_decode_time /
                                                                   (float) m_video_decode_stats_cache.current_decoded_frames;

                m_video_decode_stats_cache.session_receive_time = (float) m_video_decode_stats_cache.total_reassembly_time /
                                                                  (float) m_video_decode_stats_cache.total_received_frames;
                m_video_decode_stats_cache.session_decoding_time = (float) m_video_decode_stats_cache.total_decode_time /
                                                                   (float) m_video_decode_stats_cache.total_decoded_frames;

                timeCount -= time_interval;
            }

            m_frame = get_frame(true);
            if (m_frame != nullptr)
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
        return nullptr;
    }

    if (hw_device_ctx) {
#if defined(BOREALIS_USE_DEKO3D) || defined(PLATFORM_ANDROID) || defined(USE_METAL_RENDERER)
        // DEKO decoder will work with hardware frame
        // Android already produce software Frame
        resultFrame = decodeFrame;
#else

#if defined(PLATFORM_SWITCH) && !defined(BOREALIS_USE_DEKO3D)
        for (int i = 0; i < 2; ++i) {
            if (((uintptr_t)resultFrame->data[i] & 0xff) || (resultFrame->linesize[i] & 0xff)) {
                brls::Logger::error("Frame address/pitch not aligned to 256, falling back to cpu transfer");
                break;
            }
        }
#endif

        // Copy hardware frame into software frame
        if ((err = av_hwframe_transfer_data(resultFrame, decodeFrame, 0)) < 0) {
            char a[AV_ERROR_MAX_STRING_SIZE] = { 0 };
            brls::Logger::error("FFmpeg: Error transferring the data to system memory with error {}",  av_make_error_string(a, AV_ERROR_MAX_STRING_SIZE, err));
            return nullptr;
        }
        
        av_frame_copy_props(resultFrame, decodeFrame);
#endif
    } else {
        resultFrame = decodeFrame;
    }

    if (err == 0) {
        m_current_frame = m_next_frame;
        m_next_frame = (m_current_frame + 1) % m_frames_size;
        if (/*ffmpeg_decoder == SOFTWARE ||*/ native_frame)
            return resultFrame;
    } else {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Receive failed - %d/%s", err, error);
    }
    return nullptr;
}

VideoDecodeStats* FFmpegVideoDecoder::video_decode_stats() {
    return (VideoDecodeStats*)&m_video_decode_stats_cache;
}
