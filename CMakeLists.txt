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

# project info
project(borealis_demo)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "13")
set(VERSION_ALTER "3")
set(VERSION_BUILD "0")
set(PACKAGE_NAME "com.borealis.demo")
set(VITA_TITLEID  "BOREALIS0")
set(VITA_VERSION  "01.00")
set(PROJECT_AUTHOR "borealis")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/moonlight_icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

add_definitions(
        -DAPP_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
        -DUSE_MBEDTLS_CRYPTO
        -DMULTICAST_DISABLED
        -Dcimg_display=0
        -Dcimg_use_png
        -Dcimg_no_system_calls
        -DBRLS_I18N_PREFIX=\"main/\")

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()

add_subdirectory(extern)

# setting src and include
file(GLOB_RECURSE MAIN_SRC app/src/*.cpp)

# Fmt
list(APPEND MAIN_SRC
        ${BOREALIS_LIBRARY}/lib/extern/fmt/src/format.cc
        ${BOREALIS_LIBRARY}/lib/extern/fmt/src/os.cc)

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
        glfw3 EGL glapi drm_nouveau
        # base lib
        nx m
    )
    list(APPEND MAIN_SRC
            ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c
            app/src/switch/wrapper.c
            )
elseif (PLATFORM_PSV)
    add_definitions(-D__psp2__ -D__PSV__)
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
        vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
            VERSION ${VITA_VERSION}
            NAME ${PROJECT_NAME}
            FILE ${CMAKE_SOURCE_DIR}/psv/sce_sys sce_sys
            FILE ${CMAKE_SOURCE_DIR}/psv/module/ module
        )
    else()
        vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
            VERSION ${VITA_VERSION}
            NAME ${PROJECT_NAME}
            FILE ${CMAKE_SOURCE_DIR}/resources resources
            FILE ${CMAKE_SOURCE_DIR}/psv/sce_sys sce_sys
            FILE ${CMAKE_SOURCE_DIR}/psv/module/ module
        )
    endif()
elseif (PLATFORM_SWITCH)
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
    list(APPEND APP_PLATFORM_LIB swresample dav1d z)
elseif (PLATFORM_IOS)
    ios_bundle(
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/tvos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/iphoneos/Splash.storyboard"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/Images.xcassets"
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/iOSBundleInfo.plist.in"
            "borealis"
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
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
        extern/zeroconf
        ${MBEDTLS_INCLUDE_DIRS}
        ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${EXTERN_PATH}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${EXTERN_PATH}/cmake")
#find_package(PkgConfig REQUIRED)

#pkg_check_modules(jansson REQUIRED IMPORTED_TARGET jansson)
#add_library(libjansson ALIAS PkgConfig::jansson)

#pkg_check_modules(opus REQUIRED IMPORTED_TARGET opus)
#add_library(libopus ALIAS PkgConfig::opus)

#pkg_check_modules(expat REQUIRED IMPORTED_TARGET expat)
#add_library(libexpat ALIAS PkgConfig::expat)

find_package(PNG REQUIRED)
find_package(CURL REQUIRED)
find_package(mbedTLS REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Opus REQUIRED)
find_package(Jansson REQUIRED)
find_package(EXPAT REQUIRED)
#find_package(FFMPEG REQUIRED)

find_library(AVCODEC_LIBRARY avcodec)
message("avcodec: ${AVCODEC_LIBRARY}")

find_library(AVFORMAT_LIBRARY avformat)
message("avformat: ${AVFORMAT_LIBRARY}")

find_library(AVUTIL_LIBRARY avutil)
message("avutil: ${AVUTIL_LIBRARY}")

find_library(SWRESAMPLE_LIBRARY swresample)
message("swresample: ${SWRESAMPLE_LIBRARY}")

target_link_libraries(${PROJECT_NAME} PRIVATE
        borealis
        moonlight-common-c
        Jansson::Jansson
        EXPAT::EXPAT
        CURL::libcurl
        mbed::tls
        mbed::x509
        mbed::crypto
        ${AVCODEC_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${AVFORMAT_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
        opus
        SDL2::SDL2
        PNG::PNG
        ${APP_PLATFORM_LIB})