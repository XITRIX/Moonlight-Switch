include("${CMAKE_CURRENT_LIST_DIR}/../../vcpkg/triplets/arm64-android.cmake")

# The pinned Jansson port declares CMake 3.1 policies, which CMake 4 rejects.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
