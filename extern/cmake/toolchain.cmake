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
elseif (PLATFORM_IOS OR PLATFORM_TVOS)
    if (PLATFORM_IOS)
        add_definitions(-DPLATFORM_IOS)
        message(STATUS "building for iOS")
        set(PLATFORM OS64)
    elseif (PLATFORM_TVOS)
        add_definitions(-DPLATFORM_TVOS)
        message(STATUS "building for tvOS")
        set(PLATFORM TVOS)

        # Allow VCPKG to build for unsupported tvOS platform skipping platform checks
        set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")
    endif ()

    set(ENABLE_STRICT_TRY_COMPILE ON)
    if (NOT DEFINED BOREALIS_LIBRARY)
        message(FATAL_ERROR BOREALIS_LIBRARY is not defined)
    endif ()
    set(DEPLOYMENT_TARGET 13.0)
    set(CMAKE_TOOLCHAIN_FILE ${EXTERN_PATH}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE PATH "vcpkg toolchain file")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${BOREALIS_LIBRARY}/cmake/ios.toolchain.cmake CACHE PATH "ios toolchain file")
    set(CMAKE_SHARED_LINKER_FLAGS "-rpath @executable_path/Frameworks -rpath @loader_path/Frameworks")
    set(USE_SDL2 ON)
    set(USE_GLFW OFF)
    set(USE_GLES3 ON)
    set(USE_LIBROMFS ON)

    if (PLATFORM_IOS)
        set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2") # iphone, ipad
    elseif (PLATFORM_TVOS)
        set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3") # tvos
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

if (PLATFORM_IOS OR PLATFROM_TVOS OR PLATFORM_MACOS)
    set(PLATFORM_APPLE ON)
    add_definitions(-DPLATFORM_APPLE)
endif()

# OpenGL driver
if (USE_DEKO3D)
    message(STATUS "USE_DEKO3D")
    set(BOREALIS_USE_DEKO3D ON)
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

    # execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/basic_vsh.dksh" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/video/deko3d/basic_vsh.glsl" TIMEOUT 5)
    # execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/color_fsh.dksh" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/video/deko3d/color_fsh.glsl" TIMEOUT 5)
    execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${romfs_dir}/texture_fsh.dksh" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/streaming/video/deko3d/texture_fsh.glsl" TIMEOUT 5)
endfunction()