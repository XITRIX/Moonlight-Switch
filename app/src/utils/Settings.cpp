#include "Settings.hpp"
#include <jansson.h>
#include <algorithm>
#include <string.h>
#include <iomanip>
#include <limits.h>
#include <sys/stat.h>

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
    m_log_path = working_dir + "/log.txt";
    m_gamepad_mapping_path = working_dir + "/gamepad_mapping_v1.2.0.json";
    
    mkdirtree(m_working_dir.c_str());
    mkdirtree(m_key_dir.c_str());
    mkdirtree(m_boxart_dir.c_str());
    
    load();
}

void Settings::add_host(const Host& host) {
    remove_host(host);
    
    m_hosts.push_back(host);
    save();
}

void Settings::remove_host(const Host& host) {
    auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [host](auto h){
        return h.address == host.address || h.hostname == host.hostname || h.mac == host.mac;
    });
    
    if (it != m_hosts.end()) {
        m_hosts.erase(it);
        save();
    }
}

void Settings::load() {
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
                    json_object_set(json, "address", json_string(host.address.c_str()));
                    json_object_set(json, "hostname", json_string(host.hostname.c_str()));
                    json_object_set(json, "mac", json_string(host.mac.c_str()));
                    json_array_append(hosts, json);
                }
            }
            json_object_set(root, "hosts", hosts);
        }
        
        if (json_t* settings = json_object()) {
            json_object_set(settings, "resolution", json_integer(m_resolution));
            json_object_set(settings, "fps", json_integer(m_fps));
            json_object_set(settings, "video_codec", json_integer(m_video_codec));
            json_object_set(settings, "bitrate", json_integer(m_bitrate));
            json_object_set(settings, "ignore_unsupported_resolutions", m_ignore_unsupported_resolutions ? json_true() : json_false());
            json_object_set(settings, "decoder_threads", json_integer(m_decoder_threads));
            json_object_set(settings, "click_by_tap", m_click_by_tap ? json_true() : json_false());
            json_object_set(settings, "sops", m_sops ? json_true() : json_false());
            json_object_set(settings, "play_audio", m_play_audio ? json_true() : json_false());
            json_object_set(settings, "write_log", m_write_log ? json_true() : json_false());
            json_object_set(root, "settings", settings);
        }
        
        json_dump_file(root, (m_working_dir + "/settings.json").c_str(), JSON_INDENT(4));
        json_decref(root);
    }
}
