#pragma once

#include "Singleton.hpp"
#include <borealis.hpp>
#include <map>
#include <stdio.h>
#include <string>
#include <vector>

enum VideoCodec : int { H264, H265 };

enum KeyboardType : int { COMPACT, FULLSIZED };

struct KeyMappingLayout {
    std::string title;
    bool editable;
    std::map<int, int> mapping;
};

struct KeyComboOptions {
    int holdTime;
    std::vector<brls::ControllerButton> buttons;
};

struct App {
    std::string name;
    int app_id;
};

struct Host {
    std::string address;
    std::string hostname;
    std::string mac;
    std::vector<App> favorites;
};

class Settings : public Singleton<Settings> {
  public:
    void set_working_dir(std::string working_dir);

    std::string key_dir() const { return m_key_dir; }

    std::string boxart_dir() const { return m_boxart_dir; }

    std::string log_path() const { return m_log_path; }

    std::string gamepad_mapping_path() const { return m_gamepad_mapping_path; }

    std::vector<Host> hosts() const { return m_hosts; }

    void add_host(const Host& host);
    void remove_host(const Host& host);

    void add_favorite(const Host& host, const App& app);
    void remove_favorite(const Host& host, int app_id);
    bool is_favorite(const Host& host, int app_id);
    bool has_any_favorite();

    int resolution() const { return m_resolution; }

    void set_resolution(int resolution) { m_resolution = resolution; }

    int fps() const { return m_fps; }

    void set_fps(int fps) { m_fps = fps; }

    VideoCodec video_codec() const { return m_video_codec; }

    void set_video_codec(VideoCodec video_codec) {
        m_video_codec = video_codec;
    }

    int bitrate() const { return m_bitrate; }

    void set_bitrate(int bitrate) { m_bitrate = bitrate; }

    void
    set_ignore_unsupported_resolutions(bool ignore_unsupported_resolutions) {
        m_ignore_unsupported_resolutions = ignore_unsupported_resolutions;
    }

    bool ignore_unsupported_resolutions() const {
        return m_ignore_unsupported_resolutions;
    }

    bool click_by_tap() const { return m_click_by_tap; }

    void set_click_by_tap(bool click_by_tap) { m_click_by_tap = click_by_tap; }

    void set_decoder_threads(int decoder_threads) {
        m_decoder_threads = decoder_threads;
    }

    int decoder_threads() const { return m_decoder_threads; }

    void set_sops(bool sops) { m_sops = sops; }

    bool sops() const { return m_sops; }

    void set_play_audio(bool play_audio) { m_play_audio = play_audio; }

    bool play_audio() const { return m_play_audio; }

    void set_write_log(bool write_log) { m_write_log = write_log; }

    bool write_log() const { return m_write_log; }

    void set_swap_ui_keys(bool swap_ui_keys) { m_swap_ui_keys = swap_ui_keys; }

    bool swap_ui_keys() const { return m_swap_ui_keys; }

    void set_swap_game_keys(bool swap_game_keys) {
        m_swap_game_keys = swap_game_keys;
    }

    bool swap_game_keys() const { return m_swap_game_keys; }

    void set_swap_mouse_keys(bool swap_mouse_keys) {
        m_swap_mouse_keys = swap_mouse_keys;
    }

    bool touchscreen_mouse_mode() const { return m_touchscreen_mouse_mode; }

    void set_touchscreen_mouse_mode(bool touchscreen_mouse_mode) {
        m_touchscreen_mouse_mode = touchscreen_mouse_mode;
    }

    bool swap_mouse_keys() const { return m_swap_mouse_keys; }

    void set_swap_mouse_scroll(bool swap_mouse_scroll) {
        m_swap_mouse_scroll = swap_mouse_scroll;
    }

    bool swap_mouse_scroll() const { return m_swap_mouse_scroll; }

    void set_guide_key_options(KeyComboOptions options) {
        m_guide_key_options = options;
    }

    KeyComboOptions guide_key_options() const { return m_guide_key_options; }

    void set_overlay_options(KeyComboOptions options) {
        m_overlay_options = options;
    }

    KeyComboOptions overlay_options() const { return m_overlay_options; }

    void set_mouse_input_options(KeyComboOptions options) {
        m_mouse_input_options = options;
    }

    KeyComboOptions mouse_input_options() const {
        return m_mouse_input_options;
    }

    void set_volume_amplification(bool allow) {
        m_volume_amplification = allow;
    }

    bool get_volume_amplification() const { return m_volume_amplification; }

    void set_volume(int volume) { m_volume = volume; }

    int get_volume() const { return m_volume; }

    void set_keyboard_type(KeyboardType type) { m_keyboard_type = type; }

    KeyboardType get_keyboard_type() { return m_keyboard_type; }

    void set_keyboard_locale(int locale) { m_keyboard_locale = locale; }

    int get_keyboard_locale() { return m_keyboard_locale; }

    void set_mouse_speed_multiplier(int mouse_speed_multiplier) {
        m_mouse_speed_multiplier = mouse_speed_multiplier;
    }

    int get_mouse_speed_multiplier() const { return m_mouse_speed_multiplier; }

    int get_current_mapping_layout();

    void set_current_mapping_layout(int layout) {
        m_current_mapping_layout = layout;
    }

    std::vector<KeyMappingLayout>* get_mapping_laouts() {
        return &m_mapping_laouts;
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
    bool m_swap_ui_keys = false;
    bool m_swap_game_keys = false;
    bool m_touchscreen_mouse_mode = false;
    bool m_swap_mouse_keys = false;
    bool m_swap_mouse_scroll = false;
    int m_volume = 100;
    KeyboardType m_keyboard_type = COMPACT;
    int m_keyboard_locale = 0;
    bool m_volume_amplification = false;
    int m_mouse_speed_multiplier = 34;
    int m_current_mapping_layout = 0;
    std::vector<KeyMappingLayout> m_mapping_laouts;
    KeyComboOptions m_guide_key_options{
        .holdTime = 0,
        .buttons = {},
    };
    KeyComboOptions m_overlay_options{
        .holdTime = 0,
        .buttons = {brls::ControllerButton::BUTTON_BACK,
                    brls::ControllerButton::BUTTON_START},
    };
    KeyComboOptions m_mouse_input_options{
        .holdTime = 0,
        .buttons = {},
    };

    void loadBaseLayouts();
};
