#pragma once

#include "Singleton.hpp"
#include <borealis.hpp>
#include <map>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

enum VideoCodec : int { H264, H265, AV1 };
std::string getVideoCodecName(VideoCodec codec);

enum AudioBackend : int {
    SDL,
#ifdef __SWITCH__
    AUDREN,
#endif
};

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

    [[nodiscard]] std::string key_dir() const { return m_key_dir; }

    [[nodiscard]] std::string boxart_dir() const { return m_boxart_dir; }

    [[nodiscard]] std::string log_path() const { return m_log_path; }

    [[nodiscard]] std::string gamepad_mapping_path() const { return m_gamepad_mapping_path; }

    [[nodiscard]] std::vector<Host> hosts() const { return m_hosts; }

    void add_host(const Host& host);
    void remove_host(const Host& host);

    void add_favorite(const Host& host, const App& app);
    void remove_favorite(const Host& host, int app_id);
    bool is_favorite(const Host& host, int app_id);
    bool has_any_favorite();

    [[nodiscard]] int resolution() const { return m_resolution; }
    void set_resolution(int resolution) { m_resolution = resolution; }

    [[nodiscard]] int fps() const { return m_fps; }
    void set_fps(int fps) { m_fps = fps; }

    [[nodiscard]] VideoCodec video_codec() const { return m_video_codec; }
    void set_video_codec(VideoCodec video_codec) { m_video_codec = video_codec; }

    [[nodiscard]] AudioBackend audio_backend() const { return m_audio_backend; }
    void set_audio_backend(AudioBackend audio_backend) { m_audio_backend = audio_backend; }

    [[nodiscard]] int bitrate() const { return m_bitrate; }
    void set_bitrate(int bitrate) { m_bitrate = bitrate; }

    [[nodiscard]] bool click_by_tap() const { return m_click_by_tap; }
    void set_click_by_tap(bool click_by_tap) { m_click_by_tap = click_by_tap; }

    void set_decoder_threads(int decoder_threads) { m_decoder_threads = decoder_threads; }
    [[nodiscard]] int decoder_threads() const { return m_decoder_threads; }

    void set_sops(bool sops) { m_sops = sops; }
    [[nodiscard]] bool sops() const { return m_sops; }

    void set_play_audio(bool play_audio) { m_play_audio = play_audio; }
    [[nodiscard]] bool play_audio() const { return m_play_audio; }

    void set_write_log(bool write_log) { m_write_log = write_log; }
    [[nodiscard]] bool write_log() const { return m_write_log; }

    void set_swap_ui_keys(bool swap_ui_keys) { m_swap_ui_keys = swap_ui_keys; }
    [[nodiscard]] bool swap_ui_keys() const { return m_swap_ui_keys; }

    void set_swap_joycon_stick_to_dpad(bool value) { m_swap_joycon_stick_to_dpad = value; }
    [[nodiscard]] bool swap_joycon_stick_to_dpad() const { return m_swap_joycon_stick_to_dpad; }

    void set_swap_mouse_keys(bool swap_mouse_keys) { m_swap_mouse_keys = swap_mouse_keys; }
    [[nodiscard]] bool touchscreen_mouse_mode() const { return m_touchscreen_mouse_mode; }

    void set_touchscreen_mouse_mode(bool touchscreen_mouse_mode) { m_touchscreen_mouse_mode = touchscreen_mouse_mode; }
    [[nodiscard]] bool swap_mouse_keys() const { return m_swap_mouse_keys; }

    void set_swap_mouse_scroll(bool swap_mouse_scroll) { m_swap_mouse_scroll = swap_mouse_scroll; }
    [[nodiscard]] bool swap_mouse_scroll() const { return m_swap_mouse_scroll; }

    void set_guide_key_options(KeyComboOptions options) { m_guide_key_options = std::move(options); }
    [[nodiscard]] KeyComboOptions guide_key_options() const { return m_guide_key_options; }

    void set_overlay_options(KeyComboOptions options) { m_overlay_options = std::move(options); }
    [[nodiscard]] KeyComboOptions overlay_options() const { return m_overlay_options; }

    void set_mouse_input_options(KeyComboOptions options) { m_mouse_input_options = std::move(options); }
    [[nodiscard]] KeyComboOptions mouse_input_options() const { return m_mouse_input_options; }

    void set_volume_amplification(bool allow) { m_volume_amplification = allow; }
    [[nodiscard]] bool get_volume_amplification() const { return m_volume_amplification; }

    void set_volume(int volume) { m_volume = volume; }
    [[nodiscard]] int get_volume() const { return m_volume; }

    void set_use_hw_decoding(bool hw_decoding) { m_use_hw_decoding = hw_decoding; }
    [[nodiscard]] bool use_hw_decoding() const { return m_use_hw_decoding; }

    void set_keyboard_type(KeyboardType type) { m_keyboard_type = type; }
    [[nodiscard]] KeyboardType get_keyboard_type() const { return m_keyboard_type; }

    void set_keyboard_fingers(int fingers) { m_keyboard_fingers = fingers; }
    [[nodiscard]] int get_keyboard_fingers() const { return m_keyboard_fingers; }

    void set_keyboard_locale(int locale) { m_keyboard_locale = locale; }
    [[nodiscard]] int get_keyboard_locale() const { return m_keyboard_locale; }

    void set_rumble_force(float rumble_force) { m_rumble_force = int(rumble_force * 100); }
    [[nodiscard]] float get_rumble_force() const { return float(m_rumble_force) / 100.f; }

    void set_mouse_speed_multiplier(int mouse_speed_multiplier) { m_mouse_speed_multiplier = mouse_speed_multiplier; }
    [[nodiscard]] int get_mouse_speed_multiplier() const { return m_mouse_speed_multiplier; }

    int get_current_mapping_layout();
    void set_current_mapping_layout(int layout) { m_current_mapping_layout = layout; }

    std::vector<KeyMappingLayout>* get_mapping_laouts() { return &m_mapping_laouts; }

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
    VideoCodec m_video_codec = H265;
    AudioBackend m_audio_backend = SDL;
    int m_bitrate = 10000;
    bool m_click_by_tap = false;
    int m_decoder_threads = 4;
    bool m_sops = true;
    bool m_play_audio = false;
    bool m_write_log = false;
    bool m_swap_ui_keys = false;
    bool m_swap_joycon_stick_to_dpad = false;
    bool m_touchscreen_mouse_mode = false;
    bool m_swap_mouse_keys = false;
    bool m_swap_mouse_scroll = false;
    int m_rumble_force = 100;
    int m_volume = 100;
    bool m_use_hw_decoding = true;
    KeyboardType m_keyboard_type = COMPACT;
    int m_keyboard_fingers = 3;
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
