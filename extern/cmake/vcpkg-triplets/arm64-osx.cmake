set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

# mbedTLS 2.28 and other dependencies in the pinned vcpkg baseline still
# declare compatibility with CMake policy versions older than CMake 4 accepts.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
