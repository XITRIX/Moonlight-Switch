message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

function(check_libromfs_generator)
    if (NOT DEFINED LIBROMFS_PREBUILT_GENERATOR OR NOT EXISTS "${LIBROMFS_PREBUILT_GENERATOR}")
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libromfs-generator")
            set(LIBROMFS_PREBUILT_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/libromfs-generator" PARENT_SCOPE)
        else ()
            message(FATAL_ERROR "libromfs-generator has not been built, please refer to borealis/build_libromfs_generator.sh for more information")
        endif ()
    endif()
endfunction()

function(_moonlight_normalize_apple_mobile_sdk_variant)
    set(_moonlight_allowed_variants DEVICE SIMULATOR BOTH)
    if (NOT DEFINED APPLE_MOBILE_SDK_VARIANT OR APPLE_MOBILE_SDK_VARIANT STREQUAL "")
        set(APPLE_MOBILE_SDK_VARIANT "BOTH" CACHE STRING "Apple mobile SDK selection for iOS/tvOS/visionOS builds: DEVICE, SIMULATOR, or BOTH." FORCE)
    endif ()

    string(TOUPPER "${APPLE_MOBILE_SDK_VARIANT}" _moonlight_sdk_variant)
    if (NOT _moonlight_sdk_variant IN_LIST _moonlight_allowed_variants)
        message(FATAL_ERROR "APPLE_MOBILE_SDK_VARIANT must be one of DEVICE, SIMULATOR, or BOTH. Current value: ${APPLE_MOBILE_SDK_VARIANT}")
    endif ()

    set(APPLE_MOBILE_SDK_VARIANT "${_moonlight_sdk_variant}" CACHE STRING "Apple mobile SDK selection for iOS/tvOS/visionOS builds: DEVICE, SIMULATOR, or BOTH." FORCE)
    set_property(CACHE APPLE_MOBILE_SDK_VARIANT PROPERTY STRINGS DEVICE SIMULATOR BOTH)
endfunction()

function(_moonlight_get_default_simulator_arch _out_var)
    execute_process(
        COMMAND /usr/sbin/sysctl -in hw.optional.arm64
        OUTPUT_VARIABLE _moonlight_arm64_capable
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (_moonlight_arm64_capable STREQUAL "1")
        set(${_out_var} "arm64" PARENT_SCOPE)
    elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(${_out_var} "arm64" PARENT_SCOPE)
    else ()
        set(${_out_var} "x64" PARENT_SCOPE)
    endif ()
endfunction()

function(_moonlight_sync_path _source _destination)
    if (NOT EXISTS "${_source}")
        message(FATAL_ERROR "Cannot stage missing dependency: ${_source}")
    endif ()

    get_filename_component(_moonlight_destination_dir "${_destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${_moonlight_destination_dir}")

    if (EXISTS "${_destination}" OR IS_SYMLINK "${_destination}")
        file(REMOVE "${_destination}")
    endif ()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_source}" "${_destination}"
        RESULT_VARIABLE _moonlight_link_result
        ERROR_VARIABLE _moonlight_link_error)
    if (NOT _moonlight_link_result EQUAL 0)
        if (IS_DIRECTORY "${_source}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_source}" "${_destination}"
                RESULT_VARIABLE _moonlight_copy_result
                ERROR_VARIABLE _moonlight_copy_error)
        else ()
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_source}" "${_destination}"
                RESULT_VARIABLE _moonlight_copy_result
                ERROR_VARIABLE _moonlight_copy_error)
        endif ()
        if (NOT _moonlight_copy_result EQUAL 0)
            message(FATAL_ERROR "Failed to stage ${_source} at ${_destination}: ${_moonlight_copy_error}")
        endif ()
    endif ()
endfunction()

function(_moonlight_collect_vcpkg_install_args _out_var)
    set(_moonlight_install_root_override)
    if (ARGC GREATER 1)
        set(_moonlight_install_root_override "${ARGV1}")
    endif ()
    set(_moonlight_args)
    if (DEFINED VCPKG_MANIFEST_DIR AND NOT VCPKG_MANIFEST_DIR STREQUAL "")
        list(APPEND _moonlight_args "--x-manifest-root=${VCPKG_MANIFEST_DIR}")
    endif ()
    if (DEFINED _moonlight_install_root_override AND NOT _moonlight_install_root_override STREQUAL "")
        list(APPEND _moonlight_args "--x-install-root=${_moonlight_install_root_override}")
    elseif (DEFINED VCPKG_INSTALLED_DIR AND NOT VCPKG_INSTALLED_DIR STREQUAL "")
        list(APPEND _moonlight_args "--x-install-root=${VCPKG_INSTALLED_DIR}")
    endif ()
    if (DEFINED VCPKG_OVERLAY_PORTS)
        foreach(_moonlight_overlay_port IN LISTS VCPKG_OVERLAY_PORTS)
            list(APPEND _moonlight_args "--overlay-ports=${_moonlight_overlay_port}")
        endforeach ()
    endif ()
    if (DEFINED VCPKG_OVERLAY_TRIPLETS)
        foreach(_moonlight_overlay_triplet IN LISTS VCPKG_OVERLAY_TRIPLETS)
            list(APPEND _moonlight_args "--overlay-triplets=${_moonlight_overlay_triplet}")
        endforeach ()
    endif ()
    if (DEFINED VCPKG_INSTALL_OPTIONS)
        foreach(_moonlight_install_option IN LISTS VCPKG_INSTALL_OPTIONS)
            list(APPEND _moonlight_args "${_moonlight_install_option}")
        endforeach ()
    endif ()
    set(${_out_var} "${_moonlight_args}" PARENT_SCOPE)
endfunction()

function(_moonlight_install_vcpkg_triplet _triplet _install_root)
    if (NOT DEFINED Z_VCPKG_EXECUTABLE OR NOT EXISTS "${Z_VCPKG_EXECUTABLE}")
        message(FATAL_ERROR "vcpkg executable is unavailable, cannot install companion triplet ${_triplet}")
    endif ()
    if (NOT DEFINED Z_VCPKG_ROOT_DIR OR Z_VCPKG_ROOT_DIR STREQUAL "")
        message(FATAL_ERROR "vcpkg root is unavailable, cannot install companion triplet ${_triplet}")
    endif ()

    _moonlight_collect_vcpkg_install_args(_moonlight_install_args "${_install_root}")
    message(STATUS "Ensuring vcpkg dependencies are installed for ${_triplet}")
    execute_process(
        COMMAND "${Z_VCPKG_EXECUTABLE}" install
            --triplet "${_triplet}"
            --vcpkg-root "${Z_VCPKG_ROOT_DIR}"
            ${_moonlight_install_args}
        RESULT_VARIABLE _moonlight_install_result
        OUTPUT_VARIABLE _moonlight_install_log
        ERROR_VARIABLE _moonlight_install_log)

    if (NOT _moonlight_install_result EQUAL 0)
        set(_moonlight_install_log_file "${CMAKE_BINARY_DIR}/vcpkg-manifest-install-${_triplet}.log")
        file(WRITE "${_moonlight_install_log_file}" "${_moonlight_install_log}")
        message(FATAL_ERROR "vcpkg install failed for ${_triplet}. See ${_moonlight_install_log_file}")
    endif ()
endfunction()

function(moonlight_prepare_apple_mobile_vcpkg)
    if (NOT MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK)
        return()
    endif ()

    # Manifest mode treats one install root as one desired dependency state. If we
    # install the simulator triplet into the primary root, vcpkg removes the device
    # triplet packages. Keep a hidden companion root, then surface the simulator
    # triplet back under the main vcpkg_installed tree for convenience.
    set(_moonlight_simulator_install_root "${VCPKG_INSTALLED_DIR}/.moonlight-companion")
    _moonlight_install_vcpkg_triplet("${MOONLIGHT_APPLE_MOBILE_VCPKG_SIMULATOR_TRIPLET}" "${_moonlight_simulator_install_root}")
    _moonlight_sync_path(
        "${_moonlight_simulator_install_root}/${MOONLIGHT_APPLE_MOBILE_VCPKG_SIMULATOR_TRIPLET}"
        "${VCPKG_INSTALLED_DIR}/${MOONLIGHT_APPLE_MOBILE_VCPKG_SIMULATOR_TRIPLET}")

    set(_moonlight_stage_specs
        "curl|libcurl.a|libcurl-d.a"
        "jansson|libjansson.a|libjansson.a"
        "expat|libexpat.a|libexpat.a"
        "png16|libpng16.a|libpng16d.a"
        "avcodec|libavcodec.a|libavcodec.a"
        "avformat|libavformat.a|libavformat.a"
        "avutil|libavutil.a|libavutil.a"
        "swresample|libswresample.a|libswresample.a"
        "zstd|libzstd.a|libzstd.a"
        "opus|libopus.a|libopus.a"
        "z|libz.a|libz.a"
        "mbedtls|libmbedtls.a|libmbedtls.a"
        "mbedx509|libmbedx509.a|libmbedx509.a"
        "mbedcrypto|libmbedcrypto.a|libmbedcrypto.a")

    set(_moonlight_stage_root "${CMAKE_BINARY_DIR}/apple-mobile-vcpkg")
    foreach(_moonlight_sdk_kind DEVICE SIMULATOR)
        if (_moonlight_sdk_kind STREQUAL "DEVICE")
            set(_moonlight_triplet "${MOONLIGHT_APPLE_MOBILE_VCPKG_DEVICE_TRIPLET}")
            set(_moonlight_sdk_name "${MOONLIGHT_APPLE_MOBILE_XCODE_DEVICE_SDK}")
            set(_moonlight_triplet_root "${VCPKG_INSTALLED_DIR}/${_moonlight_triplet}")
        else ()
            set(_moonlight_triplet "${MOONLIGHT_APPLE_MOBILE_VCPKG_SIMULATOR_TRIPLET}")
            set(_moonlight_sdk_name "${MOONLIGHT_APPLE_MOBILE_XCODE_SIMULATOR_SDK}")
            set(_moonlight_triplet_root "${_moonlight_simulator_install_root}/${_moonlight_triplet}")
        endif ()

        foreach(_moonlight_config Debug Release)
            set(_moonlight_stage_dir "${_moonlight_stage_root}/${_moonlight_sdk_name}/${_moonlight_config}")
            if (_moonlight_config STREQUAL "Debug")
                set(_moonlight_config_dir "${_moonlight_triplet_root}/debug/lib")
            else ()
                set(_moonlight_config_dir "${_moonlight_triplet_root}/lib")
            endif ()

            foreach(_moonlight_stage_spec IN LISTS _moonlight_stage_specs)
                string(REPLACE "|" ";" _moonlight_stage_spec_parts "${_moonlight_stage_spec}")
                list(GET _moonlight_stage_spec_parts 0 _moonlight_alias_name)
                list(GET _moonlight_stage_spec_parts 1 _moonlight_release_name)
                list(GET _moonlight_stage_spec_parts 2 _moonlight_debug_name)

                if (_moonlight_config STREQUAL "Debug")
                    set(_moonlight_source_file "${_moonlight_config_dir}/${_moonlight_debug_name}")
                    if (NOT EXISTS "${_moonlight_source_file}")
                        set(_moonlight_source_file "${_moonlight_triplet_root}/lib/${_moonlight_release_name}")
                    endif ()
                else ()
                    set(_moonlight_source_file "${_moonlight_config_dir}/${_moonlight_release_name}")
                endif ()

                set(_moonlight_staged_file "${_moonlight_stage_dir}/lib${_moonlight_alias_name}.a")
                _moonlight_sync_path("${_moonlight_source_file}" "${_moonlight_staged_file}")
            endforeach ()
        endforeach ()
    endforeach ()

    set(MOONLIGHT_APPLE_MOBILE_VCPKG_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/${MOONLIGHT_APPLE_MOBILE_VCPKG_PRIMARY_TRIPLET}/include" PARENT_SCOPE)
    set(MOONLIGHT_APPLE_MOBILE_VCPKG_STAGING_ROOT "${_moonlight_stage_root}" PARENT_SCOPE)
    set(MOONLIGHT_APPLE_MOBILE_VCPKG_LIBRARIES
        curl
        jansson
        expat
        png16
        avcodec
        avutil
        avformat
        swresample
        zstd
        opus
        z
        mbedtls
        mbedx509
        mbedcrypto
        PARENT_SCOPE)
    message(STATUS "Prepared Apple mobile multi-SDK dependency staging at ${_moonlight_stage_root}")
endfunction()

# For unknown reason PLATFORM_SWITCH reports APPLE flag as TRUE
if (APPLE AND NOT PLATFORM_SWITCH)
    set(PLATFORM_APPLE ON)
    add_definitions(-DPLATFORM_APPLE)
endif ()

if (PLATFORM_DESKTOP)
    add_definitions(-DPLATFORM_DESKTOP)
    if (APPLE)
        set(PLATFORM_MACOS ON)
        add_definitions(-DPLATFORM_MACOS)
    endif ()
    message(STATUS "building for Desktop")
    set(CMAKE_TOOLCHAIN_FILE ${EXTERN_PATH}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE PATH "vcpkg toolchain file")
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g2 -ggdb -Wall")
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
    set(USE_SDL2 ON)
    set(USE_GLES3 ON)
    set(USE_LIBROMFS ON)
    check_libromfs_generator()
elseif (PLATFORM_IOS OR PLATFORM_TVOS OR PLATFORM_VISIONOS)
    _moonlight_normalize_apple_mobile_sdk_variant()
    _moonlight_get_default_simulator_arch(_moonlight_simulator_arch)

    set(USE_METAL ON)

    if (PLATFORM_IOS)
        add_definitions(-DPLATFORM_IOS)
        message(STATUS "building for iOS")
        set(_moonlight_device_platform OS64)
        set(_moonlight_simulator_sdk iphonesimulator)
        set(_moonlight_device_triplet arm64-ios)
        set(_moonlight_device_sdk iphoneos)
        if (_moonlight_simulator_arch STREQUAL "arm64")
            set(_moonlight_simulator_platform SIMULATORARM64)
            set(_moonlight_simulator_triplet arm64-ios-simulator)
        else ()
            set(_moonlight_simulator_platform SIMULATOR64)
            set(_moonlight_simulator_triplet x64-ios)
        endif ()
    elseif (PLATFORM_TVOS)
        add_definitions(-DPLATFORM_TVOS)
        message(STATUS "building for tvOS")
        set(_moonlight_device_platform TVOS)
        set(_moonlight_device_triplet arm64-tvos)
        set(_moonlight_device_sdk appletvos)
        set(_moonlight_simulator_sdk appletvsimulator)
        if (_moonlight_simulator_arch STREQUAL "arm64")
            set(_moonlight_simulator_platform SIMULATORARM64_TVOS)
            set(_moonlight_simulator_triplet arm64-tvos-simulator)
        else ()
            set(_moonlight_simulator_platform SIMULATOR_TVOS)
            set(_moonlight_simulator_triplet x64-tvos-simulator)
        endif ()

        # Allow VCPKG to build for unsupported tvOS platform skipping platform checks
        set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")
    elseif (PLATFORM_VISIONOS)
        add_definitions(-DPLATFORM_VISIONOS)
        message(STATUS "building for visionOS")
        set(_moonlight_device_platform VISIONOS)
        set(_moonlight_device_triplet arm64-visionos)
        set(_moonlight_device_sdk xros)
        set(_moonlight_simulator_sdk xrsimulator)
        if (_moonlight_simulator_arch STREQUAL "arm64")
            set(_moonlight_simulator_platform SIMULATORARM64_VISIONOS)
            set(_moonlight_simulator_triplet arm64-visionos-simulator)
        else ()
            set(_moonlight_simulator_platform SIMULATOR_VISIONOS)
            set(_moonlight_simulator_triplet x64-visionos-simulator)
        endif ()

        # visionOS is not part of the bundled vcpkg platform matrix yet.
        set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")
    endif ()

    set(ENABLE_STRICT_TRY_COMPILE ON)
    if (NOT DEFINED BOREALIS_LIBRARY)
        message(FATAL_ERROR BOREALIS_LIBRARY is not defined)
    endif ()
    set(_moonlight_vcpkg_overlay_triplets ${EXTERN_PATH}/cmake/vcpkg-triplets)
    if (DEFINED VCPKG_OVERLAY_TRIPLETS)
        list(APPEND _moonlight_vcpkg_overlay_triplets ${VCPKG_OVERLAY_TRIPLETS})
    endif ()
    list(REMOVE_DUPLICATES _moonlight_vcpkg_overlay_triplets)
    set(VCPKG_OVERLAY_TRIPLETS "${_moonlight_vcpkg_overlay_triplets}" CACHE STRING "Overlay triplets to use for Apple mobile builds")
    unset(_moonlight_vcpkg_overlay_triplets)

    if (PLATFORM_VISIONOS)
        set(DEPLOYMENT_TARGET 1.0)

        # Force manifest mode to use the live vendored ports tree so local visionOS
        # fixes are applied instead of a versioned snapshot cached by vcpkg.
        set(_moonlight_vcpkg_overlay_ports ${EXTERN_PATH}/vcpkg/ports)
        if (DEFINED VCPKG_OVERLAY_PORTS)
            list(APPEND _moonlight_vcpkg_overlay_ports ${VCPKG_OVERLAY_PORTS})
        endif ()
        list(REMOVE_DUPLICATES _moonlight_vcpkg_overlay_ports)
        set(VCPKG_OVERLAY_PORTS "${_moonlight_vcpkg_overlay_ports}" CACHE STRING "Overlay ports to use for visionOS builds")
        unset(_moonlight_vcpkg_overlay_ports)
    else ()
        set(DEPLOYMENT_TARGET 13.0)
    endif ()

    if (APPLE_MOBILE_SDK_VARIANT STREQUAL "SIMULATOR")
        set(PLATFORM "${_moonlight_simulator_platform}")
        if (NOT VCPKG_TARGET_TRIPLET)
            set(VCPKG_TARGET_TRIPLET "${_moonlight_simulator_triplet}" CACHE STRING "vcpkg triplet for Apple mobile builds" FORCE)
        endif ()
        set(MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK OFF)
    elseif (APPLE_MOBILE_SDK_VARIANT STREQUAL "DEVICE")
        set(PLATFORM "${_moonlight_device_platform}")
        if (NOT VCPKG_TARGET_TRIPLET)
            set(VCPKG_TARGET_TRIPLET "${_moonlight_device_triplet}" CACHE STRING "vcpkg triplet for Apple mobile builds" FORCE)
        endif ()
        set(MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK OFF)
    else ()
        set(PLATFORM "${_moonlight_device_platform}")
        if (NOT VCPKG_TARGET_TRIPLET)
            set(VCPKG_TARGET_TRIPLET "${_moonlight_device_triplet}" CACHE STRING "vcpkg triplet for Apple mobile builds" FORCE)
        endif ()
        set(MOONLIGHT_APPLE_MOBILE_USE_MULTI_SDK ON)
    endif ()

    set(MOONLIGHT_APPLE_MOBILE_SDK_VARIANT "${APPLE_MOBILE_SDK_VARIANT}")
    set(MOONLIGHT_APPLE_MOBILE_VCPKG_PRIMARY_TRIPLET "${VCPKG_TARGET_TRIPLET}")
    set(MOONLIGHT_APPLE_MOBILE_VCPKG_DEVICE_TRIPLET "${_moonlight_device_triplet}")
    set(MOONLIGHT_APPLE_MOBILE_VCPKG_SIMULATOR_TRIPLET "${_moonlight_simulator_triplet}")
    set(MOONLIGHT_APPLE_MOBILE_XCODE_DEVICE_SDK "${_moonlight_device_sdk}")
    set(MOONLIGHT_APPLE_MOBILE_XCODE_SIMULATOR_SDK "${_moonlight_simulator_sdk}")
    message(STATUS "Apple mobile SDK variant: ${APPLE_MOBILE_SDK_VARIANT}")

    set(CMAKE_TOOLCHAIN_FILE ${EXTERN_PATH}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE PATH "vcpkg toolchain file")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${BOREALIS_LIBRARY}/cmake/ios.toolchain.cmake CACHE PATH "ios toolchain file")
    set(CMAKE_SHARED_LINKER_FLAGS "-rpath @executable_path/Frameworks -rpath @loader_path/Frameworks")
    set(USE_SDL2 ON)
    set(USE_GLFW OFF)
    if (PLATFORM_VISIONOS)
        set(USE_GL3 OFF CACHE BOOL "using OpenGL 3.2+" FORCE)
        set(USE_GL2 OFF CACHE BOOL "using OpenGL 2.1" FORCE)
        set(USE_GLES2 OFF CACHE BOOL "using OpenGL ES 2.0" FORCE)
        set(USE_GLES3 OFF CACHE BOOL "using OpenGL ES 3.0" FORCE)
        set(USE_METAL ON CACHE BOOL "using Metal" FORCE)
        set(BOREALIS_USE_METAL ON)
    else ()
        set(USE_GLES3 ON CACHE BOOL "using OpenGL ES 3.0" FORCE)
    endif ()
    set(USE_LIBROMFS ON)

    if (PLATFORM_IOS)
        set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2") # iphone, ipad
    elseif (PLATFORM_TVOS)
        set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3") # tvos
    elseif (PLATFORM_VISIONOS)
        set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "7") # Apple Vision
        set(CMAKE_XCODE_ATTRIBUTE_XROS_DEPLOYMENT_TARGET "${DEPLOYMENT_TARGET}")
    endif ()

    check_libromfs_generator()
elseif(PLATFORM_ANDROID)
    add_definitions(-DPLATFORM_ANDROID)
    message(STATUS "building for Android")
    set(CMAKE_TOOLCHAIN_FILE ${EXTERN_PATH}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE PATH "vcpkg toolchain file")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE $ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake CACHE PATH "android toolchain file")
    # set(ANDROID_ABI arm64-v8a)
    # set(ANDROID_ABI armeabi-v7a)
    set(USE_SDL2 ON)
    set(USE_GLFW OFF)
    set(USE_GLES3 ON)
    # set(USE_GL2 ON)
    set(USE_LIBROMFS ON)
    check_libromfs_generator()
elseif(PLATFORM_PSV)
    add_definitions(-DPLATFORM_PSV)
    message(STATUS "building for PSVita")
    set(USE_SDL2 ON)
    set(USE_GLFW OFF)
    set(USE_GLES2 ON)
    if (NOT DEFINED ENV{VITASDK})
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif ()
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    set(VITASDK $ENV{VITASDK} CACHE BOOL "VITASDK")
    include("${VITASDK}/share/vita.cmake" REQUIRED)
    add_definitions(-D__psp2__ -D__PSV__)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=pedantic -Wno-psabi -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=pedantic -Wno-psabi -g")
elseif (PLATFORM_SWITCH)
    add_definitions(-DPLATFORM_SWITCH)
    message(STATUS "building for SWITCH")
    # set(USE_SDL2 ON)
    set(USE_GLFW ON)
    set(USE_DEKO3D ON)

    if (NOT DEFINED ENV{DEVKITPRO})
        message(FATAL_ERROR "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
    endif ()
    set(DEVKITPRO $ENV{DEVKITPRO} CACHE BOOL "DEVKITPRO")
    set(__SWITCH__ ON)
    add_compile_definitions("HAS_SOCKLEN_T=1") # Requires by GitHub Workflow for Enet
    set(CMAKE_C_FLAGS "-I${DEVKITPRO}/libnx/include -I${DEVKITPRO}/portlibs/switch/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
    include(${DEVKITPRO}/cmake/Switch.cmake REQUIRED)
else()
    message(FATAL_ERROR "Please set build target. Example: -DPLATFORM_DESKTOP=ON or -DPLATFORM_SWITCH=ON")
endif ()

# OpenGL driver
if (USE_DEKO3D)
    message(STATUS "USE_DEKO3D")
    set(BOREALIS_USE_DEKO3D ON)
elseif (USE_METAL)
    message(STATUS "USE_METAL")
    set(BOREALIS_USE_METAL ON)
elseif (USE_GL2)
    message(STATUS "USE_GL2")
    add_definitions(-DUSE_GL2)
elseif (USE_GLES2)
    message(STATUS "USE_GLES2")
    add_definitions(-DUSE_GLES2)
elseif (USE_GLES3)
    message(STATUS "USE_GLES3")
    add_definitions(-DUSE_GLES3)
else ()
    message(STATUS "USE_GL3/4")
endif ()

# SDL or GLFW
if (USE_SDL2)
    message(STATUS "SDL2")
    set(USE_SDL2 ON)
    set(USE_GLFW OFF)
    add_definitions(-D__SDL2__)
else ()
    message(STATUS "GLFW")
    set(USE_SDL2 OFF)
    set(USE_GLFW ON)
    add_definitions(-D__GLFW__)
endif ()

# todo: Remove this once https://github.com/xfangfang/wiliwili/pull/173 is merged
function(main_target var1 var2 var3)
    if (WIN32)
        if (WIN32_TERMINAL)
            add_executable(${var1} ${var2} ${var3})
        else ()
            add_executable(${var1} WIN32 ${var2} ${var3})
        endif ()
    elseif (PLATFORM_ANDROID)
        add_library(${var1} SHARED)
        target_sources(${var1} PRIVATE ${var2})
    else ()
        add_executable(${var1} ${var2})
    endif ()
endfunction()

function(program_target target source)
    if (WIN32 AND NOT WIN32_TERMINAL)
        add_executable(${target} WIN32 ${source})
    elseif (PLATFORM_ANDROID)
        add_library(${target} SHARED)
        target_sources(${target} PRIVATE ${source})
    else ()
        add_executable(${target} ${source})
    endif ()
endfunction()

function(ios_bundle iosStoryBoard assets plist name version)
    set(IOS_CODE_SIGN_IDENTITY "" CACHE STRING "The code sign identity to use when building the IPA.")
    set(IOS_GUI_IDENTIFIER "" CACHE STRING "The package name")
    if(IOS_CODE_SIGN_IDENTITY STREQUAL "")
        set(IOS_CODE_SIGNING_ENABLED NO)
    else()
        set(IOS_CODE_SIGNING_ENABLED YES)
    endif()
    if (IOS_GUI_IDENTIFIER STREQUAL "")
        set(IOS_GUI_IDENTIFIER "${PACKAGE_NAME}")
        message(WARNING "Using default package name: ${PACKAGE_NAME}")
    endif ()

    set(IOS_SPLASH_STORYBOARD "${iosStoryBoard}")

    set(IOS_ASSETS ${assets})
    set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${IOS_CODE_SIGN_IDENTITY}"
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED YES
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "${IOS_CODE_SIGNING_ENABLED}"
            BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${plist}
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER ${IOS_GUI_IDENTIFIER}
            MACOSX_BUNDLE_BUNDLE_NAME ${name}
            MACOSX_BUNDLE_BUNDLE_VERSION "${version}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${version}"
            XCODE_ATTRIBUTE_ENABLE_BITCODE NO
            XCODE_ATTRIBUTE_SKIP_INSTALL NO
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
            )
    set_property(
            SOURCE ${IOS_ASSETS}
            PROPERTY MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_property(
            SOURCE ${IOS_SPLASH_STORYBOARD}
            PROPERTY MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${PROJECT_NAME} PRIVATE
            ${IOS_ASSETS}
            ${IOS_SPLASH_STORYBOARD}
            )
endfunction()

function(linux_install res res_install svg_icon icon_res_dir icon_res_list appdata desktop)
    message("install prefix: ${CMAKE_INSTALL_PREFIX}")
    # bin
    install(TARGETS ${PROJECT_NAME}
            DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
    # resources
    install(DIRECTORY ${res}
            DESTINATION ${res_install}
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    # svg icon
    install(FILES ${svg_icon}
            DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    # png icon
    foreach (icon_dir ${icon_res_list})
        install(FILES ${icon_res_dir}/${icon_dir}/${PACKAGE_NAME}.png
                DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/${icon_dir}/apps
                PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endforeach ()
    # appdata
    install(FILES ${appdata}
            DESTINATION ${CMAKE_INSTALL_PREFIX}/share/metainfo
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    # desktop
    install(FILES ${desktop} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
endfunction()

function(add_libromfs app res)
    message(STATUS "USE LIBROMFS")
    add_definitions(-DUSE_LIBROMFS)
    set(LIBROMFS_PROJECT_NAME ${app} PARENT_SCOPE)
    set(LIBROMFS_RESOURCE_LOCATION "${res}" PARENT_SCOPE)
endfunction()

function(git_info tag short)
    # Add git info
    find_package(Git)
    if(GIT_EXECUTABLE)
        execute_process(COMMAND git describe --tags
                TIMEOUT 5
                OUTPUT_VARIABLE GIT_TAG_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )
        execute_process(COMMAND git rev-parse --short HEAD
                TIMEOUT 5
                OUTPUT_VARIABLE GIT_TAG_SHORT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )
        message(STATUS "building from git tag: ${GIT_TAG_VERSION}")
        message(STATUS "building from git commit: ${GIT_TAG_SHORT}")
    else ()
        set(GIT_TAG_VERSION "")
        set(GIT_TAG_SHORT "")
    endif ()
    set(${tag} ${GIT_TAG_VERSION} PARENT_SCOPE)
    set(${short} ${GIT_TAG_SHORT} PARENT_SCOPE)
endfunction()

# switch platform
# glsl -> deko3d dksh
function(gen_dksh romfs_dir)
    find_program(NX_UAM_EXE NAMES uam HINTS "${DEVKITPRO}/tools/bin")
    if (NOT NX_UAM_EXE)
        message(FATAL_ERROR "Could not find uam: try installing uam")
    endif()
    if (NOT DEFINED BOREALIS_LIBRARY)
        message(FATAL_ERROR BOREALIS_LIBRARY is not defined)
    endif ()

    set(NANOVG_SHADER_BASE "${BOREALIS_LIBRARY}/lib/extern/nanovg/deko3d/shaders")

    file(MAKE_DIRECTORY "${romfs_dir}")
    execute_process(COMMAND ${NX_UAM_EXE} -s vert -o "${romfs_dir}/fill_vsh.dksh" "${NANOVG_SHADER_BASE}/fill_vsh.glsl" TIMEOUT 5)
    execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/fill_aa_fsh.dksh" "${NANOVG_SHADER_BASE}/fill_aa_fsh.glsl" TIMEOUT 5)
    execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/fill_fsh.dksh" "${NANOVG_SHADER_BASE}/fill_fsh.glsl" TIMEOUT 5)

    execute_process(COMMAND ${NX_UAM_EXE} -s vert -o "${romfs_dir}/basic_vsh.dksh" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/video/deko3d/basic_vsh.glsl" TIMEOUT 5)
    execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/color_fsh.dksh" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/video/deko3d/color_fsh.glsl" TIMEOUT 5)
    execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/texture_fsh.dksh" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/video/deko3d/texture_fsh.glsl" TIMEOUT 5)
endfunction()
