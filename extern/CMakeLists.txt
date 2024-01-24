
add_subdirectory(borealis/library)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(mbedTLS REQUIRED)

set(BUILD_SHARED_LIBS OFF)
set(USE_MBEDTLS ON)
add_subdirectory(moonlight-common-c)

# HacBrewPack
add_definitions(-DMBEDTLS_CMAC_C)
add_library(HackBrewPack
        hacBrewPack/mbedtls/library/cmac.c
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

target_link_libraries(HackBrewPack PRIVATE
        mbed::tls
        mbed::x509
        mbed::crypto
)
target_include_directories(HackBrewPack PRIVATE ${MBEDTLS_INCLUDE_DIRS})

# HacPackToolNACP
add_library(HacPackToolNACP
        hacPack/hacPack-Tools/hacPackTools-NACP/nacp.cpp
)
set_target_properties(HacPackToolNACP PROPERTIES CXX_STANDARD 17)

target_link_libraries(HackBrewPack PRIVATE
        tinyxml2
)
#target_include_directories(HackBrewPack PRIVATE ${MBEDTLS_INCLUDE_DIRS})
