#include "Singleton.hpp"
#include <stdio.h>
#include <string>
#include <vector>
#pragma once

enum VideoCodec: int {
    H264,
    H265
};

struct Host {
    std::string address;
    std::string hostname;
    std::string mac;
};

class Settings: public Singleton<Settings> {
public:
    void set_working_dir(std::string working_dir);
    
    std::string key_dir() const {
        return m_key_dir;
    }
    
    std::string boxart_dir() const {
        return m_boxart_dir;
    }
    
    std::string log_path() const {
        return m_log_path;
    }
    
    std::string gamepad_mapping_path() const {
        return m_gamepad_mapping_path;
    }
    
    std::vector<Host> hosts() const {
        return m_hosts;
    }
    
    void add_host(const Host& host);
    void remove_host(const Host& host);
    
    int resolution() const {
        return m_resolution;
    }
    
    void set_resolution(int resolution) {
        m_resolution = resolution;
    }
    
    int fps() const {
        return m_fps;
    }
    
    void set_fps(int fps) {
        m_fps = fps;
    }
    
    VideoCodec video_codec() const {
        return m_video_codec;
    }
    
    void set_video_codec(VideoCodec video_codec) {
        m_video_codec = video_codec;
    }
    
    int bitrate() const {
        return m_bitrate;
    }
    
    void set_bitrate(int bitrate) {
        m_bitrate = bitrate;
    }
    
    void set_ignore_unsupported_resolutions(bool ignore_unsupported_resolutions) {
        m_ignore_unsupported_resolutions = ignore_unsupported_resolutions;
    }
    
    bool ignore_unsupported_resolutions() const {
        return m_ignore_unsupported_resolutions;
    }
    
    bool click_by_tap() const {
        return m_click_by_tap;
    }
    
    void set_click_by_tap(bool click_by_tap) {
        m_click_by_tap = click_by_tap;
    }
    
    void set_decoder_threads(int decoder_threads) {
        m_decoder_threads = decoder_threads;
    }
    
    int decoder_threads() const {
        return m_decoder_threads;
    }
    
    void set_sops(bool sops) {
        m_sops = sops;
    }
    
    bool sops() const {
        return m_sops;
    }
    
    void set_play_audio(bool play_audio) {
        m_play_audio = play_audio;
    }
    
    bool play_audio() const {
        return m_play_audio;
    }
    
    void set_write_log(bool write_log) {
        m_write_log = write_log;
    }
    
    bool write_log() const {
        return m_write_log;
    }
    
    void load();
    void save();

private:
    std::string m_working_dir;
    std::string m_key_dir;
    std::string m_boxart_dir;
    std::string m_log_path;
    std::string m_gamepad_mapping_path;
    
    std::vector<Host> m_hosts;
    int m_resolution = 720;
    int m_fps = 60;
    VideoCodec m_video_codec = H264;
    int m_bitrate = 10000;
    bool m_ignore_unsupported_resolutions = false;
    bool m_click_by_tap = false;
    int m_decoder_threads = 4;
    bool m_sops = true;
    bool m_play_audio = false;
    bool m_write_log = false;
};
