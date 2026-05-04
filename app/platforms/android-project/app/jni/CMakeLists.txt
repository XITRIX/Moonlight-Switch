cmake_minimum_required(VERSION 3.6)

if (DEFINED ANDROID_NDK AND EXISTS "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    set(ANDROID_NDK_HOME "${ANDROID_NDK}")
elseif (DEFINED ANDROID_NDK_HOME AND EXISTS "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
    set(ANDROID_NDK_HOME "${ANDROID_NDK_HOME}")
elseif (DEFINED ENV{ANDROID_NDK_HOME} AND EXISTS "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
    set(ANDROID_NDK_HOME "$ENV{ANDROID_NDK_HOME}")
elseif (DEFINED ENV{ANDROID_NDK_ROOT} AND EXISTS "$ENV{ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake")
    set(ANDROID_NDK_HOME "$ENV{ANDROID_NDK_ROOT}")
elseif (DEFINED ENV{ANDROID_NDK} AND EXISTS "$ENV{ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    set(ANDROID_NDK_HOME "$ENV{ANDROID_NDK}")
else ()
    set(ANDROID_SDK_ROOT "")
    if (DEFINED ENV{ANDROID_SDK_ROOT} AND EXISTS "$ENV{ANDROID_SDK_ROOT}")
        set(ANDROID_SDK_ROOT "$ENV{ANDROID_SDK_ROOT}")
    elseif (DEFINED ENV{ANDROID_HOME} AND EXISTS "$ENV{ANDROID_HOME}")
        set(ANDROID_SDK_ROOT "$ENV{ANDROID_HOME}")
    endif ()

    if (ANDROID_SDK_ROOT)
        if (EXISTS "${ANDROID_SDK_ROOT}/ndk-bundle/build/cmake/android.toolchain.cmake")
            set(ANDROID_NDK_HOME "${ANDROID_SDK_ROOT}/ndk-bundle")
        else ()
            file(GLOB _BRLS_ANDROID_NDK_CANDIDATES LIST_DIRECTORIES true "${ANDROID_SDK_ROOT}/ndk/*")
            if (_BRLS_ANDROID_NDK_CANDIDATES)
                list(SORT _BRLS_ANDROID_NDK_CANDIDATES)
                list(REVERSE _BRLS_ANDROID_NDK_CANDIDATES)
                list(GET _BRLS_ANDROID_NDK_CANDIDATES 0 ANDROID_NDK_HOME)
            endif ()
        endif ()
    endif ()
endif ()

if (NOT ANDROID_NDK_HOME OR NOT EXISTS "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
    message(FATAL_ERROR "Unable to resolve Android NDK. Set ANDROID_NDK_HOME, ANDROID_NDK_ROOT, ANDROID_NDK, ANDROID_SDK_ROOT, or ANDROID_HOME to a valid installation")
endif ()

set(PROJECT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
set(CMAKE_TOOLCHAIN_FILE ${PROJECT_PATH}/extern/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE PATH "vcpkg toolchain file" FORCE)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake CACHE PATH "android toolchain file")
#message(Toolchain!!!! ${CMAKE_TOOLCHAIN_FILE})

project(Moonlight)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()


# SDL sources are in a subfolder named "SDL"
add_subdirectory(SDL)

# Compilation of companion libraries
#add_subdirectory(SDL_image)
#add_subdirectory(SDL_mixer)
#add_subdirectory(SDL_ttf)

# Your game and its CMakeLists.txt are in a subfolder named "src"
#add_subdirectory(src)


SET(PLATFORM_ANDROID ON CACHE BOOL "")
SET(LIBROMFS_PREBUILT_GENERATOR "${PROJECT_PATH}/libromfs-generator" CACHE STRING "")

if (NOT EXISTS "${LIBROMFS_PREBUILT_GENERATOR}")
    message(FATAL_ERROR "libromfs-generator has not been built, please refer to borealis/build_libromfs_generator.sh for more information")
endif()

add_subdirectory(moonlight)
