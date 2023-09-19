
add_subdirectory(borealis/library)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
find_library(MbedTLS MbedTLS)

set(BUILD_SHARED_LIBS OFF)
set(USE_MBEDTLS ON)
set(USE_MBEDTLS_CRYPTO ON)
add_subdirectory(moonlight-common-c)

add_library(CImg INTERFACE)
target_include_directories(CImg INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/CImg/CImg.h)