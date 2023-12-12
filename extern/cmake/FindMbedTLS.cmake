##
# Supported components: crypto tls x509
# This module defines
# mbed::crypto imported target
# mbedTLS_CRYPTO_LIBRARY
#
# mbed::tls imported target
# mbedTLS_TLS_LIBRARY
#
# mbed::x509 imported target
# mbedTLS_X509_LIBRARY
# 
# And also
# mbedTLS_LIBRARIES
# mbedTLS_INCLUDE_DIRS
# mbedTLS_FOUND
# mbedTLS_VERSION_STRING
# mbedTLS_VERSION_MAJOR
# mbedTLS_VERSION_MINOR
# mbedTLS_VERSION_PATCH
##

if(mbedTLS_FIND_COMPONENTS)
    # Mark only needed components
    foreach(component ${mbedTLS_FIND_COMPONENTS})
        string(TOUPPER ${component} _COMPONENT)
        set(mbedTLS_FIND_COMPONENT_${_COMPONENT} ON)
    endforeach()
endif()

find_path(mbedTLS_INCLUDE_DIR "version.h" PATH_SUFFIXES "mbedtls" DOC "mbedtls include directory")
mark_as_advanced(mbedTLS_INCLUDE_DIR)
if(mbedTLS_INCLUDE_DIR AND EXISTS "${mbedTLS_INCLUDE_DIR}/version.h")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/version.h" VERSION_STRING_LINE REGEX "^#define MBEDTLS_VERSION_STRING[ \\t\\n\\r]+\"[^\"]*\"$")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/version.h" VERSION_MAJOR_LINE REGEX "^#define MBEDTLS_VERSION_MAJOR[ \\t\\n\\r]+[0-9]+$")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/version.h" VERSION_MINOR_LINE REGEX "^#define MBEDTLS_VERSION_MINOR[ \\t\\n\\r]+[0-9]+$")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/version.h" VERSION_PATCH_LINE REGEX "^#define MBEDTLS_VERSION_PATCH[ \\t\\n\\r]+[0-9]+$")

    string(REGEX REPLACE "^#define MBEDTLS_VERSION_STRING[ \\t\\n\\r]+\"([^\"]*)\"$" "\\1" mbedTLS_VERSION_STRING "${VERSION_STRING_LINE}")
    string(REGEX REPLACE "^#define MBEDTLS_VERSION_MAJOR[ \\t\\n\\r]+([0-9]+)$" "\\1" mbedTLS_VERSION_MAJOR "${VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define MBEDTLS_VERSION_MINOR[ \\t\\n\\r]+([0-9]+)$" "\\1" mbedTLS_VERSION_MINOR "${VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define MBEDTLS_VERSION_PATCH[ \\t\\n\\r]+([0-9]+)$" "\\1" mbedTLS_VERSION_PATCH "${VERSION_PATCH_LINE}")
endif()

if(NOT mbedTLS_FIND_COMPONENTS OR mbedTLS_FIND_COMPONENT_CRYPTO)
    find_library(mbedTLS_CRYPTO_LIBRARY "mbedcrypto" DOC "Path to mbedcrypto library")
    set(_REQUIRED_LIBS_LIST ${_REQUIRED_LIBS_LIST} mbedTLS_CRYPTO_LIBRARY)
endif()
if(NOT mbedTLS_FIND_COMPONENTS OR mbedTLS_FIND_COMPONENT_TLS)
    find_library(mbedTLS_TLS_LIBRARY "mbedtls" DOC "Path to mbedtls library")
    set(_REQUIRED_LIBS_LIST ${_REQUIRED_LIBS_LIST} mbedTLS_TLS_LIBRARY)
endif()
if(NOT mbedTLS_FIND_COMPONENTS OR mbedTLS_FIND_COMPONENT_X509)
    find_library(mbedTLS_X509_LIBRARY "mbedx509" DOC "Path to mbedx509 library")
    set(_REQUIRED_LIBS_LIST ${_REQUIRED_LIBS_LIST} mbedTLS_X509_LIBRARY)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(mbedTLS REQUIRED_VARS mbedTLS_INCLUDE_DIR mbedTLS_VERSION_STRING mbedTLS_VERSION_MAJOR
        mbedTLS_VERSION_MINOR mbedTLS_VERSION_PATCH ${_REQUIRED_LIBS_LIST}
        VERSION_VAR mbedTLS_VERSION_STRING)
if(mbedTLS_FOUND)
    set(mbedTLS_INCLUDE_DIRS ${mbedTLS_INCLUDE_DIR})
    set(mbedTLS_LIBRARIES ${_REQUIRED_LIBS_LIST})

    if(NOT TARGET mbed::crypto AND mbedTLS_CRYPTO_LIBRARY)
        add_library(mbed::crypto UNKNOWN IMPORTED)
        set_target_properties(mbed::crypto PROPERTIES
                IMPORTED_LOCATION "${mbedTLS_CRYPTO_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${mbedTLS_INCLUDE_DIRS}")
    endif()
    if(NOT TARGET mbed::tls AND mbedTLS_TLS_LIBRARY)
        add_library(mbed::tls UNKNOWN IMPORTED)
        set_target_properties(mbed::tls PROPERTIES
                IMPORTED_LOCATION "${mbedTLS_TLS_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${mbedTLS_INCLUDE_DIRS}")
    endif()
    if(NOT TARGET mbed::x509 AND mbedTLS_X509_LIBRARY)
        add_library(mbed::x509 UNKNOWN IMPORTED)
        set_target_properties(mbed::x509 PROPERTIES
                IMPORTED_LOCATION "${mbedTLS_X509_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${mbedTLS_INCLUDE_DIRS}")
    endif()
endif()