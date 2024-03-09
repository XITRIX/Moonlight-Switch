add_subdirectory(borealis/library)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

set(BUILD_SHARED_LIBS OFF)

if (USE_MBEDTLS_CRYPTO)
    find_library(MbedTLS MbedTLS)
    set(USE_MBEDTLS ON)
endif ()

add_subdirectory(moonlight-common-c)

if (NOT MULTICAST_DISABLED)
    set(MDNS_BUILD_EXAMPLE OFF)
    add_subdirectory(mdns)
endif ()

#include(FindPkgConfig)
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBAV REQUIRED
#        libavformat
#        libavcodec
#        libavutil
#        )



#pkg_check_modules(av REQUIRED IMPORTED_TARGET avcodec avformat avutil)
#add_library(libav ALIAS PkgConfig::av)