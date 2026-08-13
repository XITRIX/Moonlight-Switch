#include "Settings.hpp"
#include <jansson.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <climits>
#include <filesystem>
#include <set>

using namespace brls;
using namespace brls::literals;

namespace {
namespace fs = std::filesystem;

bool key_combos_conflict(
    const std::vector<brls::ControllerButton>& lhs,
    const std::vector<brls::ControllerButton>& rhs) {
    if (lhs.empty() || rhs.empty())
        return false;

    std::set<brls::ControllerButton> lhsButtons(lhs.begin(), lhs.end());
    std::set<brls::ControllerButton> rhsButtons(rhs.begin(), rhs.end());
    const auto& smaller = lhsButtons.size() < rhsButtons.size() ? lhsButtons
                                                                : rhsButtons;
    const auto& larger = lhsButtons.size() < rhsButtons.size() ? rhsButtons
                                                               : lhsButtons;
    return std::includes(larger.begin(), larger.end(), smaller.begin(),
                         smaller.end());
}

void load_button_array(json_t* settings, const char* key,
                       std::vector<brls::ControllerButton>& buttons) {
    json_t* jsonButtons = json_object_get(settings, key);
    if (!json_is_array(jsonButtons))
        return;

    buttons.clear();
    const size_t size = json_array_size(jsonButtons);
    for (size_t i = 0; i < size; i++) {
        json_t* button = json_array_get(jsonButtons, i);
        if (!json_is_integer(button))
            continue;

        const auto value = json_integer_value(button);
        if (value < 0 || value >= brls::ControllerButton::_BUTTON_MAX)
            continue;

        const auto controllerButton =
            static_cast<brls::ControllerButton>(value);
        if (std::find(buttons.begin(), buttons.end(), controllerButton) ==
            buttons.end())
            buttons.push_back(controllerButton);
    }
}

void save_button_array(json_t* settings, const char* key,
                       const std::vector<brls::ControllerButton>& buttons) {
    json_t* jsonButtons = json_array();
    if (!jsonButtons)
        return;

    for (auto button : buttons)
        json_array_append_new(jsonButtons, json_integer(button));
    json_object_set_new(settings, key, jsonButtons);
}

Host* find_host(std::vector<Host>& hosts, const Host& target) {
    auto it = std::find_if(hosts.begin(), hosts.end(), [target](const Host& host) {
        return hosts_match(host, target);
    });
    return it != hosts.end() ? &(*it) : nullptr;
}

const Host* find_host(const std::vector<Host>& hosts, const Host& target) {
    auto it = std::find_if(hosts.begin(), hosts.end(), [target](const Host& host) {
        return hosts_match(host, target);
    });
    return it != hosts.end() ? &(*it) : nullptr;
}

void merge_host(Host& target, const Host& source) {
    if (!source.address.empty())
        target.address = source.address;
    if (!source.remoteAddress.empty())
        target.remoteAddress = source.remoteAddress;
    if (!source.hostname.empty())
        target.hostname = source.hostname;
    if (!source.mac.empty())
        target.mac = source.mac;
}

std::string make_preferred_path(const fs::path& path) {
    auto preferred = path;
    preferred.make_preferred();
    return preferred.string();
}

std::string settings_file_path(const std::string& working_dir) {
    return make_preferred_path(fs::path(working_dir) / "settings.json");
}

const char* stream_context_key(StreamProfileContext context) {
    switch (context) {
    case StreamProfileContext::LocalWifi:
        return "local_wifi";
    case StreamProfileContext::LocalEthernet:
        return "local_ethernet";
    case StreamProfileContext::Remote:
        return "remote";
    }
    return "";
}

constexpr StreamProfileContext stream_contexts[] = {
    StreamProfileContext::LocalWifi,
    StreamProfileContext::LocalEthernet,
    StreamProfileContext::Remote,
};

std::optional<int> optional_integer(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    if (!json_is_integer(value))
        return std::nullopt;
    return static_cast<int>(json_integer_value(value));
}

std::optional<float> optional_number(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    if (!json_is_number(value))
        return std::nullopt;
    return static_cast<float>(json_number_value(value));
}

std::optional<bool> optional_boolean(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    if (!json_is_boolean(value))
        return std::nullopt;
    return json_is_true(value);
}

StreamProfile load_stream_profile(json_t* json) {
    StreamProfile profile;
    if (!json_is_object(json))
        return profile;

    profile.enabled = optional_boolean(json, "enabled").value_or(false);
    profile.resolution = optional_integer(json, "resolution");
    profile.nativeResolutionScale = optional_integer(json, "native_resolution_scale");
    profile.fps = optional_integer(json, "fps");
    profile.bitrate = optional_integer(json, "bitrate");
    if (const auto value = optional_integer(json, "video_codec"))
        profile.videoCodec = static_cast<VideoCodec>(*value);
    profile.requestHdr = optional_boolean(json, "enable_hdr");
    if (const auto value = optional_integer(json, "frame_pacing_mode"))
        profile.framePacingMode = static_cast<FramePacingMode>(*value);
    if (const auto value = optional_integer(json, "upscaling_mode"))
        profile.upscalingMode = static_cast<UpscalingMode>(*value);
    profile.dithering = optional_boolean(json, "enable_dithering");
    profile.ditheringStrength = optional_number(json, "dithering_strength");
    profile.rcas = optional_boolean(json, "enable_rcas");
    profile.rcasStrength = optional_number(json, "rcas_strength");
    return profile;
}

void set_optional_integer(json_t* json, const char* key,
                          const std::optional<int>& value) {
    if (value)
        json_object_set_new(json, key, json_integer(*value));
}

void set_optional_number(json_t* json, const char* key,
                         const std::optional<float>& value) {
    if (value)
        json_object_set_new(json, key, json_real(*value));
}

void set_optional_boolean(json_t* json, const char* key,
                          const std::optional<bool>& value) {
    if (value)
        json_object_set_new(json, key, *value ? json_true() : json_false());
}

json_t* save_stream_profile(const StreamProfile& profile) {
    json_t* json = json_object();
    json_object_set_new(json, "enabled", profile.enabled ? json_true() : json_false());
    set_optional_integer(json, "resolution", profile.resolution);
    set_optional_integer(json, "native_resolution_scale", profile.nativeResolutionScale);
    set_optional_integer(json, "fps", profile.fps);
    set_optional_integer(json, "bitrate", profile.bitrate);
    if (profile.videoCodec)
        json_object_set_new(json, "video_codec", json_integer(*profile.videoCodec));
    set_optional_boolean(json, "enable_hdr", profile.requestHdr);
    if (profile.framePacingMode)
        json_object_set_new(json, "frame_pacing_mode",
                            json_integer(static_cast<int>(*profile.framePacingMode)));
    if (profile.upscalingMode)
        json_object_set_new(json, "upscaling_mode",
                            json_integer(static_cast<int>(*profile.upscalingMode)));
    set_optional_boolean(json, "enable_dithering", profile.dithering);
    set_optional_number(json, "dithering_strength", profile.ditheringStrength);
    set_optional_boolean(json, "enable_rcas", profile.rcas);
    set_optional_number(json, "rcas_strength", profile.rcasStrength);
    return json;
}

StreamQualityAggregate load_stream_quality(json_t* json) {
    StreamQualityAggregate aggregate;
    if (!json_is_object(json))
        return aggregate;
    aggregate.sampleCount = static_cast<uint32_t>(
        std::max(0, optional_integer(json, "sample_count").value_or(0)));
    aggregate.rttMs = optional_number(json, "rtt_ms").value_or(0.0f);
    aggregate.rttVariationMs = optional_number(json, "rtt_variation_ms").value_or(0.0f);
    aggregate.networkFrameLoss = optional_number(json, "network_frame_loss").value_or(0.0f);
    aggregate.hostFps = optional_number(json, "host_fps").value_or(0.0f);
    aggregate.receivedFps = optional_number(json, "received_fps").value_or(0.0f);
    aggregate.decodedFps = optional_number(json, "decoded_fps").value_or(0.0f);
    aggregate.renderedFps = optional_number(json, "rendered_fps").value_or(0.0f);
    aggregate.decodeTimeMs = optional_number(json, "decode_time_ms").value_or(0.0f);
    aggregate.gpuTimeMs = optional_number(json, "gpu_time_ms").value_or(0.0f);
    aggregate.postProcessingTimeMs = optional_number(json, "post_processing_time_ms").value_or(0.0f);
    aggregate.queueDepth = optional_number(json, "queue_depth").value_or(0.0f);
    aggregate.underflowsPerSecond = optional_number(json, "underflows_per_second").value_or(0.0f);
    return aggregate;
}

json_t* save_stream_quality(const StreamQualityAggregate& aggregate) {
    json_t* json = json_object();
    json_object_set_new(json, "sample_count", json_integer(aggregate.sampleCount));
    json_object_set_new(json, "rtt_ms", json_real(aggregate.rttMs));
    json_object_set_new(json, "rtt_variation_ms", json_real(aggregate.rttVariationMs));
    json_object_set_new(json, "network_frame_loss", json_real(aggregate.networkFrameLoss));
    json_object_set_new(json, "host_fps", json_real(aggregate.hostFps));
    json_object_set_new(json, "received_fps", json_real(aggregate.receivedFps));
    json_object_set_new(json, "decoded_fps", json_real(aggregate.decodedFps));
    json_object_set_new(json, "rendered_fps", json_real(aggregate.renderedFps));
    json_object_set_new(json, "decode_time_ms", json_real(aggregate.decodeTimeMs));
    json_object_set_new(json, "gpu_time_ms", json_real(aggregate.gpuTimeMs));
    json_object_set_new(json, "post_processing_time_ms", json_real(aggregate.postProcessingTimeMs));
    json_object_set_new(json, "queue_depth", json_real(aggregate.queueDepth));
    json_object_set_new(json, "underflows_per_second", json_real(aggregate.underflowsPerSecond));
    return json;
}
}

std::string getVideoCodecName(VideoCodec codec) {
    switch (codec) {
        case H264:
            return "settings/h264"_i18n;
        case H265:
            return "settings/h265"_i18n;
        case AV1:
            return "settings/av1"_i18n;
        default:
            return "settings/h264"_i18n;
    }
}

std::string getFramePacingModeName(FramePacingMode mode) {
    switch (mode) {
    case FramePacingMode::LOWEST_LATENCY:
        return "settings/frame_pacing_lowest_latency"_i18n;
    case FramePacingMode::BALANCED:
        return "settings/frame_pacing_balanced"_i18n;
    case FramePacingMode::SMOOTHEST_VIDEO:
        return "settings/frame_pacing_smoothest"_i18n;
    default:
        return "settings/frame_pacing_balanced"_i18n;
    }
}

const char* getFramePacingModeDebugName(FramePacingMode mode) {
    switch (mode) {
    case FramePacingMode::LOWEST_LATENCY:
        return "lowest-latency";
    case FramePacingMode::BALANCED:
        return "balanced";
    case FramePacingMode::SMOOTHEST_VIDEO:
        return "smoothest-video";
    default:
        return "balanced";
    }
}

void Settings::set_working_dir(const std::string& working_dir) {
    const fs::path base_path = fs::path(working_dir).make_preferred();

    m_working_dir = base_path.string();
    m_key_dir = make_preferred_path(base_path / "key");
    m_boxart_dir = make_preferred_path(base_path / "boxart");
    m_log_path = make_preferred_path(base_path / "log.log");
    m_gamepad_mapping_path =
            make_preferred_path(base_path / "gamepad_mapping_v1.2.0.json");

    std::error_code error;
    fs::create_directories(base_path, error);
    fs::create_directories(fs::path(m_key_dir), error);
    fs::create_directories(fs::path(m_boxart_dir), error);
    
    load();
}

void Settings::add_host(const Host& host) {
    if (Host* existing = find_host(m_hosts, host)) {
        merge_host(*existing, host);
    } else if (!host.preferred_address().empty() && !host.mac.empty()) {
        m_hosts.push_back(host);
    }

    save();
}

std::optional<Host> Settings::host(const Host& host) const {
    if (const Host* existing = find_host(m_hosts, host))
        return *existing;
    return std::nullopt;
}

void Settings::set_stream_profile(const Host& host,
                                  StreamProfileContext context,
                                  const StreamProfile& profile,
                                  bool persist) {
    if (Host* existing = find_host(m_hosts, host)) {
        existing->streamProfiles[context] = profile;
        if (persist)
            save();
    }
}

void Settings::set_stream_quality_aggregate(
    const Host& host, StreamProfileContext context,
    const StreamQualityAggregate& aggregate) {
    if (Host* existing = find_host(m_hosts, host)) {
        existing->streamQuality[context] = aggregate;
        save();
    }
}

void Settings::remove_host(const Host& host) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](const Host& item) {
        return hosts_match(item, host);
    });
    
    if (it != m_hosts.end()) {
        m_hosts.erase(it);
        save();
    }
}

void Settings::add_favorite(const Host& host, const App& app) {
    if (Host* existing = find_host(m_hosts, host)) {
        auto app_it = std::find_if(existing->favorites.begin(), existing->favorites.end(), [app](auto h){
            return h.app_id == app.app_id;
        });

        if (app_it != existing->favorites.end()) {
            existing->favorites.erase(app_it);
        }
        
        existing->favorites.push_back(app);
        save();
    }
}

void Settings::remove_favorite(const Host& host, int app_id) {
    if (Host* existing = find_host(m_hosts, host)) {
        auto app_it = std::find_if(existing->favorites.begin(), existing->favorites.end(), [app_id](auto h){
            return h.app_id == app_id;
        });

        if (app_it != existing->favorites.end()) {
            existing->favorites.erase(app_it);
            save();
        }
    }
}

bool Settings::is_favorite(const Host& host, int app_id) {
    if (const Host* existing = find_host(m_hosts, host)) {
        auto app_it = std::find_if(existing->favorites.begin(), existing->favorites.end(), [app_id](auto h){
            return h.app_id == app_id;
        });

        if (app_it != existing->favorites.end()) {
            return true;
        }
    }

    return false;
}

bool Settings::has_any_favorite() {
    return std::any_of(m_hosts.begin(), m_hosts.end(), [&](const auto &item) {
        return !item.favorites.empty();
    });
}

void Settings::set_key_combo_options(KeyComboAction action,
                                     KeyComboOptions options) {
    switch (action) {
    case KeyComboAction::GUIDE:
        m_guide_key_options = std::move(options);
        break;
    case KeyComboAction::OVERLAY:
        m_overlay_options = std::move(options);
        break;
    case KeyComboAction::MOUSE_INPUT:
        m_mouse_input_options = std::move(options);
        break;
    case KeyComboAction::HOST_CLOSE_APP:
        options.holdTime = 1;
        m_host_close_app_options = std::move(options);
        break;
    case KeyComboAction::HOST_SWITCH_WINDOW:
        options.holdTime = 1;
        m_host_switch_window_options = std::move(options);
        break;
    }
}

KeyComboOptions Settings::key_combo_options(KeyComboAction action) const {
    switch (action) {
    case KeyComboAction::GUIDE:
        return m_guide_key_options;
    case KeyComboAction::OVERLAY:
        return m_overlay_options;
    case KeyComboAction::MOUSE_INPUT:
        return m_mouse_input_options;
    case KeyComboAction::HOST_CLOSE_APP:
        return m_host_close_app_options;
    case KeyComboAction::HOST_SWITCH_WINDOW:
        return m_host_switch_window_options;
    }

    return {.holdTime = 0, .buttons = {}};
}

std::optional<KeyComboAction> Settings::key_combo_conflict(
    KeyComboAction action,
    const std::vector<brls::ControllerButton>& buttons) const {
    constexpr KeyComboAction actions[] = {
        KeyComboAction::GUIDE,
        KeyComboAction::OVERLAY,
        KeyComboAction::MOUSE_INPUT,
        KeyComboAction::HOST_CLOSE_APP,
        KeyComboAction::HOST_SWITCH_WINDOW,
    };

    for (auto candidate : actions) {
        if (candidate == action)
            continue;
        if (key_combos_conflict(buttons,
                                key_combo_options(candidate).buttons))
            return candidate;
    }

    return std::nullopt;
}

void Settings::sanitizeHostShortcutOptions() {
    constexpr KeyComboAction existingActions[] = {
        KeyComboAction::GUIDE,
        KeyComboAction::OVERLAY,
        KeyComboAction::MOUSE_INPUT,
    };

    for (auto action : existingActions) {
        if (key_combos_conflict(m_host_close_app_options.buttons,
                                key_combo_options(action).buttons)) {
            m_host_close_app_options.buttons.clear();
            break;
        }
    }

    for (auto action : existingActions) {
        if (key_combos_conflict(m_host_switch_window_options.buttons,
                                key_combo_options(action).buttons)) {
            m_host_switch_window_options.buttons.clear();
            return;
        }
    }

    if (key_combos_conflict(m_host_switch_window_options.buttons,
                            m_host_close_app_options.buttons))
        m_host_switch_window_options.buttons.clear();
}

void Settings::load() {
    loadBaseLayouts();

    json_t* root = json_load_file(settings_file_path(m_working_dir).c_str(), 0, nullptr);
    
    if (root && json_typeof(root) == JSON_OBJECT) {
        if (json_t* hosts = json_object_get(root, "hosts")) {
            size_t size = json_array_size(hosts);
            for (size_t i = 0; i < size; i++) {
                if (json_t* json = json_array_get(hosts, i)) {
                    if (json_typeof(json) == JSON_OBJECT) {
                        Host host;
                        
                        if (json_t* address = json_object_get(json, "address")) {
                            if (json_typeof(address) == JSON_STRING) {
                                host.address = json_string_value(address);
                            }
                        }

                        if (json_t* remoteAddress = json_object_get(json, "remote_address")) {
                            if (json_typeof(remoteAddress) == JSON_STRING) {
                                host.remoteAddress = json_string_value(remoteAddress);
                            }
                        } else if (json_t* legacyRemoteAddress = json_object_get(json, "remoteAddress")) {
                            if (json_typeof(legacyRemoteAddress) == JSON_STRING) {
                                host.remoteAddress = json_string_value(legacyRemoteAddress);
                            }
                        }
                        
                        if (json_t* hostname = json_object_get(json, "hostname")) {
                            if (json_typeof(hostname) == JSON_STRING) {
                                host.hostname = json_string_value(hostname);
                            }
                        }
                        
                        if (json_t* mac = json_object_get(json, "mac")) {
                            if (json_typeof(mac) == JSON_STRING) {
                                host.mac = json_string_value(mac);
                            }
                        }

                        if (json_t* favorites = json_object_get(json, "favorites")) {
                            size_t size = json_array_size(favorites);
                            for (size_t i = 0; i < size; i++) {
                                if (json_t* json = json_array_get(favorites, i)) {
                                    if (json_typeof(json) == JSON_OBJECT) {
                                        App app;

                                        if (json_t* name = json_object_get(json, "name")) {
                                            if (json_typeof(name) == JSON_STRING) {
                                                app.name = json_string_value(name);
                                            }
                                        }

                                        if (json_t* id = json_object_get(json, "id")) {
                                            if (json_typeof(id) == JSON_INTEGER) {
                                                app.app_id = (int)json_integer_value(id);
                                            }
                                        }
                                        
                                        host.favorites.push_back(app);
                                    }
                                }
                            }
                        }

                        if (json_t* profiles =
                                json_object_get(json, "stream_profiles");
                            json_is_object(profiles)) {
                            for (auto context : stream_contexts) {
                                json_t* profile = json_object_get(
                                    profiles, stream_context_key(context));
                                if (json_is_object(profile))
                                    host.streamProfiles[context] =
                                        load_stream_profile(profile);
                            }
                        }

                        if (json_t* quality =
                                json_object_get(json, "stream_quality");
                            json_is_object(quality)) {
                            for (auto context : stream_contexts) {
                                json_t* aggregate = json_object_get(
                                    quality, stream_context_key(context));
                                if (json_is_object(aggregate))
                                    host.streamQuality[context] =
                                        load_stream_quality(aggregate);
                            }
                        }
                        
                        m_hosts.push_back(host);
                    }
                }
            }
        }
        
        if (json_t* settings = json_object_get(root, "settings")) {
            if (json_t* resolution = json_object_get(settings, "resolution")) {
                if (json_typeof(resolution) == JSON_INTEGER) {
                    m_resolution = (int)json_integer_value(resolution);
                }
            }

            if (json_t* native_resolution_scale = json_object_get(settings, "native_resolution_scale")) {
                if (json_typeof(native_resolution_scale) == JSON_INTEGER) {
                    set_native_resolution_scale((int)json_integer_value(native_resolution_scale));
                }
            }
            
            if (json_t* fps = json_object_get(settings, "fps")) {
                if (json_typeof(fps) == JSON_INTEGER) {
                    m_fps = (int)json_integer_value(fps);
                }
            }
            
            if (json_t* video_codec = json_object_get(settings, "video_codec")) {
                if (json_typeof(video_codec) == JSON_INTEGER) {
                    set_video_codec((VideoCodec)json_integer_value(video_codec));
                }
            }

            if (json_t* audio_backend = json_object_get(settings, "audio_backend")) {
                if (json_typeof(audio_backend) == JSON_INTEGER) {
                    m_audio_backend = (AudioBackend)json_integer_value(audio_backend);
                }
            }

#ifdef __SWITCH__
            m_audio_backend = AUDREN;
#endif

            if (json_t* bitrate = json_object_get(settings, "bitrate")) {
                if (json_typeof(bitrate) == JSON_INTEGER) {
                    m_bitrate = (int)json_integer_value(bitrate);
                }
            }

            if (json_t* enable_hdr = json_object_get(settings, "enable_hdr")) {
                m_enable_hdr = json_typeof(enable_hdr) == JSON_TRUE;
            }

            if (json_t* enable_upscaling = json_object_get(settings, "enable_upscaling")) {
                set_upscaling(json_typeof(enable_upscaling) == JSON_TRUE);
            }

            if (json_t* upscaling_mode = json_object_get(settings, "upscaling_mode")) {
                if (json_typeof(upscaling_mode) == JSON_INTEGER) {
                    set_upscaling_mode((UpscalingMode)json_integer_value(upscaling_mode));
                }
            }

            if (json_t* enable_dithering = json_object_get(settings, "enable_dithering")) {
                m_enable_dithering = json_typeof(enable_dithering) == JSON_TRUE;
            }

            if (json_t* dithering_strength = json_object_get(settings, "dithering_strength")) {
                if (json_typeof(dithering_strength) == JSON_INTEGER) {
                    m_dithering_strength = std::clamp((int)json_integer_value(dithering_strength), 1, 10);
                }
            }

            if (json_t* enable_rcas = json_object_get(settings, "enable_rcas")) {
                m_enable_rcas = json_typeof(enable_rcas) == JSON_TRUE;
            }

            if (json_t* rcas_strength = json_object_get(settings, "rcas_strength")) {
                if (json_typeof(rcas_strength) == JSON_INTEGER) {
                    m_rcas_strength = std::clamp((int)json_integer_value(rcas_strength), 0, 100);
                }
            }

            if (json_t* click_by_tap = json_object_get(settings, "click_by_tap")) {
                m_click_by_tap = json_typeof(click_by_tap) == JSON_TRUE;
            }
            
            if (json_t* decoder_threads = json_object_get(settings, "decoder_threads")) {
                if (json_typeof(decoder_threads) == JSON_INTEGER) {
                    m_decoder_threads = (int)json_integer_value(decoder_threads);
                }
            }

            if (json_t* frames_queue_size = json_object_get(settings, "frames_queue_size")) {
                if (json_typeof(frames_queue_size) == JSON_INTEGER) {
                    m_frames_queue_size = (int)json_integer_value(frames_queue_size);

                    // SANITY CHECK, APP WILL CRASH OTHERWISE
                    if (m_frames_queue_size < 1) m_frames_queue_size = 1;
                }
            }

            if (json_t* frame_pacing_mode = json_object_get(settings, "frame_pacing_mode")) {
                if (json_typeof(frame_pacing_mode) == JSON_INTEGER) {
                    set_frame_pacing_mode(
                        (FramePacingMode)json_integer_value(frame_pacing_mode));
                }
            }

            if (json_t* hw_decoding = json_object_get(settings, "use_hw_decoding")) {
                m_use_hw_decoding = json_typeof(hw_decoding) == JSON_TRUE;
            }

            if (json_t* sops = json_object_get(settings, "sops")) {
                m_sops = json_typeof(sops) == JSON_TRUE;
            }
            
            if (json_t* play_audio = json_object_get(settings, "play_audio")) {
                m_play_audio = json_typeof(play_audio) == JSON_TRUE;
            }
            
            if (json_t* write_log = json_object_get(settings, "write_log")) {
                m_write_log = json_typeof(write_log) == JSON_TRUE;
            }
            
            if (json_t* swap_ui_keys = json_object_get(settings, "swap_ui_keys")) {
                m_swap_ui_keys = json_typeof(swap_ui_keys) == JSON_TRUE;
            }

            if (json_t* swap_joycon_stick_to_dpad = json_object_get(settings, "swap_joycon_stick_to_dpad")) {
                m_swap_joycon_stick_to_dpad = json_typeof(swap_joycon_stick_to_dpad) == JSON_TRUE;
            }

            if (json_t* touchscreen_mouse_mode = json_object_get(settings, "touchscreen_mouse_mode")) {
                m_touchscreen_mouse_mode = json_typeof(touchscreen_mouse_mode) == JSON_TRUE;
            }
            
            if (json_t* swap_mouse_keys = json_object_get(settings, "swap_mouse_keys")) {
                m_swap_mouse_keys = json_typeof(swap_mouse_keys) == JSON_TRUE;
            }
            
            if (json_t* swap_mouse_scroll = json_object_get(settings, "swap_mouse_scroll")) {
                m_swap_mouse_scroll = json_typeof(swap_mouse_scroll) == JSON_TRUE;
            }
            
            if (json_t* volume_amplification = json_object_get(settings, "volume_amplification")) {
                m_volume_amplification = json_typeof(volume_amplification) == JSON_TRUE;
            }
            
            if (json_t* stream_volume = json_object_get(settings, "stream_volume")) {
                if (json_typeof(stream_volume) == JSON_INTEGER) {
                    m_volume = (int)json_integer_value(stream_volume);
                }
            }
            
            if (json_t* overlay_hold_time = json_object_get(settings, "overlay_hold_time")) {
                if (json_typeof(overlay_hold_time) == JSON_INTEGER) {
                    m_overlay_options.holdTime = (int)json_integer_value(overlay_hold_time);
                }
            }
            
            if (json_t* mouse_input_hold_time = json_object_get(settings, "mouse_input_hold_time")) {
                if (json_typeof(mouse_input_hold_time) == JSON_INTEGER) {
                    m_mouse_input_options.holdTime = (int)json_integer_value(mouse_input_hold_time);
                }
            }
            
            if (json_t* mouse_speed_multiplier = json_object_get(settings, "mouse_speed_multiplier")) {
                if (json_typeof(mouse_speed_multiplier) == JSON_INTEGER) {
                    m_mouse_speed_multiplier = (int)json_integer_value(mouse_speed_multiplier);
                }
            }

            if (json_t* deadzone_stick_left = json_object_get(settings, "deadzone_stick_left")) {
                if (json_typeof(deadzone_stick_left) == JSON_INTEGER) {
                    m_deadzone_stick_left = (float)json_integer_value(deadzone_stick_left) / 100.f;
                }
            }

            if (json_t* deadzone_stick_right = json_object_get(settings, "deadzone_stick_right")) {
                if (json_typeof(deadzone_stick_right) == JSON_INTEGER) {
                    m_deadzone_stick_right = (float)json_integer_value(deadzone_stick_right) / 100.f;
                }
            }
            
            if (json_t* rumble_force = json_object_get(settings, "rumble_force")) {
                if (json_typeof(rumble_force) == JSON_INTEGER) {
                    m_rumble_force = (int)json_integer_value(rumble_force);
                }
            }

            if (json_t* current_mapping_layout = json_object_get(settings, "current_mapping_layout")) {
                if (json_typeof(current_mapping_layout) == JSON_INTEGER) {
                    m_current_mapping_layout = (int)json_integer_value(current_mapping_layout);
                }
            }

            if (json_t* keyboard_type = json_object_get(settings, "keyboard_type")) {
                if (json_typeof(keyboard_type) == JSON_INTEGER) {
                    m_keyboard_type = (KeyboardType)json_integer_value(keyboard_type);
                }
            }

            if (json_t* keyboard_fingers = json_object_get(settings, "keyboard_fingers")) {
                if (json_typeof(keyboard_fingers) == JSON_INTEGER) {
                    m_keyboard_fingers = json_integer_value(keyboard_fingers);
                }
            }

            if (json_t* overlay_system_button = json_object_get(settings, "overlay_system_button")) {
                if (json_typeof(overlay_system_button) == JSON_INTEGER) {
                    m_overlay_system_button = (ButtonOverrideType) json_integer_value(overlay_system_button);
                }
            }

            if (json_t* guide_system_button = json_object_get(settings, "guide_system_button")) {
                if (json_typeof(guide_system_button) == JSON_INTEGER) {
                    m_guide_system_button = (ButtonOverrideType) json_integer_value(guide_system_button);
                }
            }

            load_button_array(settings, "overlay_buttons",
                              m_overlay_options.buttons);
            load_button_array(settings, "mouse_input_buttons",
                              m_mouse_input_options.buttons);
            load_button_array(settings, "guide_key_buttons",
                              m_guide_key_options.buttons);
            load_button_array(settings, "host_close_app_buttons",
                              m_host_close_app_options.buttons);
            load_button_array(settings, "host_switch_window_buttons",
                              m_host_switch_window_options.buttons);
            sanitizeHostShortcutOptions();
        }

        if (json_t* layouts = json_object_get(root, "mapping_layouts")) {
            size_t size = json_array_size(layouts);
            for (size_t i = 0; i < size; i++) {
                if (json_t* json = json_array_get(layouts, i)) {
                    if (json_typeof(json) == JSON_OBJECT) {
                        KeyMappingLayout layout;
                        layout.editable = true;

                        if (json_t* title = json_object_get(json, "title")) {
                            if (json_typeof(title) == JSON_STRING) {
                                layout.title = json_string_value(title);
                            }
                        }

                        if (json_t* mapping = json_object_get(json, "mapping")) {
                            const char *key;
                            json_t *value;
                            json_object_foreach(mapping, key, value) {
                                if (json_typeof(value) == JSON_STRING) {
                                    layout.mapping[std::atoi(key)] = std::atoi(json_string_value(value));
                                }
                            }
                        }

                        m_mapping_laouts.push_back(layout);
                    }
                }
            }
        }
        
        json_decref(root);
    }
}

void Settings::save() {
    json_t* root = json_object();
    
    if (root) {
        if (json_t* hosts = json_array()) {
            for (const auto& host: m_hosts) {
                if (json_t* json = json_object()) {
                    json_object_set_new(json, "address", json_string(host.address.c_str()));
                    json_object_set_new(json, "remote_address", json_string(host.remoteAddress.c_str()));
                    json_object_set_new(json, "hostname", json_string(host.hostname.c_str()));
                    json_object_set_new(json, "mac", json_string(host.mac.c_str()));
                    if (json_t* apps = json_array()) {
                        for (auto app: host.favorites) {
                            if (json_t* jsonApp = json_object()) {
                                json_object_set_new(jsonApp, "name", json_string(app.name.c_str()));
                                json_object_set_new(jsonApp, "id", json_integer(app.app_id));
                                json_array_append_new(apps, jsonApp);
                            }
                        }
                        json_object_set_new(json, "favorites", apps);
                    }

                    if (!host.streamProfiles.empty()) {
                        json_t* profiles = json_object();
                        for (const auto& [context, profile] : host.streamProfiles) {
                            json_object_set_new(profiles,
                                                stream_context_key(context),
                                                save_stream_profile(profile));
                        }
                        json_object_set_new(json, "stream_profiles", profiles);
                    }

                    if (!host.streamQuality.empty()) {
                        json_t* quality = json_object();
                        for (const auto& [context, aggregate] : host.streamQuality) {
                            json_object_set_new(quality,
                                                stream_context_key(context),
                                                save_stream_quality(aggregate));
                        }
                        json_object_set_new(json, "stream_quality", quality);
                    }
                    json_array_append_new(hosts, json);
                }
            }
            json_object_set_new(root, "hosts", hosts);
        }
        
        if (json_t* settings = json_object()) {
            json_object_set_new(settings, "resolution", json_integer(m_resolution));
            json_object_set_new(settings, "native_resolution_scale", json_integer(m_native_resolution_scale));
            json_object_set_new(settings, "fps", json_integer(m_fps));
            json_object_set_new(settings, "video_codec", json_integer(m_video_codec));
            json_object_set_new(settings, "audio_backend", json_integer(m_audio_backend));
            json_object_set_new(settings, "bitrate", json_integer(m_bitrate));
            json_object_set_new(settings, "decoder_threads", json_integer(m_decoder_threads));
            json_object_set_new(settings, "frames_queue_size", json_integer(m_frames_queue_size));
            json_object_set_new(settings, "frame_pacing_mode",
                                json_integer(static_cast<int>(m_frame_pacing_mode)));
            json_object_set_new(settings, "enable_hdr", m_enable_hdr ? json_true() : json_false());
            json_object_set_new(settings, "enable_upscaling", upscaling() ? json_true() : json_false());
            json_object_set_new(settings, "upscaling_mode", json_integer(upscaling_mode()));
            json_object_set_new(settings, "enable_dithering", m_enable_dithering ? json_true() : json_false());
            json_object_set_new(settings, "dithering_strength", json_integer(m_dithering_strength));
            json_object_set_new(settings, "enable_rcas", m_enable_rcas ? json_true() : json_false());
            json_object_set_new(settings, "rcas_strength", json_integer(m_rcas_strength));
            json_object_set_new(settings, "click_by_tap", m_click_by_tap ? json_true() : json_false());
            json_object_set_new(settings, "use_hw_decoding", m_use_hw_decoding ? json_true() : json_false());
            json_object_set_new(settings, "sops", m_sops ? json_true() : json_false());
            json_object_set_new(settings, "play_audio", m_play_audio ? json_true() : json_false());
            json_object_set_new(settings, "write_log", m_write_log ? json_true() : json_false());
            json_object_set_new(settings, "swap_ui_keys", m_swap_ui_keys ? json_true() : json_false());
            json_object_set_new(settings, "swap_joycon_stick_to_dpad", m_swap_joycon_stick_to_dpad ? json_true() : json_false());
            json_object_set_new(settings, "touchscreen_mouse_mode", m_touchscreen_mouse_mode ? json_true() : json_false());
            json_object_set_new(settings, "swap_mouse_keys", m_swap_mouse_keys ? json_true() : json_false());
            json_object_set_new(settings, "swap_mouse_scroll", m_swap_mouse_scroll ? json_true() : json_false());
            json_object_set_new(settings, "volume_amplification", m_volume_amplification ? json_true() : json_false());
            json_object_set_new(settings, "stream_volume", json_integer(m_volume));
            json_object_set_new(settings, "overlay_hold_time", json_integer(m_overlay_options.holdTime));
            json_object_set_new(settings, "mouse_input_hold_time", json_integer(m_mouse_input_options.holdTime));
            json_object_set_new(settings, "mouse_speed_multiplier", json_integer(m_mouse_speed_multiplier));
            json_object_set_new(settings, "deadzone_stick_left", json_integer(int(m_deadzone_stick_left * 100.f)));
            json_object_set_new(settings, "deadzone_stick_right", json_integer(int(m_deadzone_stick_right * 100.f)));
            json_object_set_new(settings, "rumble_force", json_integer(m_rumble_force));
            json_object_set_new(settings, "current_mapping_layout", json_integer(m_current_mapping_layout));
            json_object_set_new(settings, "keyboard_type", json_integer(m_keyboard_type));
            json_object_set_new(settings, "keyboard_fingers", json_integer(m_keyboard_fingers));
            json_object_set_new(settings, "overlay_system_button", json_integer((int)m_overlay_system_button));
            json_object_set_new(settings, "guide_system_button", json_integer((int)m_guide_system_button));

            save_button_array(settings, "overlay_buttons",
                              m_overlay_options.buttons);
            save_button_array(settings, "mouse_input_buttons",
                              m_mouse_input_options.buttons);
            save_button_array(settings, "guide_key_buttons",
                              m_guide_key_options.buttons);
            save_button_array(settings, "host_close_app_buttons",
                              m_host_close_app_options.buttons);
            save_button_array(settings, "host_switch_window_buttons",
                              m_host_switch_window_options.buttons);
            
            json_object_set_new(root, "settings", settings);
        }

        if (json_t* hosts = json_array()) {
            for (const auto& mappint_layout: m_mapping_laouts) {
                if (!mappint_layout.editable) continue;
                
                if (json_t* json = json_object()) {
                    json_object_set_new(json, "title", json_string(mappint_layout.title.c_str()));
                    if (json_t* mapping = json_object()) {
                        for (auto key: mappint_layout.mapping) {
                            json_object_set_new(mapping, std::to_string(key.first).c_str(), json_string(std::to_string(key.second).c_str()));
                        }
                        json_object_set_new(json, "mapping", mapping);
                    }
                    json_array_append_new(hosts, json);
                }
            }
            json_object_set_new(root, "mapping_layouts", hosts);
        }
        
        json_dump_file(root, settings_file_path(m_working_dir).c_str(), JSON_INDENT(4));
        json_decref(root);
    }
}

void Settings::loadBaseLayouts() {
    KeyMappingLayout defaultLayout {
        .title = "settings/keys_mapping_default"_i18n,
        .editable = false,
        .mapping = {}
    };
    KeyMappingLayout swapLayout {
        .title = "settings/keys_mapping_swap"_i18n,
        .editable = false,
        .mapping = { {ControllerButton::BUTTON_A, ControllerButton::BUTTON_B}, {ControllerButton::BUTTON_B, ControllerButton::BUTTON_A}, {ControllerButton::BUTTON_X, ControllerButton::BUTTON_Y}, {ControllerButton::BUTTON_Y, ControllerButton::BUTTON_X} }
    };

    m_mapping_laouts.push_back(defaultLayout);
    m_mapping_laouts.push_back(swapLayout);
}

int Settings::get_current_mapping_layout() {
    if (m_current_mapping_layout >= m_mapping_laouts.size())
        return 0;
    return m_current_mapping_layout;
}
