#include "FFmpegVideoDecoder.hpp"
#include "AVFrameHolder.hpp"
#include "FFmpegVideoDecoderPlatformHelpers.hpp"
#include "Settings.hpp"
#include "borealis.hpp"
#include "MoonlightSession.hpp"

#if defined(_WIN32)
#include <SDL.h>
#endif

extern "C" {
#include <libavutil/hwcontext.h>
}

// Disables the deblocking filter at the cost of image quality
#define DISABLE_LOOP_FILTER 0x1
// Requests low latency decode behavior
#define LOW_LATENCY_DECODE 0x2

//#if defined(PLATFORM_TVOS)
//#define DECODER_BUFFER_SIZE 92 * 1024 * 4
//#else
//#define DECODER_BUFFER_SIZE 92 * 1024 * 2
//#endif
#define DECODER_BUFFER_SIZE (1024 * 1024)

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

int FFmpegVideoDecoder::configure_decoder_context(bool enable_hw_decode,
                                                  bool enable_low_delay,
                                                  bool enable_decoder_threads) {
    if (m_decoder_context != nullptr) {
        avcodec_free_context(&m_decoder_context);
    }

    m_decoder_context = avcodec_alloc_context3(m_decoder);
    if (m_decoder_context == nullptr) {
        brls::Logger::error("FFmpeg: Couldn't allocate context");
        return AVERROR(ENOMEM);
    }

    m_decoder_context->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    m_decoder_context->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;
    m_decoder_context->flags2 |= AV_CODEC_FLAG2_FAST;

    if (m_perf_level & DISABLE_LOOP_FILTER) {
        m_decoder_context->skip_loop_filter = AVDISCARD_ALL;
    }

    if (enable_hw_decode && hw_device_ctx != nullptr) {
        m_decoder_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        if (m_decoder_context->hw_device_ctx == nullptr) {
            brls::Logger::error("FFmpeg: Couldn't retain hardware decoder context");
            return AVERROR(ENOMEM);
        }

#if defined(USE_D3D11_RENDERER)
        ffmpeg::decoder::configureD3D11DecoderContext(m_d3d11, m_decoder_context);
#endif
    }

    if (enable_low_delay) {
        // Use low delay only when decoding stays effectively single threaded.
        m_decoder_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
    }

    if (enable_decoder_threads) {
        m_decoder_context->thread_type = FF_THREAD_SLICE;
        m_decoder_context->thread_count = m_decoder_threads_setting;
    } else {
        m_decoder_context->thread_type = FF_THREAD_FRAME;
        m_decoder_context->thread_count = 1;
    }

    m_decoder_context->width = m_video_width;
    m_decoder_context->height = m_video_height;
#if defined(PLATFORM_SWITCH)
#ifdef BOREALIS_USE_DEKO3D
    if (ffmpeg::decoder::configureDeko3DDecoderContext(m_decoder_context, enable_hw_decode) < 0) {
        return AVERROR(EINVAL);
    }
#else
    m_decoder_context->pix_fmt = AV_PIX_FMT_NV12;
#endif
#endif

#if defined(PLATFORM_ANDROID)
    if (!m_pending_extradata.empty()) {
        m_decoder_context->extradata = static_cast<uint8_t*>(
            av_mallocz(m_pending_extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (m_decoder_context->extradata == nullptr) {
            brls::Logger::error("FFmpeg: Couldn't allocate decoder extradata");
            return AVERROR(ENOMEM);
        }

        memcpy(m_decoder_context->extradata, m_pending_extradata.data(),
               m_pending_extradata.size());
        m_decoder_context->extradata_size =
            static_cast<int>(m_pending_extradata.size());
    }
#endif

    return 0;
}

int FFmpegVideoDecoder::open_decoder() {
    auto log_decoder_attempt = [&]() {
        brls::Logger::info(
            "FFmpeg: Decoder threading mode: hw={} threads={} low_delay={} thread_type={} slice_support={} decoder={}",
            m_hw_decode_active ? "on" : "off",
            m_decoder_context->thread_count,
            m_use_low_delay ? "on" : "off",
            m_use_decoder_threads ? "slice" : "single",
            m_supports_slice_threading ? "on" : "off",
            m_decoder != nullptr && m_decoder->name != nullptr ? m_decoder->name : "unknown");
    };

    int err = configure_decoder_context(m_hw_decode_active, m_use_low_delay,
                                        m_use_decoder_threads);
    if (err < 0) {
        return err;
    }

    log_decoder_attempt();
    err = avcodec_open2(m_decoder_context, m_decoder, nullptr);
#if defined(PLATFORM_ANDROID)
    if (err < 0 && m_using_android_mediacodec_decoder && m_use_low_delay) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::warning(
            "FFmpeg: Couldn't open codec with Android low-latency mode - {}. Retrying without low-latency mode",
            error);

        m_use_low_delay = false;
        err = configure_decoder_context(m_hw_decode_active, m_use_low_delay,
                                        m_use_decoder_threads);
        if (err < 0) {
            return err;
        }

        log_decoder_attempt();
        err = avcodec_open2(m_decoder_context, m_decoder, nullptr);
    }

    if (err < 0 && m_using_android_mediacodec_decoder &&
        m_codec_id != AV_CODEC_ID_NONE) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::warning(
            "FFmpeg: Couldn't open Android MediaCodec decoder - {}. Falling back to software decoding",
            error);

        ffmpeg::decoder::cleanupAndroidMediaCodecState(m_android_mediacodec);
        av_buffer_unref(&hw_device_ctx);

        m_decoder = avcodec_find_decoder(m_codec_id);
        if (m_decoder == nullptr) {
            brls::Logger::error(
                "FFmpeg: Couldn't find software decoder for codec id {}",
                static_cast<int>(m_codec_id));
            return err;
        }

        m_hw_decode_active = false;
        m_using_android_mediacodec_decoder = false;
        m_use_decoder_threads =
            m_decoder_threads_setting > 1 && m_supports_slice_threading;
        m_use_low_delay =
            (m_perf_level & LOW_LATENCY_DECODE) && !m_use_decoder_threads;

        err = configure_decoder_context(m_hw_decode_active, m_use_low_delay,
                                        m_use_decoder_threads);
        if (err < 0) {
            return err;
        }

        log_decoder_attempt();
        err = avcodec_open2(m_decoder_context, m_decoder, nullptr);
    }
#endif

    if (err < 0) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Couldn't open codec - {}", error);
        return err;
    }

    m_decoder_ready = true;
    brls::Logger::info(
        "FFmpeg: Active decoder threading: requested_threads={} active_type={}",
        m_decoder_context->thread_count, m_decoder_context->active_thread_type);
    return 0;
}

int FFmpegVideoDecoder::finalize_decoder_setup() {
    if (m_decoder_finalized) {
        return 0;
    }

    m_use_zero_copy_holder = false;
#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
    m_use_zero_copy_holder = ffmpeg::decoder::useDeko3DZeroCopyHolder(m_hw_decode_active);
#elif defined(PLATFORM_ANDROID)
    m_use_zero_copy_holder = ffmpeg::decoder::useAndroidDirectHardwareFrames(m_hw_decode_active);
#elif defined(USE_D3D11_RENDERER)
    m_use_zero_copy_holder = ffmpeg::decoder::useD3D11ZeroCopyHolder(m_d3d11);
#endif
    AVFrameHolder::instance().prepare(m_use_zero_copy_holder);

    if (!m_use_zero_copy_holder) {
        m_frames_size = Settings::instance().frames_queue_size() + 1;
        m_frames = new AVFrame*[m_frames_size];

        tmp_frame = av_frame_alloc();
        for (int i = 0; i < m_frames_size; i++) {
            auto& frame = m_frames[i];
            frame = av_frame_alloc();
            if (frame == nullptr) {
                brls::Logger::error("FFmpeg: Couldn't allocate frame");
                return AVERROR(ENOMEM);
            }

#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
            frame->format = AV_PIX_FMT_NVTEGRA;
#elif defined(PLATFORM_ANDROID)
            frame->format = AV_PIX_FMT_MEDIACODEC;
#else
            if (m_video_format & VIDEO_FORMAT_MASK_10BIT)
                frame->format = AV_PIX_FMT_P010;
            else
                frame->format = AV_PIX_FMT_NV12;
#endif
            frame->width  = m_video_width;
            frame->height = m_video_height;

#if defined(PLATFORM_SWITCH) && !defined(BOREALIS_USE_DEKO3D)
            int err = av_frame_get_buffer(frame, 256);
            if (err < 0) {
                char errs[64];
                brls::Logger::error(
                    "FFmpeg: Couldn't allocate frame buffer: {}",
                    av_make_error_string(errs, 64, err));
                return err;
            }

            for (int j = 0; j < 2; j++) {
                uintptr_t ptr = (uintptr_t)frame->data[j];
                uintptr_t dst = (((ptr)+(256)-1)&~((256)-1));
                uintptr_t gap = dst - ptr;
                frame->data[j] += gap;
            }
#endif
        }
    }

    m_decoder_finalized = true;
    return 0;
}

#if defined(PLATFORM_ANDROID)
bool FFmpegVideoDecoder::should_delay_android_h264_open() const {
    return m_using_android_mediacodec_decoder && m_hw_decode_active &&
           m_codec_id == AV_CODEC_ID_H264;
}

int FFmpegVideoDecoder::prepare_android_h264_extradata(PDECODE_UNIT decode_unit) {
    if (!m_pending_extradata.empty()) {
        return 0;
    }

    PLENTRY sps_entry = nullptr;
    PLENTRY pps_entry = nullptr;

    for (PLENTRY entry = decode_unit->bufferList; entry != nullptr;
         entry = entry->next) {
        if (sps_entry == nullptr && entry->bufferType == BUFFER_TYPE_SPS) {
            sps_entry = entry;
            continue;
        }

        if (sps_entry != nullptr && entry->bufferType == BUFFER_TYPE_PPS) {
            pps_entry = entry;
            break;
        }
    }

    if (sps_entry == nullptr || pps_entry == nullptr) {
        return AVERROR(EAGAIN);
    }

    m_pending_extradata.resize(sps_entry->length + pps_entry->length);
    memcpy(m_pending_extradata.data(), sps_entry->data, sps_entry->length);
    memcpy(m_pending_extradata.data() + sps_entry->length, pps_entry->data,
           pps_entry->length);

    brls::Logger::info(
        "FFmpeg: Prepared Android H.264 MediaCodec extradata from decode unit (sps={} pps={})",
        sps_entry->length, pps_entry->length);
    return 0;
}
#endif

int FFmpegVideoDecoder::setup(int video_format, int width, int height,
                              int redraw_rate, void* context, int dr_flags) {
    m_stream_fps = redraw_rate;
#if defined(PLATFORM_ANDROID)
    ffmpeg::decoder::cleanupAndroidMediaCodecState(m_android_mediacodec);
#endif
#if defined(_WIN32) && defined(USE_D3D11_RENDERER)
    ffmpeg::decoder::resetD3D11State(m_d3d11);
#endif

    std::string format;
    switch (video_format) {
        case VIDEO_FORMAT_H264:
            format = "H264";
            break;
        case VIDEO_FORMAT_H265:
            format = "HEVC";
            break;
        case VIDEO_FORMAT_H265_MAIN10:
            format = "HEVC HDR";
            break;
        case VIDEO_FORMAT_AV1_MAIN8:
            format = "AV1";
            break;
        case VIDEO_FORMAT_AV1_MAIN10:
            format = "AV1 HDR";
            break;
        default:
            format = "UNKNOWN";
            break;
    }
    brls::Logger::debug("FFMpeg's AVCodec version: {}.{}.{}", AV_VERSION_MAJOR(avcodec_version()), AV_VERSION_MINOR(avcodec_version()), AV_VERSION_MICRO(avcodec_version()));
    brls::Logger::info(
        "FFmpeg: Setup with format: {}, width: {}, height: {}, fps: {}", format, width, height, redraw_rate);

    // av_log_set_level(AV_LOG_WARNING);
    av_log_set_level(AV_LOG_INFO);
    av_log_set_callback(&ffmpegLog); // Uncomment to see FFMpeg logs
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)
    avcodec_register_all();
#endif

    m_video_format = video_format;
    m_video_width = width;
    m_video_height = height;
    m_perf_level = LOW_LATENCY_DECODE;
    m_decoder_threads_setting = Settings::instance().decoder_threads();
    m_codec_id = AV_CODEC_ID_NONE;
    m_hw_decode_active = false;
    m_using_android_mediacodec_decoder = false;
    m_supports_slice_threading = false;
    m_use_decoder_threads = false;
    m_use_low_delay = false;
    m_decoder_ready = false;
    m_decoder_finalized = false;
#if defined(PLATFORM_ANDROID)
    m_defer_android_h264_open = false;
    m_pending_extradata.clear();
#endif
    m_use_zero_copy_holder = false;

    m_packet = av_packet_alloc();
    if (m_packet == nullptr) {
        brls::Logger::error("FFmpeg: Couldn't allocate packet");
        return AVERROR(ENOMEM);
    }

#ifdef PLATFORM_ANDROID
    if (video_format & VIDEO_FORMAT_MASK_H264) {
        m_decoder = avcodec_find_decoder_by_name("h264_mediacodec");
        m_codec_id = AV_CODEC_ID_H264;
    } else if (video_format & VIDEO_FORMAT_MASK_H265) {
        m_decoder = avcodec_find_decoder_by_name("hevc_mediacodec");
        m_codec_id = AV_CODEC_ID_HEVC;
    } else {
        // Unsupported decoder type
    }
#else
    if (video_format & VIDEO_FORMAT_MASK_H264) {
        m_decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
        m_codec_id = AV_CODEC_ID_H264;
    } else if (video_format & VIDEO_FORMAT_MASK_H265) {
        m_decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        m_codec_id = AV_CODEC_ID_HEVC;
    } else {
        // Unsupported decoder type
    }
#endif

    if (m_decoder == nullptr) {
        brls::Logger::error("FFmpeg: Couldn't find decoder");
        return -1;
    }

    int err = 0;

    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
#if defined(PLATFORM_SWITCH)
    hwType = AV_HWDEVICE_TYPE_NVTEGRA;
#elif defined(PLATFORM_ANDROID)
    hwType = AV_HWDEVICE_TYPE_MEDIACODEC;
    m_using_android_mediacodec_decoder = true;
#elif defined(USE_D3D11_RENDERER)
    ffmpeg::decoder::prepareD3D11Setup(m_d3d11, video_format);
    hwType = AV_HWDEVICE_TYPE_D3D11VA;
#elif defined(PLATFORM_APPLE)
    hwType = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#endif

    if (Settings::instance().use_hw_decoding() && hwType != AV_HWDEVICE_TYPE_NONE) {
#if defined(PLATFORM_ANDROID)
        if (hwType == AV_HWDEVICE_TYPE_MEDIACODEC) {
            err = ffmpeg::decoder::initializeAndroidMediaCodecHardwareDevice(
                m_android_mediacodec, hw_device_ctx, width, height);
        } else
#endif
#if defined(USE_D3D11_RENDERER)
        if (hwType == AV_HWDEVICE_TYPE_D3D11VA) {
            err = ffmpeg::decoder::initializeD3D11HardwareDevice(m_d3d11, hw_device_ctx);
        } else
#endif
        {
            err = av_hwdevice_ctx_create(&hw_device_ctx, hwType, nullptr, nullptr, 0);
        }

        if (err < 0) {
            char error[512];
            av_strerror(err, error, sizeof(error));
#if defined(USE_D3D11_RENDERER)
            if (hwType == AV_HWDEVICE_TYPE_D3D11VA) {
                ffmpeg::decoder::logD3D11HardwareInitFailure(m_d3d11, error);
            } else
#endif
            brls::Logger::error("FFmpeg: Error initializing hardware decoder - {}", error);
#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
            return err;
#endif
        } else {
            m_hw_decode_active = hw_device_ctx != nullptr;
        }
    } else {
        brls::Logger::warning("FFmpeg: HW decoding disabled or unsupported by Platform");
    }

    m_supports_slice_threading =
        (video_format & (VIDEO_FORMAT_MASK_H264 | VIDEO_FORMAT_MASK_H265)) != 0;
    m_use_decoder_threads =
        !m_hw_decode_active && m_decoder_threads_setting > 1 && m_supports_slice_threading;
    m_use_low_delay =
        (m_perf_level & LOW_LATENCY_DECODE) && !m_use_decoder_threads;

    m_ffmpeg_buffer =
        (char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
    if (m_ffmpeg_buffer == nullptr) {
        brls::Logger::error("FFmpeg: Not enough memory");
        cleanup();
        return -1;
    }

#if defined(PLATFORM_ANDROID)
    m_defer_android_h264_open = should_delay_android_h264_open();
    if (m_defer_android_h264_open) {
        brls::Logger::info(
            "FFmpeg: Delaying Android H.264 MediaCodec open until SPS/PPS is available");
        brls::Logger::info("FFmpeg: Setup done!");
        return DR_OK;
    }
#endif

    err = open_decoder();
    if (err < 0) {
        cleanup();
        return err;
    }

    err = finalize_decoder_setup();
    if (err < 0) {
        cleanup();
        return err;
    }

    brls::Logger::info("FFmpeg: Setup done!");
    return DR_OK;
}

void FFmpegVideoDecoder::cleanup() {
    brls::Logger::info("FFmpeg: Cleanup...");

    m_decoder_ready = false;
    m_decoder_finalized = false;
    m_hw_decode_active = false;
    m_using_android_mediacodec_decoder = false;
    m_use_decoder_threads = false;
    m_use_low_delay = false;
#if defined(PLATFORM_ANDROID)
    m_defer_android_h264_open = false;
    m_pending_extradata.clear();
#endif

    av_packet_free(&m_packet);

    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
    }

    if (m_decoder_context) {
        avcodec_free_context(&m_decoder_context);
    }

#if defined(PLATFORM_ANDROID)
    ffmpeg::decoder::cleanupAndroidMediaCodecState(m_android_mediacodec);
#endif

    if (m_frames) {
        for (int i = 0; i < m_frames_size; i++) {
            av_frame_free(&m_frames[i]);
        }
    }

    if (tmp_frame) {
        av_frame_free(&tmp_frame);
    }

    if (m_ffmpeg_buffer) {
        free(m_ffmpeg_buffer);
        m_ffmpeg_buffer = nullptr;
    }

    AVFrameHolder::instance().cleanup();
    delete[] m_frames;
    m_frames = nullptr;
    m_frames_size = 0;
    tmp_frame = nullptr;
    m_decoder = nullptr;
    m_codec_id = AV_CODEC_ID_NONE;
    m_use_zero_copy_holder = false;

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

#if defined(PLATFORM_ANDROID)
        if (!m_decoder_ready) {
            if (m_defer_android_h264_open) {
                const int extradata_err =
                    prepare_android_h264_extradata(decode_unit);
                if (extradata_err < 0) {
                    brls::Logger::warning(
                        "FFmpeg: Android H.264 MediaCodec is waiting for SPS/PPS before opening the decoder");
                    return DR_NEED_IDR;
                }
            }

            const int open_err = open_decoder();
            if (open_err < 0) {
                return DR_NEED_IDR;
            }

            const int finalize_err = finalize_decoder_setup();
            if (finalize_err < 0) {
                brls::Logger::error(
                    "FFmpeg: Couldn't finalize deferred decoder setup ({})",
                    finalize_err);
                return DR_NEED_IDR;
            }

            m_defer_android_h264_open = false;
        }
#endif

        if (!m_decoder_ready || m_decoder_context == nullptr) {
            return DR_NEED_IDR;
        }

        int length = 0;
        while (entry != nullptr) {
            if (length > DECODER_BUFFER_SIZE) {
                brls::Logger::error("FFmpeg: Big buffer to decode... !");
            }

            memcpy(m_ffmpeg_buffer + length, entry->data, entry->length);
            length += entry->length;
            entry = entry->next;
        }

        if (length <= DECODER_BUFFER_SIZE) {
            memset(m_ffmpeg_buffer + length, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        }

        m_video_decode_stats_progress.current_reassembly_time += LiGetMillis() - (decode_unit->receiveTimeUs / 1000);
        m_frames_in++;

        uint64_t before_decode = LiGetMillis();

        if (length > DECODER_BUFFER_SIZE) {
            brls::Logger::error("FFmpeg: Big buffer to decode...");
        }

        const int decoded_frames = decode(m_ffmpeg_buffer, length);
        if (decoded_frames >= 0) {
            if (decoded_frames == 0) {
                return DR_OK;
            }

            m_frames_out += decoded_frames;

            auto decodeTime = LiGetMillis() - before_decode;
            m_video_decode_stats_progress.current_decode_time += decodeTime;

            // Also count the frame-to-frame delay if the decoder is delaying
            // frames until a subsequent frame is submitted.
            int pending_frames = m_frames_in - m_frames_out;
            if (pending_frames < 0) {
                pending_frames = 0;
            }
            m_video_decode_stats_progress.current_decoder_delay_time +=
                pending_frames * (1000 / m_stream_fps);
            m_video_decode_stats_progress.current_decoded_frames += decoded_frames;

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
                m_video_decode_stats_progress.total_decoder_delay_time = m_video_decode_stats_cache.total_decoder_delay_time + m_video_decode_stats_cache.current_decoder_delay_time;

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
                m_video_decode_stats_cache.current_decoder_delay = (float) m_video_decode_stats_cache.current_decoder_delay_time /
                                                                   (float) m_video_decode_stats_cache.current_decoded_frames;

                m_video_decode_stats_cache.session_receive_time = (float) m_video_decode_stats_cache.total_reassembly_time /
                                                                  (float) m_video_decode_stats_cache.total_received_frames;
                m_video_decode_stats_cache.session_decoding_time = (float) m_video_decode_stats_cache.total_decode_time /
                                                                   (float) m_video_decode_stats_cache.total_decoded_frames;
                m_video_decode_stats_cache.session_decoder_delay = (float) m_video_decode_stats_cache.total_decoder_delay_time /
                                                                   (float) m_video_decode_stats_cache.total_decoded_frames;

                timeCount -= time_interval;
            }

        }
        else {
            if (MoonlightSession::activeSession() != nullptr)
                MoonlightSession::activeSession()->restart();
        }
    } else {
        brls::Logger::error("FFmpeg: Big buffer to decode... 2");
    }
    return DR_OK;
}

int FFmpegVideoDecoder::capabilities() const {
#if defined(__SWITCH__)
    return CAPABILITY_SLICES_PER_FRAME(4);
#else
    return CAPABILITY_SLICES_PER_FRAME(4) | CAPABILITY_DIRECT_SUBMIT;
#endif
}

int FFmpegVideoDecoder::decode(char* indata, int inlen) {
    m_packet->data = (uint8_t*)indata;
    m_packet->size = inlen;

#if defined(_WIN32)
    (void) SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#elif !defined(PLATFORM_SWITCH)
    int policy;
    sched_param params{};
    pthread_getschedparam(pthread_self(), &policy, &params);
    params.sched_priority = sched_get_priority_max(policy);
    pthread_setschedparam(pthread_self(), policy, &params);
#endif

//    m_decoder_context->skip_frame = AVDISCARD_ALL;

    int decoded_frames = 0;
    int err = avcodec_send_packet(m_decoder_context, m_packet);
    if (err == AVERROR(EAGAIN)) {
        decoded_frames = drain_frames();
        if (decoded_frames < 0) {
            return decoded_frames;
        }

        err = avcodec_send_packet(m_decoder_context, m_packet);
    }

    if (err != 0) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        brls::Logger::error("FFmpeg: Decode failed - {}", error);
        return err;
    }

    const int drained_frames = drain_frames();
    if (drained_frames < 0) {
        return drained_frames;
    }

    return decoded_frames + drained_frames;
}

int FFmpegVideoDecoder::drain_frames() {
    int decoded_frames = 0;

    while (true) {
        AVFrame* frame = nullptr;
        const int err = get_frame(true, &frame);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
            return decoded_frames;
        }

        if (err < 0) {
            return err;
        }

        if (!frame) {
            continue;
        }

        if (m_use_zero_copy_holder) {
            AVFrameHolder::instance().pushTransferred(frame);
        } else {
            AVFrameHolder::instance().push(frame);
        }

        decoded_frames++;
    }
}

int FFmpegVideoDecoder::get_frame(bool native_frame, AVFrame** frame) {
    int err;
    *frame = nullptr;
    AVFrame* resultFrame = nullptr;
    AVFrame* decodeFrame = resultFrame;

    if (m_use_zero_copy_holder) {
        resultFrame = AVFrameHolder::instance().acquireWriteFrame();
        if (!resultFrame) {
            brls::Logger::error("FFmpeg: Couldn't acquire frame from holder");
            return AVERROR(EAGAIN);
        }
        decodeFrame = resultFrame;
    } else {
        resultFrame = m_frames[m_next_frame];
        decodeFrame = resultFrame;
    }

#if !defined(PLATFORM_ANDROID) && !defined(BOREALIS_USE_DEKO3D) && !defined(USE_METAL_RENDERER)
    if (hw_device_ctx && !m_use_zero_copy_holder) {
        // For HW->SW transfer path we decode into a temporary hardware frame.
        av_frame_unref(resultFrame);
        decodeFrame = tmp_frame;
    }
#endif

    if ((err = avcodec_receive_frame(m_decoder_context, decodeFrame)) < 0) {
        if (err == AVERROR(EAGAIN)) {
            if (m_use_zero_copy_holder) {
                AVFrameHolder::instance().recycleWriteFrame(resultFrame);
            }
            return err;
        }

        char a[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        brls::Logger::error("FFmpeg: Error receiving frame with error {}",  av_make_error_string(a, AV_ERROR_MAX_STRING_SIZE, err));
        if (m_use_zero_copy_holder) {
            AVFrameHolder::instance().recycleWriteFrame(resultFrame);
        }
        return err;
    }

    if (hw_device_ctx) {
#if defined(BOREALIS_USE_DEKO3D) || defined(USE_METAL_RENDERER) || defined(PLATFORM_ANDROID)
        // Keep hardware-backed frame references per queue slot.
        resultFrame = decodeFrame;
#elif defined(USE_D3D11_RENDERER)
        if (m_use_zero_copy_holder) {
            resultFrame = decodeFrame;
        } else {
            if ((err = av_hwframe_transfer_data(resultFrame, decodeFrame, 0)) < 0) {
                char a[AV_ERROR_MAX_STRING_SIZE] = { 0 };
                brls::Logger::error("FFmpeg: Error transferring the data to system memory with error {}",  av_make_error_string(a, AV_ERROR_MAX_STRING_SIZE, err));
                return err;
            }

            av_frame_copy_props(resultFrame, decodeFrame);
        }
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
            return err;
        }

        av_frame_copy_props(resultFrame, decodeFrame);
#endif
    } else {
        resultFrame = decodeFrame;
    }

    if (!m_use_zero_copy_holder) {
        m_current_frame = m_next_frame;
        m_next_frame = (m_current_frame + 1) % m_frames_size;
    }
    if (native_frame) {
        *frame = resultFrame;
        return 0;
    }

    if (m_use_zero_copy_holder) {
        AVFrameHolder::instance().recycleWriteFrame(resultFrame);
    }
    return 0;
}

VideoDecodeStats* FFmpegVideoDecoder::video_decode_stats() {
    return (VideoDecodeStats*)&m_video_decode_stats_cache;
}
