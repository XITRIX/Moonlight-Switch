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

set(_mbedtls_search_roots)
if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND _mbedtls_search_roots "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()
foreach(_mbedtls_root_var MbedTLS_ROOT mbedTLS_ROOT MBEDTLS_ROOT)
    if (DEFINED ${_mbedtls_root_var} AND NOT "${${_mbedtls_root_var}}" STREQUAL "")
        list(APPEND _mbedtls_search_roots "${${_mbedtls_root_var}}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _mbedtls_search_roots)

function(_mbedtls_find_path _out_var)
    if (_mbedtls_search_roots)
        find_path(${_out_var}
                NAMES ${ARGN}
                HINTS ${_mbedtls_search_roots}
                PATH_SUFFIXES include
                NO_DEFAULT_PATH)
    endif()
    if (NOT ${_out_var})
        find_path(${_out_var}
                NAMES ${ARGN}
                PATH_SUFFIXES include)
    endif()
    set(${_out_var} "${${_out_var}}" PARENT_SCOPE)
endfunction()

function(_mbedtls_find_library _out_var _lib_name)
    if (_mbedtls_search_roots)
        find_library(${_out_var}
                NAMES ${_lib_name}
                HINTS ${_mbedtls_search_roots}
                PATH_SUFFIXES lib
                NO_DEFAULT_PATH)
    endif()
    if (NOT ${_out_var})
        find_library(${_out_var}
                NAMES ${_lib_name}
                PATH_SUFFIXES lib)
    endif()
    set(${_out_var} "${${_out_var}}" PARENT_SCOPE)
endfunction()

function(_mbedtls_find_debug_library _out_var _lib_name)
    if (_mbedtls_search_roots)
        find_library(${_out_var}
                NAMES ${_lib_name}
                HINTS ${_mbedtls_search_roots}
                PATH_SUFFIXES debug/lib
                NO_DEFAULT_PATH)
    endif()
    if (NOT ${_out_var})
        find_library(${_out_var}
                NAMES ${_lib_name}
                PATH_SUFFIXES debug/lib)
    endif()
    set(${_out_var} "${${_out_var}}" PARENT_SCOPE)
endfunction()

function(_mbedtls_add_target _target _release_lib _debug_lib)
    if (TARGET ${_target})
        return()
    endif()

    add_library(${_target} UNKNOWN IMPORTED)
    set_target_properties(${_target} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${mbedTLS_INCLUDE_DIRS}")

    if (_release_lib)
        set_property(TARGET ${_target} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION_RELEASE "${_release_lib}")
    endif()

    if (_debug_lib)
        set_property(TARGET ${_target} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION_DEBUG "${_debug_lib}")
    endif()

    if (_release_lib)
        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION "${_release_lib}")
    elseif (_debug_lib)
        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION "${_debug_lib}")
    endif()
endfunction()

if(mbedTLS_FIND_COMPONENTS)
    # Mark only needed components
    foreach(component ${mbedTLS_FIND_COMPONENTS})
        string(TOUPPER ${component} _COMPONENT)
        set(mbedTLS_FIND_COMPONENT_${_COMPONENT} ON)
    endforeach()
endif()

_mbedtls_find_path(mbedTLS_INCLUDE_DIR "mbedtls/version.h")
mark_as_advanced(mbedTLS_INCLUDE_DIR)
if(mbedTLS_INCLUDE_DIR AND EXISTS "${mbedTLS_INCLUDE_DIR}/mbedtls/version.h")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/mbedtls/version.h" VERSION_STRING_LINE REGEX "^#define MBEDTLS_VERSION_STRING[ \\t\\n\\r]+\"[^\"]*\"$")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/mbedtls/version.h" VERSION_MAJOR_LINE REGEX "^#define MBEDTLS_VERSION_MAJOR[ \\t\\n\\r]+[0-9]+$")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/mbedtls/version.h" VERSION_MINOR_LINE REGEX "^#define MBEDTLS_VERSION_MINOR[ \\t\\n\\r]+[0-9]+$")
    file(STRINGS "${mbedTLS_INCLUDE_DIR}/mbedtls/version.h" VERSION_PATCH_LINE REGEX "^#define MBEDTLS_VERSION_PATCH[ \\t\\n\\r]+[0-9]+$")

    string(REGEX REPLACE "^#define MBEDTLS_VERSION_STRING[ \\t\\n\\r]+\"([^\"]*)\"$" "\\1" mbedTLS_VERSION_STRING "${VERSION_STRING_LINE}")
    string(REGEX REPLACE "^#define MBEDTLS_VERSION_MAJOR[ \\t\\n\\r]+([0-9]+)$" "\\1" mbedTLS_VERSION_MAJOR "${VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define MBEDTLS_VERSION_MINOR[ \\t\\n\\r]+([0-9]+)$" "\\1" mbedTLS_VERSION_MINOR "${VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define MBEDTLS_VERSION_PATCH[ \\t\\n\\r]+([0-9]+)$" "\\1" mbedTLS_VERSION_PATCH "${VERSION_PATCH_LINE}")
endif()

if(NOT mbedTLS_FIND_COMPONENTS OR mbedTLS_FIND_COMPONENT_CRYPTO)
    _mbedtls_find_library(mbedTLS_CRYPTO_LIBRARY "mbedcrypto")
    _mbedtls_find_debug_library(mbedTLS_CRYPTO_LIBRARY_DEBUG "mbedcrypto")
    set(_REQUIRED_LIBS_LIST ${_REQUIRED_LIBS_LIST} mbedTLS_CRYPTO_LIBRARY)
endif()
if(NOT mbedTLS_FIND_COMPONENTS OR mbedTLS_FIND_COMPONENT_TLS)
    _mbedtls_find_library(mbedTLS_TLS_LIBRARY "mbedtls")
    _mbedtls_find_debug_library(mbedTLS_TLS_LIBRARY_DEBUG "mbedtls")
    set(_REQUIRED_LIBS_LIST ${_REQUIRED_LIBS_LIST} mbedTLS_TLS_LIBRARY)
endif()
if(NOT mbedTLS_FIND_COMPONENTS OR mbedTLS_FIND_COMPONENT_X509)
    _mbedtls_find_library(mbedTLS_X509_LIBRARY "mbedx509")
    _mbedtls_find_debug_library(mbedTLS_X509_LIBRARY_DEBUG "mbedx509")
    set(_REQUIRED_LIBS_LIST ${_REQUIRED_LIBS_LIST} mbedTLS_X509_LIBRARY)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MbedTLS REQUIRED_VARS mbedTLS_INCLUDE_DIR ${_REQUIRED_LIBS_LIST}
        VERSION_VAR mbedTLS_VERSION_STRING)
if(MbedTLS_FOUND)
    set(mbedTLS_INCLUDE_DIRS ${mbedTLS_INCLUDE_DIR})
    set(mbedTLS_LIBRARIES)
    if(mbedTLS_TLS_LIBRARY)
        list(APPEND mbedTLS_LIBRARIES "${mbedTLS_TLS_LIBRARY}")
    endif()
    if(mbedTLS_X509_LIBRARY)
        list(APPEND mbedTLS_LIBRARIES "${mbedTLS_X509_LIBRARY}")
    endif()
    if(mbedTLS_CRYPTO_LIBRARY)
        list(APPEND mbedTLS_LIBRARIES "${mbedTLS_CRYPTO_LIBRARY}")
    endif()

    set(MBEDTLS_INCLUDE_DIRS "${mbedTLS_INCLUDE_DIRS}")
    set(MBEDTLS_LIBRARY "${mbedTLS_TLS_LIBRARY}")
    set(MBEDX509_LIBRARY "${mbedTLS_X509_LIBRARY}")
    set(MBEDCRYPTO_LIBRARY "${mbedTLS_CRYPTO_LIBRARY}")
    set(MBEDTLS_LIBRARIES "${mbedTLS_LIBRARIES}")
    set(mbedTLS_FOUND "${MbedTLS_FOUND}")

    if(mbedTLS_CRYPTO_LIBRARY)
        _mbedtls_add_target(MbedTLS::mbedcrypto "${mbedTLS_CRYPTO_LIBRARY}" "${mbedTLS_CRYPTO_LIBRARY_DEBUG}")
    endif()
    if(mbedTLS_X509_LIBRARY)
        _mbedtls_add_target(MbedTLS::mbedx509 "${mbedTLS_X509_LIBRARY}" "${mbedTLS_X509_LIBRARY_DEBUG}")
        set_target_properties(MbedTLS::mbedx509 PROPERTIES
                INTERFACE_LINK_LIBRARIES "MbedTLS::mbedcrypto")
    endif()
    if(mbedTLS_TLS_LIBRARY)
        _mbedtls_add_target(MbedTLS::mbedtls "${mbedTLS_TLS_LIBRARY}" "${mbedTLS_TLS_LIBRARY_DEBUG}")
        set_target_properties(MbedTLS::mbedtls PROPERTIES
                INTERFACE_LINK_LIBRARIES "MbedTLS::mbedx509;MbedTLS::mbedcrypto")
    endif()

    if(NOT TARGET mbed::crypto AND TARGET MbedTLS::mbedcrypto)
        add_library(mbed::crypto INTERFACE IMPORTED)
        set_target_properties(mbed::crypto PROPERTIES
                INTERFACE_LINK_LIBRARIES "MbedTLS::mbedcrypto")
    endif()
    if(NOT TARGET mbed::x509 AND TARGET MbedTLS::mbedx509)
        add_library(mbed::x509 INTERFACE IMPORTED)
        set_target_properties(mbed::x509 PROPERTIES
                INTERFACE_LINK_LIBRARIES "MbedTLS::mbedx509")
    endif()
    if(NOT TARGET mbed::tls AND TARGET MbedTLS::mbedtls)
        add_library(mbed::tls INTERFACE IMPORTED)
        set_target_properties(mbed::tls PROPERTIES
                INTERFACE_LINK_LIBRARIES "MbedTLS::mbedtls")
    endif()
endif()

mark_as_advanced(
        mbedTLS_INCLUDE_DIR
        mbedTLS_CRYPTO_LIBRARY
        mbedTLS_CRYPTO_LIBRARY_DEBUG
        mbedTLS_TLS_LIBRARY
        mbedTLS_TLS_LIBRARY_DEBUG
        mbedTLS_X509_LIBRARY
        mbedTLS_X509_LIBRARY_DEBUG)
