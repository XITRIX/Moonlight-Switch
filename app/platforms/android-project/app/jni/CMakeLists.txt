cmake_minimum_required(VERSION 3.6)

set(ANDROID_NDK_HOME "/Users/xitrix/Library/Android/sdk/ndk-bundle")
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
