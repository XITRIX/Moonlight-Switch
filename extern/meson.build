subdir('borealis/library')

zeroconf_include = include_directories(
    'zeroconf',
)

moonlight_common_c_include = include_directories(
    'moonlight-common-c/enet/include',
    'moonlight-common-c/reedsolomon',
    'moonlight-common-c/src',
)

moonlight_common_c_files = files(
    'moonlight-common-c/enet/callbacks.c',
    'moonlight-common-c/enet/compress.c',
    'moonlight-common-c/enet/host.c',
    'moonlight-common-c/enet/list.c',
    'moonlight-common-c/enet/packet.c',
    'moonlight-common-c/enet/peer.c',
    'moonlight-common-c/enet/protocol.c',
    'moonlight-common-c/enet/unix.c',
    'moonlight-common-c/enet/win32.c',
    'moonlight-common-c/enet/list.c',

    'moonlight-common-c/reedsolomon/rs.c',

    'moonlight-common-c/src/AudioStream.c',
    'moonlight-common-c/src/ByteBuffer.c',
    'moonlight-common-c/src/Connection.c',
    'moonlight-common-c/src/ControlStream.c',
    'moonlight-common-c/src/FakeCallbacks.c',
    'moonlight-common-c/src/InputStream.c',
    'moonlight-common-c/src/LinkedBlockingQueue.c',
    'moonlight-common-c/src/Misc.c',
    'moonlight-common-c/src/PlatformCrypto.c',
    'moonlight-common-c/src/PlatformSockets.c',
    'moonlight-common-c/src/PlatformStuff.c',
    'moonlight-common-c/src/RtpAudioQueue.c',
    'moonlight-common-c/src/RtspConnection.c',
    'moonlight-common-c/src/RtspParser.c',
    'moonlight-common-c/src/SdpGenerator.c',
    'moonlight-common-c/src/SimpleStun.c',
    'moonlight-common-c/src/VideoDepacketizer.c',
    'moonlight-common-c/src/VideoStream.c',

    'moonlight-common-c/src/RtpVideoQueue.c',
    'moonlight-common-c/src/RtpVideoQueue.c',
    'moonlight-common-c/src/RtpVideoQueue.c',
    'moonlight-common-c/src/RtpVideoQueue.c',
    'moonlight-common-c/src/RtpVideoQueue.c',
    'moonlight-common-c/src/RtpVideoQueue.c',
    'moonlight-common-c/src/RtpVideoQueue.c',
)

dep_opus = dependency('opus', static : true)
dep_expat = dependency('expat', static : true)
dep_jansson = dependency('jansson', static : true)
dep_sdl2 = dependency('sdl2', static : true)
dep_avcodec = dependency('libavcodec')
dep_avutil = dependency('libavutil', static : true)
dep_avformat = dependency('libavformat', static : true)

compiler = meson.get_compiler('c')

dep_curl = [ compiler.find_library('curl') ]
dep_mbedtls = [ compiler.find_library('mbedtls', static : true) ]
dep_mbedx509 = [ compiler.find_library('mbedx509', static : true) ]
dep_mbedcrypto = [ compiler.find_library('mbedcrypto', static : true) ]

dep_win32 = [ compiler.find_library('ws2_32', static : true) ]
dep_libwinmm = [ compiler.find_library('libwinmm', static : true) ]
dep_libwlan = [ compiler.find_library('libwlanapi', static : true) ]


moonlight_dependencies = [ dep_win32, dep_libwinmm, dep_curl, dep_mbedtls, dep_mbedx509, dep_mbedcrypto, dep_avcodec, dep_avformat, dep_avutil, dep_opus, dep_expat, dep_jansson, dep_sdl2, ]
moonlight_cpp_args = [ '-DUSE_MBEDTLS_CRYPTO', '-DMULTICAST_DISABLED', ]