project('moonlight', ['c', 'cpp'],
    version: '1.0.0',
    default_options: [ 'buildtype=release', 'strip=true', 'b_ndebug=if-release', 'cpp_std=c++17' ],
)

subdir('extern')

app_include = include_directories(
    'app/include',
    'app/include/gestures',
    'app/src/utils',
    'app/src/libgamestream',
    'app/src/crypto',
    'app/src/streaming',
    'app/src/streaming/audio',
    'app/src/streaming/ffmpeg',
    'app/src/streaming/video',
    'app/src/streaming/video/OpenGL',
    'app/src/streaming/switch',
)

app_files = files(
    'app/src/main.cpp',
    'app/src/add_host_tab.cpp',
    'app/src/app_cell.cpp',
    'app/src/app_list_view.cpp',
    'app/src/button_selecting_dialog.cpp',
    'app/src/grid_view.cpp',
    'app/src/helper.cpp',
    'app/src/host_tab.cpp',
    'app/src/main_tabs_view.cpp',
    'app/src/ingame_overlay_view.cpp',
    'app/src/keyboard_view.cpp',
    'app/src/loading_overlay.cpp',
    'app/src/main_activity.cpp',
    'app/src/settings_tab.cpp',
    'app/src/streaming_input_overlay.cpp',
    'app/src/streaming_view.cpp',
    'app/src/mapping_layout_editor.cpp',
    'app/src/favorite_tab.cpp',

    'app/src/gestures/fingers_gesture_recognizer.cpp',
    'app/src/gestures/click_gesture_recognizer.cpp',
    'app/src/gestures/two_finger_scroll_recognizer.cpp',

    'app/src/crypto/Data.cpp',
    'app/src/crypto/MbedTLSCryptoManager.cpp',

    'app/src/keyboards/english_layout.cpp',
    'app/src/keyboards/numpad_layout.cpp',

    'app/src/libgamestream/client.cpp',
    'app/src/libgamestream/http.cpp',
    'app/src/libgamestream/xml.cpp',

    'app/src/streaming/DiscoverManager.cpp',
    'app/src/streaming/GameStreamClient.cpp',
    'app/src/streaming/InputManager.cpp',
    'app/src/streaming/MoonlightSession.cpp',
    'app/src/streaming/WakeOnLanManager.cpp',

    'app/src/streaming/audio/SDLAudioRenderer.cpp',
    'app/src/streaming/ffmpeg/FFmpegVideoDecoder.cpp',
    'app/src/streaming/switch/SwitchMoonlightSessionDecoderAndRenderProvider.cpp',
    'app/src/streaming/video/OpenGL/GLVideoRenderer.cpp',

    'app/src/utils/BoxArtManager.cpp',
    'app/src/utils/Settings.cpp',
)

add_project_arguments('-DUSE_MBEDTLS', language : 'c')

moonlight = executable(
    'moonlight',
    [ app_files, borealis_files, moonlight_common_c_files ],
    dependencies : [borealis_dependencies, moonlight_dependencies],
    install: true,
    include_directories: [ borealis_include, app_include, moonlight_common_c_include, zeroconf_include ],
    cpp_args: [ '-static', '-w', '-g', '-O2', '-DAPP_VERSION="0.13.1"', '-DBRLS_RESOURCES="./resources/"', '-DBRLS_I18N_PREFIX="main/"', ] + moonlight_cpp_args + borealis_cpp_args
)
