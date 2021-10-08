#include "Settings.hpp"
#include <jansson.h>
#include <algorithm>
#include <string.h>
#include <iomanip>
#include <limits.h>
#include <sys/stat.h>

using namespace brls;

#ifdef _WIN32
static int mkdir(const char* dir, mode_t mode) {
    return mkdir(dir);
}
#endif

static int mkdirtree(const char* directory) {
    char buffer[PATH_MAX];
    char* p = buffer;
    
    // The passed in string could be a string literal
    // so we must copy it first
    strncpy(p, directory, PATH_MAX - 1);
    buffer[PATH_MAX - 1] = '\0';
    
    while (*p != 0) {
        // Find the end of the path element
        do {
            p++;
        } while (*p != 0 && *p != '/');
        
        char oldChar = *p;
        *p = 0;
        
        // Create the directory if it doesn't exist already
        if (mkdir(buffer, 0775) == -1 && errno != EEXIST) {
            return -1;
        }
        
        *p = oldChar;
    }
    return 0;
}

void Settings::set_working_dir(std::string working_dir) {
    m_working_dir = working_dir;
    m_key_dir = working_dir + "/key";
    m_boxart_dir = working_dir + "/boxart";
    m_log_path = working_dir + "/log.log";
    m_gamepad_mapping_path = working_dir + "/gamepad_mapping_v1.2.0.json";
    
    mkdirtree(m_working_dir.c_str());
    mkdirtree(m_key_dir.c_str());
    mkdirtree(m_boxart_dir.c_str());
    
    load();
}

void Settings::add_host(const Host& host) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](auto h){
        return h.mac == host.mac;
    });

    if (it != m_hosts.end()) {
        it->address = host.address;
        it->hostname = host.hostname;
    } else if (!host.address.empty() && !host.mac.empty()) {
        m_hosts.push_back(host);
    }

    save();
}

void Settings::remove_host(const Host& host) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](auto h){
        return h.mac == host.mac;
    });
    
    if (it != m_hosts.end()) {
        m_hosts.erase(it);
        save();
    }
}

void Settings::add_favorite(const Host& host, const App& app) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](auto h){
        return h.mac == host.mac;
    });

    if (it != m_hosts.end()) {
        auto app_it = std::find_if(it->favorites.begin(), it->favorites.end(), [app](auto h){
            return h.app_id == app.app_id;
        });

        if (app_it != it->favorites.end()) {
            it->favorites.erase(app_it);
        }
        
        it->favorites.push_back(app);
        save();
    }
}

void Settings::remove_favorite(const Host& host, int app_id) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](auto h){
        return h.mac == host.mac;
    });

    if (it != m_hosts.end()) {
        auto app_it = std::find_if(it->favorites.begin(), it->favorites.end(), [app_id](auto h){
            return h.app_id == app_id;
        });

        if (app_it != it->favorites.end()) {
            it->favorites.erase(app_it);
            save();
        }
    }
}

bool Settings::is_favorite(const Host& host, int app_id) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](auto h){
        return h.mac == host.mac;
    });

    if (it != m_hosts.end()) {
        auto app_it = std::find_if(it->favorites.begin(), it->favorites.end(), [app_id](auto h){
            return h.app_id == app_id;
        });

        if (app_it != it->favorites.end()) {
            return true;
        }
    }

    return false;
}

bool Settings::has_any_favorite() {
    for (auto host : m_hosts) {
        if (!host.favorites.empty())
            return true;
    }
    return false;
}

void Settings::load() {
    loadBaseLayouts();

    json_t* root = json_load_file((m_working_dir + "/settings.json").c_str(), 0, NULL);
    
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
            
            if (json_t* fps = json_object_get(settings, "fps")) {
                if (json_typeof(fps) == JSON_INTEGER) {
                    m_fps = (int)json_integer_value(fps);
                }
            }
            
            if (json_t* video_codec = json_object_get(settings, "video_codec")) {
                if (json_typeof(video_codec) == JSON_INTEGER) {
                    m_video_codec = (VideoCodec)json_integer_value(video_codec);
                }
            }
            
            if (json_t* bitrate = json_object_get(settings, "bitrate")) {
                if (json_typeof(bitrate) == JSON_INTEGER) {
                    m_bitrate = (int)json_integer_value(bitrate);
                }
            }
            
            if (json_t* ignore_unsupported_resolutions = json_object_get(settings, "ignore_unsupported_resolutions")) {
                m_ignore_unsupported_resolutions = json_typeof(ignore_unsupported_resolutions) == JSON_TRUE;
            }
            
            if (json_t* click_by_tap = json_object_get(settings, "click_by_tap")) {
                m_click_by_tap = json_typeof(click_by_tap) == JSON_TRUE;
            }
            
            if (json_t* decoder_threads = json_object_get(settings, "decoder_threads")) {
                if (json_typeof(decoder_threads) == JSON_INTEGER) {
                    m_decoder_threads = (int)json_integer_value(decoder_threads);
                }
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
            
            if (json_t* swap_game_keys = json_object_get(settings, "swap_game_keys")) {
                m_swap_game_keys = json_typeof(swap_game_keys) == JSON_TRUE;
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
            
            if (json_t* mouse_speed_multiplier = json_object_get(settings, "mouse_speed_multiplier")) {
                if (json_typeof(mouse_speed_multiplier) == JSON_INTEGER) {
                    m_mouse_speed_multiplier = (int)json_integer_value(mouse_speed_multiplier);
                }
            }
            
            if (json_t* buttons = json_object_get(settings, "overlay_buttons")) {
                m_overlay_options.buttons.clear();
                size_t size = json_array_size(buttons);
                for (size_t i = 0; i < size; i++) {
                    if (json_t* j_button = json_array_get(buttons, i)) {
                        brls::ControllerButton button;
                        if (json_typeof(j_button) == JSON_INTEGER) {
                            button = (brls::ControllerButton)json_integer_value(j_button);
                            m_overlay_options.buttons.push_back(button);
                        }
                    }
                }
            }
            
            if (json_t* buttons = json_object_get(settings, "guide_key_buttons")) {
                m_guide_key_options.buttons.clear();
                size_t size = json_array_size(buttons);
                for (size_t i = 0; i < size; i++) {
                    if (json_t* j_button = json_array_get(buttons, i)) {
                        brls::ControllerButton button;
                        if (json_typeof(j_button) == JSON_INTEGER) {
                            button = (brls::ControllerButton)json_integer_value(j_button);
                            m_guide_key_options.buttons.push_back(button);
                        }
                    }
                }
            }
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
            for (auto host: m_hosts) {
                if (json_t* json = json_object()) {
                    json_object_set_new(json, "address", json_string(host.address.c_str()));
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
                    json_array_append_new(hosts, json);
                }
            }
            json_object_set_new(root, "hosts", hosts);
        }
        
        if (json_t* settings = json_object()) {
            json_object_set_new(settings, "resolution", json_integer(m_resolution));
            json_object_set_new(settings, "fps", json_integer(m_fps));
            json_object_set_new(settings, "video_codec", json_integer(m_video_codec));
            json_object_set_new(settings, "bitrate", json_integer(m_bitrate));
            json_object_set_new(settings, "ignore_unsupported_resolutions", m_ignore_unsupported_resolutions ? json_true() : json_false());
            json_object_set_new(settings, "decoder_threads", json_integer(m_decoder_threads));
            json_object_set_new(settings, "click_by_tap", m_click_by_tap ? json_true() : json_false());
            json_object_set_new(settings, "sops", m_sops ? json_true() : json_false());
            json_object_set_new(settings, "play_audio", m_play_audio ? json_true() : json_false());
            json_object_set_new(settings, "write_log", m_write_log ? json_true() : json_false());
            json_object_set_new(settings, "swap_ui_keys", m_swap_ui_keys ? json_true() : json_false());
            json_object_set_new(settings, "swap_game_keys", m_swap_game_keys ? json_true() : json_false());
            json_object_set_new(settings, "swap_mouse_keys", m_swap_mouse_keys ? json_true() : json_false());
            json_object_set_new(settings, "swap_mouse_scroll", m_swap_mouse_scroll ? json_true() : json_false());
            json_object_set_new(settings, "volume_amplification", m_volume_amplification ? json_true() : json_false());
            json_object_set_new(settings, "stream_volume", json_integer(m_volume));
            json_object_set_new(settings, "overlay_hold_time", json_integer(m_overlay_options.holdTime));
            json_object_set_new(settings, "mouse_speed_multiplier", json_integer(m_mouse_speed_multiplier));
            
            if (json_t* overlayButtons = json_array()) {
                for (auto button: m_overlay_options.buttons) {
                    json_array_append_new(overlayButtons, json_integer(button));
                }
                json_object_set_new(settings, "overlay_buttons", overlayButtons);
            }
            
            if (json_t* guideKeyButtons = json_array()) { 
                for (auto button: m_guide_key_options.buttons) {
                    json_array_append_new(guideKeyButtons, json_integer(button));
                }
                json_object_set_new(settings, "guide_key_buttons", guideKeyButtons);
            }
            
            json_object_set_new(root, "settings", settings);
        }

        if (json_t* hosts = json_array()) {
            for (auto mappint_layout: m_mapping_laouts) {
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
        
        json_dump_file(root, (m_working_dir + "/settings.json").c_str(), JSON_INDENT(4));
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
