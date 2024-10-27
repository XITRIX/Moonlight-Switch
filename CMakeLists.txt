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
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
endif ()

if (PLATFORM_IOS OR PLATFORM_TVOS)
    # set(ENABLE_ARC OFF)
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
set(VERSION_ALTER "2")
set(VERSION_BUILD "1")
set(PACKAGE_NAME "ru.xitrix.Moonlight")
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

if (PLATFORM_ANDROID OR PLATFORM_PSV)
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
        # list(APPEND MAIN_SRC ${CMAKE_SOURCE_DIR}/app/borealis.icns)
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
elseif (PLATFORM_IOS OR PLATFORM_TVOS)
    list(APPEND MAIN_SRC app/platforms/ios/apple_mdns.mm)
    list(APPEND MAIN_SRC app/platforms/ios/apple_display.mm)
    list(APPEND MAIN_SRC app/src/streaming/video/Metal/MetalVideoRenderer.mm)
endif ()


# building target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)


# building release file
if (PLATFORM_DESKTOP)
    if (BUNDLE_MACOS_APP)
        set(IOS_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/mac/Images.xcassets)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
            # MACOSX_BUNDLE_ICON_FILE "borealis.icns"
            MACOSX_BUNDLE_COPYRIGHT "Copyright 2023 ${PROJECT_AUTHOR}"
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
        add_custom_target(${PRxOJECT_NAME}.data
                COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
        )
    endif ()

    list(APPEND google_angle_libs 
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/mac/libEGL.dylib
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/mac/libGLESv2.dylib
    )

    set_property(
        SOURCE ${google_angle_libs}
        PROPERTY MACOSX_PACKAGE_LOCATION "Frameworks"
    )

    target_sources(${PROJECT_NAME} PRIVATE
        ${google_angle_libs}
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../Frameworks"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ${google_angle_libs}
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

    list(APPEND IOS_FRAMEWORKS ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/ios/MetalANGLE.framework)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        INSTALL_RPATH @executable_path/Frameworks
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsControllerUserInteraction YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCRequiresControllerUserInteraction YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
        XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/ios/"
        XCODE_ATTRIBUTE_INSTALL_PATH $(LOCAL_APPS_DIR))
elseif (PLATFORM_TVOS)
    ios_bundle(
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/tvos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/Images.xcassets"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/platforms/ios/iOSBundleInfo.plist.in"
            ${PROJECT_NAME}
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            "${VERSION_BUILD}")
    list(APPEND IOS_FRAMEWORKS ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/angle/tvos/MetalANGLE.framework)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        INSTALL_RPATH @executable_path/Frameworks
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsGameMode YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCSupportsControllerUserInteraction YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_GCRequiresControllerUserInteraction YES
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
        app/src/streaming/video/deko3d
        app/src/streaming/video/OpenGL
        app/src/streaming/video/Metal
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

if(PLATFORM_SWITCH)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libavcodec.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libavformat.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libavutil.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/libswresample.a
    )

    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/switch/include
    )
else()
    find_library(AVCODEC_LIBRARY avcodec)
    message("avcodec: ${AVCODEC_LIBRARY}")
    
    find_library(AVFORMAT_LIBRARY avformat)
    message("avformat: ${AVFORMAT_LIBRARY}")

    find_library(AVUTIL_LIBRARY avutil)
    message("avutil: ${AVUTIL_LIBRARY}")

    find_library(SWRESAMPLE_LIBRARY swresample)
    message("swresample: ${SWRESAMPLE_LIBRARY}")
endif ()

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

if (PLATFORM_IOS OR PLATFORM_TVOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_EMBED_FRAMEWORKS "${IOS_FRAMEWORKS}")
endif()

if (APPLE)
    set(XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC OFF)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreMedia" "-framework VideoToolbox" "-framework AVKit")
elseif (PLATFORM_PSV)
    target_link_libraries(${PROJECT_NAME} PRIVATE mp3lame libGLESv2_stub)
endif ()