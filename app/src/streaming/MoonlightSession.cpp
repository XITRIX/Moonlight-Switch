#include "MoonlightSession.hpp"
#include "AVFrameHolder.hpp"
#include "GameStreamClient.hpp"
#include "InputManager.hpp"
#include "Settings.hpp"
#include "VideoCodecSupport.hpp"
#include "borealis.hpp"
#include <string.h>
#include <utility>

#if defined(PLATFORM_IOS) || defined(PLATFORM_VISIONOS)
extern void getWindowSize(int* w, int* h);
#endif

using namespace brls;

int m_video_format;
static MoonlightSession* m_active_session = nullptr;
static MoonlightSessionDecoderAndRenderProvider* m_provider = nullptr;


MoonlightSession* MoonlightSession::activeSession() {
    return m_active_session;
}

void MoonlightSession::set_provider(
    MoonlightSessionDecoderAndRenderProvider* provider) {
    m_provider = provider;
}

MoonlightSession::MoonlightSession(const std::string& address, int app_id) {
    m_address = address;
    m_app_id = app_id;
    m_active_session = this;
    m_stream_settings = StreamProfileResolver::globalDefaults();
    setActiveResolvedStreamSettings(&m_stream_settings);

    m_video_decoder = m_provider->video_decoder();
    m_video_renderer = m_provider->video_renderer();
    m_audio_renderer = m_provider->audio_renderer();
}

MoonlightSession::~MoonlightSession() {
    if (m_video_decoder) {
        delete m_video_decoder;
    }

    if (m_video_renderer) {
        delete m_video_renderer;
    }

    if (m_audio_renderer) {
        delete m_audio_renderer;
    }

    setActiveResolvedStreamSettings(nullptr);
    m_active_session = nullptr;
}

void MoonlightSession::set_stream_settings(
    const ResolvedStreamSettings& settings) {
    if (m_is_active)
        return;
    m_stream_settings = settings;
    setActiveResolvedStreamSettings(&m_stream_settings);
}

// MARK: Connection callbacks

static const char* stages[] = {"STAGE_NONE",
                               "STAGE_PLATFORM_INIT",
                               "STAGE_NAME_RESOLUTION",
                               "STAGE_RTSP_HANDSHAKE",
                               "STAGE_CONTROL_STREAM_INIT",
                               "STAGE_VIDEO_STREAM_INIT",
                               "STAGE_AUDIO_STREAM_INIT",
                               "STAGE_INPUT_STREAM_INIT",
                               "STAGE_CONTROL_STREAM_START",
                               "STAGE_VIDEO_STREAM_START",
                               "STAGE_AUDIO_STREAM_START",
                               "STAGE_INPUT_STREAM_START"};

void MoonlightSession::connection_stage_starting(int stage) {
    brls::Logger::info("MoonlightSession: Starting: {}", stages[stage]);
}

void MoonlightSession::connection_stage_complete(int stage) {
    brls::Logger::info("MoonlightSession: Complete: {}", stages[stage]);
}

void MoonlightSession::connection_stage_failed(int stage, int error_code) {
    brls::Logger::error("MoonlightSession: Failed: {} with error code: {}", stages[stage], error_code);
}

void MoonlightSession::connection_started() {
    brls::Logger::info("MoonlightSession: Connection started");
    if (!m_active_session)
        return;

    m_active_session->m_stop_requested = false;
    m_active_session->m_is_active = true;
}

void MoonlightSession::connection_terminated(int error_code) {
    brls::Logger::info("MoonlightSession: Connection terminated with code: {}", error_code);

    if (!m_active_session)
        return;

    if (m_active_session->m_stop_requested) {
        brls::Logger::info("MoonlightSession: Termination acknowledged after stop request");
        m_active_session->m_is_active = false;
        m_active_session->m_is_terminated = true;
        return;
    }

    if (error_code != 0) {
        brls::Logger::info("MoonlightSession: Reconnection attempt");

        // Connection is already terminated here; avoid toggling the user stop flag.
        LiStopConnection();

        m_active_session->start_internal([](const GSResult<bool>& result) {
            if (result.isSuccess()) {
                brls::Logger::info("MoonlightSession: Reconnected");
            } else {
                brls::Logger::info("MoonlightSession: Reconnection failed");
                if (m_active_session) {
                    m_active_session->m_is_active = false;
                    m_active_session->m_is_terminated = true;
                }
            }
        }, m_active_session->m_is_sunshine);
        return;
    }

    m_active_session->m_is_active = false;
    m_active_session->m_is_terminated = true;
}

void MoonlightSession::connection_log_message(const char* format, ...) {
    va_list arglist;
    va_start(arglist, format);
    int size = vsnprintf(NULL, 0, format, arglist);
    char buffer[size];
    vsnprintf(buffer, size, format, arglist);
    va_end(arglist);

    brls::Logger::info(fmt::runtime(std::string(buffer)));
}

void MoonlightSession::connection_rumble(unsigned short controller,
                                         unsigned short lowFreqMotor,
                                         unsigned short highFreqMotor) {
    MoonlightInputManager::instance().handleRumble(controller, lowFreqMotor,
                                                   highFreqMotor);
}


void MoonlightSession::connection_rumble_triggers(uint16_t controllerNumber, 
                                                  uint16_t leftTriggerMotor, 
                                                  uint16_t rightTriggerMotor) 
{
    // MoonlightInputManager::instance().handleRumbleTriggers(controllerNumber, leftTriggerMotor, rightTriggerMotor);                                                
}

void MoonlightSession::connection_status_update(int connection_status) {
    if (m_active_session) {
        m_active_session->m_connection_status_is_poor =
            connection_status == CONN_STATUS_POOR;
    }
}

void MoonlightSession::connection_set_hdr_mode(bool use_hdr) {
    if (m_active_session) {
        m_active_session->m_use_hdr = use_hdr;
    }
}

// MARK: Video decoder callbacks

int MoonlightSession::video_decoder_setup(int video_format, int width,
                                          int height, int redraw_rate,
                                          void* context, int dr_flags) {
    m_video_format = video_format;
    if (m_active_session && m_active_session->m_video_decoder) {
        const int result = m_active_session->m_video_decoder->setup(
            video_format, width, height, redraw_rate, context, dr_flags);
        if (result < 0 && (video_format & VIDEO_FORMAT_MASK_AV1) != 0) {
            m_active_session->m_av1_decoder_failed = true;
            brls::Logger::warning(
                "MoonlightSession: AV1 hardware decoder setup failed on {}; scheduling one fallback attempt",
                hardwareVideoCodecSupport().av1Backend);
        }
        return result;
    }
    return DR_OK;
}

void MoonlightSession::video_decoder_start() {
    if (m_active_session && m_active_session->m_video_decoder) {
        m_active_session->m_video_decoder->start();
    }
}

void MoonlightSession::video_decoder_stop() {
    if (m_active_session && m_active_session->m_video_decoder) {
        m_active_session->m_video_decoder->stop();
    }
}

void MoonlightSession::video_decoder_cleanup() {
    if (m_active_session && m_active_session->m_video_decoder) {
        m_active_session->m_video_decoder->cleanup();
    }
}

int MoonlightSession::video_decoder_submit_decode_unit(
    PDECODE_UNIT decode_unit) {
    if (m_active_session && m_active_session->m_video_decoder) {
        const int result =
            m_active_session->m_video_decoder->submit_decode_unit(decode_unit);
        if (result < 0) {
            if ((m_video_format & VIDEO_FORMAT_MASK_AV1) != 0 &&
                m_active_session->fallback_from_av1()) {
                return DR_OK;
            }

            m_active_session->restart();
            return DR_OK;
        }
        return result;
    }
    return DR_OK;
}

// MARK: Audio callbacks

int MoonlightSession::audio_renderer_init(
    int audio_configuration, const POPUS_MULTISTREAM_CONFIGURATION opus_config,
    void* context, int ar_flags) {
    if (m_active_session && m_active_session->m_audio_renderer) {
        return m_active_session->m_audio_renderer->init(
            audio_configuration, opus_config, context, ar_flags);
    }
    return DR_OK;
}

void MoonlightSession::audio_renderer_start() {
    if (m_active_session && m_active_session->m_audio_renderer) {
        m_active_session->m_audio_renderer->start();
    }
}

void MoonlightSession::audio_renderer_stop() {
    if (m_active_session && m_active_session->m_audio_renderer) {
        m_active_session->m_audio_renderer->stop();
    }
}

void MoonlightSession::audio_renderer_cleanup() {
    if (m_active_session && m_active_session->m_audio_renderer) {
        m_active_session->m_audio_renderer->cleanup();
    }
}

void MoonlightSession::audio_renderer_decode_and_play_sample(
    char* sample_data, int sample_length) {
    if (m_active_session && m_active_session->m_audio_renderer) {
        m_active_session->m_audio_renderer->decode_and_play_sample(
            sample_data, sample_length);
    }
}

// MARK: MoonlightSession

void MoonlightSession::start(ServerCallback<bool> callback, bool is_sunshine) {
    m_force_disable_av1 = false;
    m_av1_fallback_attempted = false;
    start_internal(std::move(callback), is_sunshine);
}

void MoonlightSession::start_internal(ServerCallback<bool> callback,
                                      bool is_sunshine) {
    m_is_sunshine = is_sunshine;
    m_stop_requested = false;
    m_is_terminated = false;
    m_av1_decoder_failed = false;

    LiInitializeStreamConfiguration(&m_config);

    int resolution = m_stream_settings.resolution;
    int h = resolution;
    int w = h * 16 / 9;
    if (resolution == -1) {
#if defined(PLATFORM_IOS) || defined(PLATFORM_VISIONOS)
        getWindowSize(&w, &h);
#else
        h = Application::windowHeight;
        w = Application::windowWidth;
#endif

        int nativeResolutionScale = m_stream_settings.nativeResolutionScale;
        if (nativeResolutionScale != 100 && w > 0 && h > 0) {
            w = (w * nativeResolutionScale + 50) / 100;
            h = (h * nativeResolutionScale + 50) / 100;
        }
    }

    // Prohibit odd values rounding up
    if ((h & 1) == 1) {
        h += 1;
    }

    if ((w & 1) == 1) {
        w += 1;
    }

    m_config.width = w;
    m_config.height = h;
    m_config.fps = m_stream_settings.fps;
    m_config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    m_config.packetSize = 1392;
    m_config.streamingRemotely = STREAM_CFG_AUTO;
    m_config.bitrate = m_stream_settings.bitrate;
    m_config.encryptionFlags = m_is_sunshine ? ENCFLG_ALL : ENCFLG_VIDEO;

    const VideoCodec requestedCodec = m_stream_settings.videoCodec;
    const VideoCodec codec = validatedVideoCodec(requestedCodec);
    if (codec != requestedCodec) {
        brls::Logger::info(
            "MoonlightSession: Requested codec is unavailable; using {}",
            getVideoCodecName(codec));
    }

    switch (codec) {
    case H264:
        m_config.supportedVideoFormats = VIDEO_FORMAT_H264;
        break;
    case H265:
        m_config.supportedVideoFormats = VIDEO_FORMAT_H265;
        if (m_stream_settings.requestHdr) {
            m_config.supportedVideoFormats |= VIDEO_FORMAT_H265_MAIN10;
        }
        break;
    case AV1:
        if (m_force_disable_av1) {
            m_config.supportedVideoFormats =
                VIDEO_FORMAT_H264 | VIDEO_FORMAT_H265;
            if (m_stream_settings.requestHdr) {
                m_config.supportedVideoFormats |= VIDEO_FORMAT_H265_MAIN10;
            }
            brls::Logger::info(
                "MoonlightSession: AV1 fallback active; advertising HEVC and H.264");
            break;
        }

        m_config.supportedVideoFormats =
            VIDEO_FORMAT_H264 | VIDEO_FORMAT_H265 | VIDEO_FORMAT_AV1_MAIN8;
        if (m_stream_settings.requestHdr) {
            m_config.supportedVideoFormats |= VIDEO_FORMAT_H265_MAIN10;
            if (hardwareVideoCodecSupport().av1Main10) {
                m_config.supportedVideoFormats |= VIDEO_FORMAT_AV1_MAIN10;
            }
        }
        brls::Logger::info(
            "MoonlightSession: Prefer AV1 via {}; HEVC and H.264 fallback enabled",
            hardwareVideoCodecSupport().av1Backend);
        break;
    default:
        break;
    }

    LiInitializeConnectionCallbacks(&m_connection_callbacks);
    m_connection_callbacks.stageStarting = connection_stage_starting;
    m_connection_callbacks.stageComplete = connection_stage_complete;
    m_connection_callbacks.stageFailed = connection_stage_failed;
    m_connection_callbacks.connectionStarted = connection_started;
    m_connection_callbacks.connectionTerminated = connection_terminated;
    m_connection_callbacks.logMessage = connection_log_message;
    m_connection_callbacks.rumble = connection_rumble;
    m_connection_callbacks.rumbleTriggers = connection_rumble_triggers;
    m_connection_callbacks.connectionStatusUpdate = connection_status_update;
    m_connection_callbacks.setHdrMode = connection_set_hdr_mode;

    LiInitializeVideoCallbacks(&m_video_callbacks);
    m_video_callbacks.setup = video_decoder_setup;
    m_video_callbacks.start = video_decoder_start;
    m_video_callbacks.stop = video_decoder_stop;
    m_video_callbacks.cleanup = video_decoder_cleanup;
    m_video_callbacks.submitDecodeUnit = video_decoder_submit_decode_unit;

    if (m_video_decoder) {
        m_video_callbacks.capabilities = m_video_decoder->capabilities();
    }

    LiInitializeAudioCallbacks(&m_audio_callbacks);
    m_audio_callbacks.init = audio_renderer_init;
    m_audio_callbacks.start = audio_renderer_start;
    m_audio_callbacks.stop = audio_renderer_stop;
    m_audio_callbacks.cleanup = audio_renderer_cleanup;
    m_audio_callbacks.decodeAndPlaySample =
        audio_renderer_decode_and_play_sample;

    if (m_audio_renderer) {
        m_audio_callbacks.capabilities = m_audio_renderer->capabilities();
    }

    GameStreamClient::instance().start(
        m_address, m_config, m_app_id, [this, callback](auto result) {
            if (result.isSuccess()) {
                m_config = result.value();
                brls::async([this, callback]() mutable {
                    auto m_data =
                        GameStreamClient::instance().server_data(m_address);

                    int result = LiStartConnection(
                        &m_data.serverInfo, &m_config, &m_connection_callbacks,
                        &m_video_callbacks, &m_audio_callbacks, NULL, 0, NULL, 0);

                    if (result != 0 && m_av1_decoder_failed &&
                        !m_av1_fallback_attempted) {
                        m_av1_fallback_attempted = true;
                        m_force_disable_av1 = true;
                        m_av1_decoder_failed = false;
                        brls::Logger::warning(
                            "MoonlightSession: Retrying once without AV1 after {} hardware decoder setup failure; fallback formats are HEVC and H.264",
                            hardwareVideoCodecSupport().av1Backend);
                        LiStopConnection();
                        start_internal(std::move(callback), m_is_sunshine);
                    } else if (result != 0) {
                        LiStopConnection();
                        callback(
                            GSResult<bool>::failure("error/stream_start"_i18n));
                    } else {
                        callback(GSResult<bool>::success(true));
                    }
                });
            } else {
                brls::Logger::error(
                    "MoonlightSession: Failed to start stream: {}",
                    result.error().c_str());
                callback(GSResult<bool>::failure(result.error()));
            }
        });
}

void MoonlightSession::stop(int terminate_app) {
    if (m_stop_requested)
        return;

    m_stop_requested = true;

    if (terminate_app) {
        GameStreamClient::instance().quit(m_address, [](auto _) {});
    }

    LiStopConnection();
}

void MoonlightSession::restart() {
    LiStopConnection();

    start_internal([](const GSResult<bool>& result) {
        if (result.isSuccess()) {
            brls::Logger::info("MoonlightSession: Reconnected");
        } else {
            brls::Logger::info("MoonlightSession: Reconnection failed");
            if (m_active_session) {
                m_active_session->m_is_active = false;
                m_active_session->m_is_terminated = true;
            }
        }
    }, m_active_session->m_is_sunshine);
}

bool MoonlightSession::fallback_from_av1() {
    if (m_av1_fallback_attempted || m_force_disable_av1) {
        return false;
    }

    m_av1_fallback_attempted = true;
    m_force_disable_av1 = true;
    brls::Logger::warning(
        "MoonlightSession: AV1 hardware frame decode failed on {}; reconnecting once with HEVC and H.264 fallback",
        hardwareVideoCodecSupport().av1Backend);
    restart();
    return true;
}

void MoonlightSession::draw(NVGcontext* vg, int width, int height) {
    if (m_video_decoder && m_video_renderer) {
        AVFrameHolder::instance().get(
            [this, vg, width, height](AVFrame* frame) {
                m_video_renderer->draw(vg, width, height, frame, m_video_format);
            });

        const uint64_t now = LiGetMillis();
        if (m_last_stats_update_ms == 0 || now - m_last_stats_update_ms >= 250) {
            m_session_stats.video_decode_stats =
                *m_video_decoder->video_decode_stats();
            m_session_stats.video_render_stats =
                *m_video_renderer->video_render_stats();
            m_last_stats_update_ms = now;
        }
    }
}
