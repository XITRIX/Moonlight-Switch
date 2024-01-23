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

add_library(HackBrewPack
        hacBrewPack/aes.c
        hacBrewPack/cnmt.c
        hacBrewPack/ConvertUTF.c
        hacBrewPack/extkeys.c
        hacBrewPack/filepath.c
        hacBrewPack/ivfc.c
        hacBrewPack/nacp.c
        hacBrewPack/nca.c
        hacBrewPack/npdm.c
        hacBrewPack/pfs0.c
        hacBrewPack/pki.c
        hacBrewPack/romfs.c
        hacBrewPack/rsa.c
        hacBrewPack/sha.c
        hacBrewPack/utils.c
)
#target_link_libraries(HackBrewPack PRIVATE mbed::crypto)
target_link_libraries(HackBrewPack PRIVATE ${MBEDCRYPTO_LIBRARY})
target_include_directories(HackBrewPack SYSTEM PRIVATE ${MBEDTLS_INCLUDE_DIRS})

#include(FindPkgConfig)
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBAV REQUIRED
#        libavformat
#        libavcodec
#        libavutil
#        )



#pkg_check_modules(av REQUIRED IMPORTED_TARGET avcodec avformat avutil)
#add_library(libav ALIAS PkgConfig::av)