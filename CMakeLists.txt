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
    option(BUNDLE_MACOS_APP "Bundle a app for macOS" ON)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
    set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
endif ()

if (PLATFORM_IOS OR PLATFORM_TVOS OR PLATFORM_VISIONOS)
    # set(ENABLE_ARC OFF)
endif ()

# toolchain
include(${EXTERN_PATH}/cmake/toolchain.cmake)

# Renderers
if (PLATFORM_APPLE)
    set(USE_METAL_RENDERER ON)
elseif (PLATFORM_SWITCH)
    set(USE_DEKO3D_RENDERER ON)
elseif (WIN32 AND USE_D3D11)
    set(USE_D3D11_RENDERER ON)
else ()
    set(USE_GL_RENDERER ON)
endif ()

set(SUPPORT_UPSCALING OFF)
if (ENABLE_UPSCALING)
    message(STATUS "Use ENABLE_UPSCALING")
    if (PLATFORM_SWITCH AND USE_DEKO3D_RENDERER)
        set(SUPPORT_UPSCALING ON)
    elseif (PLATFORM_APPLE AND USE_METAL_RENDERER)
        set(SUPPORT_UPSCALING ON)
    else ()
        message(FATAL_ERROR "ENABLE_UPSCALING=ON is only supported with the Switch deko3d renderer or Apple Metal renderer")
    endif ()
endif ()

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${EXTERN_PATH}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${EXTERN_PATH}/cmake")
#find_package(PkgConfig REQUIRED)

# project info
project(Moonlight)
if (PLATFORM_IOS OR PLATFORM_TVOS OR PLATFORM_VISIONOS)
    moonlight_prepare_apple_mobile_vcpkg()
endif ()

set(VERSION_MAJOR "1")
set(VERSION_MINOR "5")
set(VERSION_ALTER "0")
set(VERSION_BUILD "1")
set(PACKAGE_NAME "ru.xitrix.Moonlight")
set(PSN_TITLE_ID  "MNTL00000")
set(PSN_VERSION  "01.00")
set(PROJECT_AUTHOR "XITRIX")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/moonlight_icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

function(moonlight_define_multi_sdk_mbedtls_targets)
    if (TARGET MbedTLS::mbedcrypto)
        return()
    endif ()

    set(_moonlight_mbedtls_include_dir "${MOONLIGHT_APPLE_MOBILE_VCPKG_INCLUDE_DIR}")

    add_library(MbedTLS::mbedcrypto INTERFACE IMPORTED GLOBAL)
    set_target_properties(MbedTLS::mbedcrypto PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_moonlight_mbedtls_include_dir}"
        INTERFACE_LINK_LIBRARIES "mbedcrypto")

    add_library(MbedTLS::mbedx509 INTERFACE IMPORTED GLOBAL)
    set_target_properties(MbedTLS::mbedx509 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_moonlight_mbedtls_include_dir}"
        INTERFACE_LINK_LIBRARIES "mbedx509;MbedTLS::mbedcrypto")

    add_library(MbedTLS::mbedtls INTERFACE IMPORTED GLOBAL)
    set_target_properties(MbedTLS::mbedtls PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_moonlight_mbedtls_include_dir}"
        INTERFACE_LINK_LIBRARIES "mbedtls;MbedTLS::mbedx509;MbedTLS::mbedcrypto")

    if (NOT TARGET mbed::crypto)
        add_library(mbed::crypto INTERFACE IMPORTED GLOBAL)
        set_target_properties(mbed::crypto PROPERTIES
            INTERFACE_LINK_LIBRARIES "MbedTLS::mbedcrypto")
    endif ()

    if (NOT TARGET mbed::x509)
        add_library(mbed::x509 INTERFACE IMPORTED GLOBAL)
        set_target_properties(mbed::x509 PROPERTIES
            INTERFACE_LINK_LIBRARIES "MbedTLS::mbedx509")
    endif ()

    if (NOT TARGET mbed::tls)
        add_library(mbed::tls INTERFACE IMPORTED GLOBAL)
        set_target_properties(mbed::tls PROPERTIES
            INTERFACE_LINK_LIBRARIES "MbedTLS::mbedtls")
    endif ()
endfunction()

if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_moonlight_vcpkg_triplet_dir "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    if (EXISTS "${_moonlight_vcpkg_triplet_dir}")
        list(PREPEND CMAKE_PREFIX_PATH "${_moonlight_vcpkg_triplet_dir}")
    endif ()
endif ()

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

if (PLATFORM_PSV)
    set(MULTICAST_DISABLED ON)
endif ()

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()

if (USE_MBEDTLS_CRYPTO)
    if (MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
        set(MBEDTLS_INCLUDE_DIRS "${MOONLIGHT_APPLE_MOBILE_VCPKG_INCLUDE_DIR}")
        moonlight_define_multi_sdk_mbedtls_targets()
    else ()
        find_package(MbedTLS REQUIRED COMPONENTS crypto tls x509)
    endif ()
    add_compile_definitions(USE_MBEDTLS_CRYPTO)
elseif (USE_OPENSSL_CRYPTO)
    find_package(OpenSSL REQUIRED)
    add_definitions(-DUSE_OPENSSL_CRYPTO)
endif ()

unset(_moonlight_vcpkg_triplet_dir)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(extern)

# setting src and include
#file(GLOB_RECURSE MAIN_SRC app/src/*.cpp )
file(GLOB MAIN_SRC CONFIGURE_DEPENDS
    app/src/*.cpp
    app/src/crypto/*.cpp
    app/src/gestures/*.cpp
    app/src/keyboards/*.cpp
    app/src/libgamestream/*.cpp
    app/src/streaming/*.cpp
    app/src/streaming/audio/*.cpp
    app/src/streaming/ffmpeg/*.cpp
    app/src/streaming/switch/*.cpp
    app/src/views/*.cpp
    app/src/utils/*.cpp)

if (MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
    list(APPEND APP_PLATFORM_INCLUDE "${MOONLIGHT_APPLE_MOBILE_VCPKG_INCLUDE_DIR}")
endif ()

if (PLATFORM_SWITCH)
    list(REMOVE_ITEM MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/audio/SDLAudioRenderer.cpp)
endif ()

set(PLATFORM_OPTION)
if (PLATFORM_DESKTOP)
    if (WIN32)
        configure_file("${CMAKE_SOURCE_DIR}/app/platforms/windows/windows.manifest.in" "${CMAKE_BINARY_DIR}/app.manifest")
        configure_file("${CMAKE_SOURCE_DIR}/app/platforms/windows/resource.rc.in" "${CMAKE_BINARY_DIR}/resource.rc")
        set_source_files_properties(${CMAKE_BINARY_DIR}/resource.rc PROPERTIES
            OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/app.manifest")
        list(APPEND MAIN_SRC ${CMAKE_BINARY_DIR}/resource.rc)
    elseif (BUNDLE_MACOS_APP)
        # list(APPEND MAIN_SRC ${CMAKE_SOURCE_DIR}/app/borealis.icns)
    endif ()
elseif (PLATFORM_SWITCH)
    set(PLATFORM_LIBS
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
    list(APPEND MAIN_SRC app/src/streaming/video/Android/AndroidMediaCodecVideoRenderer.cpp)
elseif (PLATFORM_IOS OR PLATFORM_TVOS OR PLATFORM_VISIONOS)
    list(APPEND MAIN_SRC app/platforms/ios/apple_mdns.mm)
    list(APPEND MAIN_SRC app/platforms/ios/apple_display.mm)
endif ()

if (USE_METAL_RENDERER)
    list(APPEND MAIN_SRC app/src/streaming/video/Metal/MetalVideoRenderer.mm)
    list(APPEND MAIN_SRC app/src/streaming/video/Metal/streamutils.cpp)
    if (SUPPORT_UPSCALING AND PLATFORM_APPLE)
        list(APPEND MAIN_SRC app/src/streaming/video/Metal/AppleMetalFxUpscalingSupport.mm)
    endif ()
elseif (USE_D3D11_RENDERER)
    list(APPEND MAIN_SRC app/src/streaming/video/D3D11/D3D11VideoRenderer.cpp)
elseif (USE_DEKO3D_RENDERER)
    list(APPEND MAIN_SRC app/src/streaming/video/deko3d/DKVideoRenderer.cpp)
elseif (USE_GL_RENDERER)
    list(APPEND MAIN_SRC app/src/streaming/video/OpenGL/GLVideoRenderer.cpp)
endif ()

# building target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
if (MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_ATTRIBUTE_LIBRARY_SEARCH_PATHS[sdk=${MOONLIGHT_APPLE_MOBILE_XCODE_DEVICE_SDK}*] "$(inherited) ${MOONLIGHT_APPLE_MOBILE_VCPKG_STAGING_ROOT}/${MOONLIGHT_APPLE_MOBILE_XCODE_DEVICE_SDK}/$(CONFIGURATION)"
        XCODE_ATTRIBUTE_LIBRARY_SEARCH_PATHS[sdk=${MOONLIGHT_APPLE_MOBILE_XCODE_SIMULATOR_SDK}*] "$(inherited) ${MOONLIGHT_APPLE_MOBILE_VCPKG_STAGING_ROOT}/${MOONLIGHT_APPLE_MOBILE_XCODE_SIMULATOR_SDK}/$(CONFIGURATION)")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CURL_STATICLIB)
endif ()


# building release file
if (PLATFORM_DESKTOP)
    if (BUNDLE_MACOS_APP)
        set(IOS_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/mac/Images.xcassets)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
            # MACOSX_BUNDLE_ICON_FILE "borealis.icns"
            MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 ${PROJECT_AUTHOR}"
            # RESOURCE "${CMAKE_SOURCE_DIR}/app/borealis.icns"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${PACKAGE_NAME}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION_BUILD}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games" # Not work
            XCODE_ATTRIBUTE_INSTALL_PATH $(LOCAL_APPS_DIR)
            XCODE_EMBED_RESOURCES_CODE_SIGN_ON_COPY ON # Not work
            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON # Not work
        )

        set_property(
            SOURCE ${IOS_ASSETS}
            PROPERTY MACOSX_PACKAGE_LOCATION "Resources"
        )

        target_sources(${PROJECT_NAME} PRIVATE
            ${IOS_ASSETS}
        )

        add_custom_target(${PROJECT_NAME}.data
                COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${PROJECT_NAME}.app/Contents/Resources/resources
        )
    else ()
        add_custom_target(${PROJECT_NAME}.data
                COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
        )
    endif ()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../Frameworks"
    )

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
            FILE ${CMAKE_SOURCE_DIR}/app/platforms/psv/sce_sys sce_sys
            FILE ${CMAKE_SOURCE_DIR}/app/platforms/psv/module/ module
        )
    else()
        vita_create_vpk(${PROJECT_NAME}.vpk ${PSN_TITLE_ID} ${PROJECT_NAME}.self
            VERSION ${PSN_VERSION}
            NAME ${PROJECT_NAME}
            FILE ${CMAKE_SOURCE_DIR}/resources resources
            FILE ${CMAKE_SOURCE_DIR}/app/platforms/psv/sce_sys sce_sys
            FILE ${CMAKE_SOURCE_DIR}/app/platforms/psv/module/ module
        )
    endif()
elseif (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        gen_dksh("${PROJECT_RESOURCES}/shaders")
    endif ()
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
    list(APPEND APP_PLATFORM_LIB dav1d z)
elseif (PLATFORM_ANDROID)
    list(APPEND APP_PLATFORM_LIB android)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
    # ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/android/arm64-v8a/libEGL_angle.so
    # ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/android/arm64-v8a/libGLESv2_angle.so
    )
elseif (PLATFORM_IOS)
    ios_bundle(
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iphoneos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/Images.xcassets"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iOSBundleInfo.plist.in"
            ${PROJECT_NAME}
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            "${VERSION_BUILD}")

    set_target_properties(${PROJECT_NAME} PROPERTIES 
        INSTALL_RPATH @executable_path/Frameworks
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 ${PROJECT_AUTHOR}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${VERSION_BUILD}"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsControllerUserInteraction YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
        XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/ios/"
        XCODE_ATTRIBUTE_INSTALL_PATH $(LOCAL_APPS_DIR))
elseif (PLATFORM_VISIONOS)
    ios_bundle(
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iphoneos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/Images.xcassets"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iOSBundleInfo.plist.in"
            ${PROJECT_NAME}
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            "${VERSION_BUILD}")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH @executable_path/Frameworks
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 ${PROJECT_AUTHOR}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${VERSION_BUILD}"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsControllerUserInteraction YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
        XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/vision/"
        XCODE_ATTRIBUTE_INSTALL_PATH $(LOCAL_APPS_DIR))
elseif (PLATFORM_TVOS)
    ios_bundle(
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/tvos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/Images.xcassets"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iOSBundleInfo.plist.in"
            ${PROJECT_NAME}
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            "${VERSION_BUILD}")
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        INSTALL_RPATH @executable_path/Frameworks
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 ${PROJECT_AUTHOR}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${VERSION_BUILD}"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsControllerUserInteraction YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
        XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/tvos/"
        XCODE_ATTRIBUTE_INSTALL_PATH $(LOCAL_APPS_DIR)
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIconTvOS")
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
        app/src/streaming/video/D3D11
        app/src/streaming/video/deko3d
        app/src/streaming/video/OpenGL
        app/src/streaming/video/Metal
        app/src/utils
        extern/CImg
        ${MBEDTLS_INCLUDE_DIRS}
        ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})

if(PLATFORM_SWITCH)
    target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/include
    )
endif ()

set(MOONLIGHT_APP_SDL_LIBS)
if (USE_SDL2 AND NOT PLATFORM_ANDROID)
    if (TARGET SDL2::SDL2)
        list(APPEND MOONLIGHT_APP_SDL_LIBS SDL2::SDL2)
    elseif (TARGET SDL2::SDL2-static)
        list(APPEND MOONLIGHT_APP_SDL_LIBS SDL2::SDL2-static)
    else ()
        find_package(SDL2 REQUIRED)
        if (TARGET SDL2::SDL2)
            list(APPEND MOONLIGHT_APP_SDL_LIBS SDL2::SDL2)
        elseif (TARGET SDL2::SDL2-static)
            list(APPEND MOONLIGHT_APP_SDL_LIBS SDL2::SDL2-static)
        elseif (SDL2_LIBRARIES)
            list(APPEND MOONLIGHT_APP_SDL_LIBS ${SDL2_LIBRARIES})
        else ()
            message(FATAL_ERROR "SDL2 requested but no SDL2 target was found")
        endif ()
    endif ()
endif ()

set(MOONLIGHT_APP_FFMPEG_LIBS)
set(MOONLIGHT_APP_MISC_LIBS)
if (MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
    list(APPEND MOONLIGHT_APP_FFMPEG_LIBS avcodec avformat avutil swresample)
    list(APPEND MOONLIGHT_APP_MISC_LIBS jansson expat zstd opus png16 z)
else ()
        find_package(CURL REQUIRED)
    find_package(Jansson REQUIRED)
    find_package(EXPAT REQUIRED)
    find_package(PNG REQUIRED)

    find_library(OPUS_LIBRARY opus)
    message("opus: ${OPUS_LIBRARY}")

    if(PLATFORM_SWITCH)
        list(APPEND MOONLIGHT_APP_FFMPEG_LIBS
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libavcodec.a
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libavformat.a
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libavutil.a
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libswresample.a)
    else()
        find_library(AVCODEC_LIBRARY avcodec)
        message("avcodec: ${AVCODEC_LIBRARY}")

        find_library(AVFORMAT_LIBRARY avformat)
        message("avformat: ${AVFORMAT_LIBRARY}")

        find_library(AVUTIL_LIBRARY avutil)
        message("avutil: ${AVUTIL_LIBRARY}")

        find_library(SWRESAMPLE_LIBRARY swresample)
        message("swresample: ${SWRESAMPLE_LIBRARY}")
        list(APPEND MOONLIGHT_APP_FFMPEG_LIBS
            ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${AVFORMAT_LIBRARY}
            ${SWRESAMPLE_LIBRARY})
    endif ()

    find_library(ZSTD_LIBRARY zstd)
    message("zstd: ${ZSTD_LIBRARY}")
    list(APPEND MOONLIGHT_APP_MISC_LIBS
        Jansson::Jansson
        EXPAT::EXPAT
        ${ZSTD_LIBRARY}
        ${OPUS_LIBRARY}
        PNG::PNG)
endif ()

# Recompiled CURL with Mbedtls support for Switch
if(PLATFORM_SWITCH)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libcurl.a)
elseif (MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
    target_link_libraries(${PROJECT_NAME} PRIVATE curl)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE
        borealis
        moonlight-common-c
        ${MOONLIGHT_APP_MISC_LIBS}
        ${MOONLIGHT_APP_FFMPEG_LIBS}
        ${MOONLIGHT_APP_SDL_LIBS}
        ${APP_PLATFORM_LIB})

if (MULTICAST_DISABLED)
    add_definitions(-DMULTICAST_DISABLED)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE mdns)
endif ()

if (USE_MBEDTLS_CRYPTO)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            MbedTLS::mbedtls
            MbedTLS::mbedx509
            MbedTLS::mbedcrypto)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE
            OpenSSL::SSL
            OpenSSL::Crypto)
endif ()

if (USE_GL_RENDERER)
    if (PLATFORM_MACOS)
        set(SUPPORT_HDR ON)
    endif ()
    add_definitions(-DUSE_GL_RENDERER)
elseif (USE_D3D11_RENDERER)
    add_definitions(-DUSE_D3D11_RENDERER)
elseif (USE_METAL_RENDERER)
    set(SUPPORT_HDR ON)
    add_definitions(-DUSE_METAL_RENDERER)
elseif (USE_DEKO3D_RENDERER)
    add_definitions(-DUSE_DEKO3D_RENDERER)
endif ()

if (USE_D3D11_RENDERER)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3dcompiler)
endif ()

if (SUPPORT_HDR)
    add_definitions(-DSUPPORT_HDR)
endif ()

if (SUPPORT_UPSCALING)
    add_definitions(-DSUPPORT_UPSCALING)
endif ()

if (PLATFORM_IOS OR PLATFORM_TVOS OR PLATFORM_VISIONOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_EMBED_FRAMEWORKS "${IOS_FRAMEWORKS}")
endif()

if (APPLE)
    set(XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC OFF)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreMedia" "-framework VideoToolbox" "-framework AVKit" "-framework MetalKit")
    if (SUPPORT_UPSCALING AND USE_METAL_RENDERER)
        target_link_libraries(${PROJECT_NAME} PRIVATE "-weak_framework MetalFX")
    endif ()
    if (MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework Security"
            "-framework CoreFoundation"
            "-framework CoreServices"
            m)
    endif ()
elseif (PLATFORM_PSV)
    target_link_libraries(${PROJECT_NAME} PRIVATE mp3lame libGLESv2_stub)
endif ()

if (WIN32 AND PLATFORM_DESKTOP AND MOONLIGHT_DESKTOP_USE_SYSTEM_PACKAGES AND
    NOT MOONLIGHT_DESKTOP_SYSTEM_PREFIX STREQUAL "")
    set(_moonlight_runtime_dependency_dir "${MOONLIGHT_DESKTOP_SYSTEM_PREFIX}/bin")
    add_custom_target(${PROJECT_NAME}.runtime ALL
        COMMAND "${CMAKE_COMMAND}"
            -DMOONLIGHT_RUNTIME_EXECUTABLE=$<TARGET_FILE:${PROJECT_NAME}>
            -DMOONLIGHT_RUNTIME_SEARCH_DIR=${_moonlight_runtime_dependency_dir}
            -DMOONLIGHT_OBJDUMP_COMMAND=${CMAKE_OBJDUMP}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/extern/cmake/stage_runtime_deps.cmake"
        DEPENDS ${PROJECT_NAME}
        VERBATIM)
endif ()
