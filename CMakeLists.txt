cmake_minimum_required(VERSION 3.10)

set(EXTERN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/extern)
set(BOREALIS_LIBRARY ${EXTERN_PATH}/borealis/library)

# build options
include(${EXTERN_PATH}/cmake/commonOption.cmake)

# Dependencies
option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)

if (APPLE AND PLATFORM_DESKTOP)
    option(BUNDLE_MACOS_APP "Bundle a app for macOS" OFF)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET[arch=arm64] "11.0" CACHE STRING "" FORCE)
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
endif ()

if (PLATFORM_IOS)
    set(ENABLE_ARC OFF)
endif ()

# toolchain
include(${EXTERN_PATH}/cmake/toolchain.cmake)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${EXTERN_PATH}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${EXTERN_PATH}/cmake")
#find_package(PkgConfig REQUIRED)

# project info
project(Moonlight)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "2")
set(VERSION_ALTER "0")
set(VERSION_BUILD "1")
set(PACKAGE_NAME "com.xitrix.moonlight")
set(PSN_TITLE_ID  "MNTL00000")
set(PSN_VERSION  "01.00")
set(PROJECT_AUTHOR "XITRIX")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/moonlight_icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

add_definitions(
        -DAPP_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
        -Dcimg_display=0
        -Dcimg_use_png
        -Dcimg_no_system_calls
        -DBRLS_I18N_PREFIX=\"main/\")

if (PLATFORM_PSV OR PLATFORM_ANDROID)
    set(USE_OPENSSL_CRYPTO ON)
else ()
    set(USE_MBEDTLS_CRYPTO ON)
endif ()

if (PLATFORM_IOS OR PLATFORM_ANDROID)
    set(MULTICAST_DISABLED ON)
endif ()

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()

if (USE_MBEDTLS_CRYPTO)
    find_package(MbedTLS REQUIRED)
    add_compile_definitions(USE_MBEDTLS_CRYPTO)
elseif (USE_OPENSSL_CRYPTO)
    find_package(OpenSSL REQUIRED)
    add_definitions(-DUSE_OPENSSL_CRYPTO)
endif ()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(extern)

# setting src and include
file(GLOB_RECURSE MAIN_SRC app/src/*.cpp)

set(PLATFORM_OPTION)
if (PLATFORM_DESKTOP)
    if (WIN32)
        configure_file("${CMAKE_SOURCE_DIR}/app/resource.rc.in" "${CMAKE_BINARY_DIR}/resource.rc")
        list(APPEND MAIN_SRC ${CMAKE_BINARY_DIR}/resource.rc)
    elseif (BUNDLE_MACOS_APP)
        list(APPEND MAIN_SRC ${CMAKE_SOURCE_DIR}/app/borealis.icns)
    endif ()
elseif (PLATFORM_SWITCH)
    set(PLATFORM_LIBS
        # needed by borealis
        EGL glapi drm_nouveau
        # base lib
        nx m
    )
    list(APPEND MAIN_SRC
            ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c
            app/src/switch/wrapper.c
            )
elseif (PLATFORM_PSV)
    add_definitions(-D__psp2__ -D__PSV__)
elseif (PLATFORM_ANDROID)
endif ()


# building target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)


# building release file
if (PLATFORM_DESKTOP)
    if (BUNDLE_MACOS_APP)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
            MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}"
            MACOSX_BUNDLE_ICON_FILE "borealis.icns"
            MACOSX_BUNDLE_COPYRIGHT "Copyright 2023 ${PROJECT_AUTHOR}"
            RESOURCE "${CMAKE_SOURCE_DIR}/app/borealis.icns"
        )
        add_custom_target(${PROJECT_NAME}.data
                COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${PROJECT_NAME}.app/Contents/Resources/resources
        )
    else ()
        add_custom_target(${PROJECT_NAME}.data
                COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
        )
    endif ()
    if (NOT USE_LIBROMFS)
        add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
    endif ()
elseif (PLATFORM_PSV)
    set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12") # max heap size mode
    vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
    if (USE_LIBROMFS)
        vita_create_vpk(${PROJECT_NAME}.vpk ${PSN_TITLE_ID} ${PROJECT_NAME}.self
            VERSION ${PSN_VERSION}
            NAME ${PROJECT_NAME}
            FILE ${CMAKE_SOURCE_DIR}/psv/sce_sys sce_sys
            FILE ${CMAKE_SOURCE_DIR}/psv/module/ module
        )
    else()
        vita_create_vpk(${PROJECT_NAME}.vpk ${PSN_TITLE_ID} ${PROJECT_NAME}.self
            VERSION ${PSN_VERSION}
            NAME ${PROJECT_NAME}
            FILE ${CMAKE_SOURCE_DIR}/resources resources
            FILE ${CMAKE_SOURCE_DIR}/psv/sce_sys sce_sys
            FILE ${CMAKE_SOURCE_DIR}/psv/module/ module
        )
    endif()
elseif (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        gen_dksh("${PROJECT_RESOURCES}/shaders")
    endif ()
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
    list(APPEND APP_PLATFORM_LIB swresample dav1d z)
elseif (PLATFORM_ANDROID)
    list(APPEND APP_PLATFORM_LIB android)
elseif (PLATFORM_IOS)
    ios_bundle(
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/tvos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iphoneos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/Images.xcassets"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iOSBundleInfo.plist.in"
            ${PROJECT_NAME}
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
    list(APPEND IOS_FRAMEWORKS ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/MetalANGLE.framework)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON)
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC
        app/include
        app/include/gestures
        app/src/crypto
        app/src/gestures
        app/src/keyboards
        app/src/libgamestream
        app/src/streaming
        app/src/streaming/audio
        app/src/streaming/ffmpeg
        app/src/streaming/switch
        app/src/streaming/video
        app/src/streaming/video/deko3d
        app/src/streaming/video/OpenGL
        app/src/utils
        extern/CImg
        ${MBEDTLS_INCLUDE_DIRS}
        ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})

find_package(CURL REQUIRED)
#find_package(MbedTLS REQUIRED)
if(NOT PLATFORM_ANDROID)
    find_package(SDL2 REQUIRED)
endif()
find_package(Jansson REQUIRED)
find_package(EXPAT REQUIRED)
find_package(PNG REQUIRED)

find_library(OPUS_LIBRARY opus)
message("opus: ${OPUS_LIBRARY}")

find_library(AVCODEC_LIBRARY avcodec)
message("avcodec: ${AVCODEC_LIBRARY}")

find_library(AVFORMAT_LIBRARY avformat)
message("avformat: ${AVFORMAT_LIBRARY}")

find_library(AVUTIL_LIBRARY avutil)
message("avutil: ${AVUTIL_LIBRARY}")

find_library(SWRESAMPLE_LIBRARY swresample)
message("swresample: ${SWRESAMPLE_LIBRARY}")

# Recompiled CURL with Mbedtls support for Switch
if(PLATFORM_SWITCH)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libcurl.a)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE
        borealis
        moonlight-common-c
        Jansson::Jansson
        EXPAT::EXPAT
        ${AVCODEC_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${AVFORMAT_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
        ${OPUS_LIBRARY}
        PNG::PNG
        SDL2::SDL2
        ${APP_PLATFORM_LIB})

if (MULTICAST_DISABLED)
    add_definitions(-DMULTICAST_DISABLED)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE mdns)
endif ()

if (USE_MBEDTLS_CRYPTO)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            mbed::tls
            mbed::x509
            mbed::crypto)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE
            OpenSSL::SSL
            OpenSSL::Crypto)
endif ()

if (PLATFORM_IOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_EMBED_FRAMEWORKS "${IOS_FRAMEWORKS}")
endif()

if (APPLE)
    set(XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC OFF)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreMedia" "-framework VideoToolbox")
elseif (PLATFORM_PSV)
    target_link_libraries(${PROJECT_NAME} PRIVATE mp3lame libGLESv2_stub)
endif ()