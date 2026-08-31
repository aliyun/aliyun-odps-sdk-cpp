set(OPENSSL_DEP_VERSION 1.1.1g)
# openssl/openssl tag archive (www.openssl.org release CDN is unreachable from this env).
# Extracts to openssl-OpenSSL_1_1_1g/ and ships ./config (needs perl, present).
set(OPENSSL_DEP_URL "https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1g.tar.gz"
    CACHE STRING "openssl source URL")
set(OPENSSL_DEP_SHA256
    "281e4f13142b53657bd154481e18195b2d477572fdffa8ed1065f73ef5a19777"
    CACHE STRING "openssl source sha256")

set(OPENSSL_FETCHED FALSE)
if(NOT ODPS_FORCE_FETCH_DEPS AND NOT FORCE_FETCH_OPENSSL)
    find_package(OpenSSL QUIET)
endif()

if(OPENSSL_FOUND)
    message(STATUS "Using system OpenSSL: ${OPENSSL_LIBRARIES}")
    get_filename_component(OPENSSL_PREFIX "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
else()
    set(OPENSSL_FETCHED TRUE)
    set(_hash_args "")
    if(OPENSSL_DEP_SHA256)
        set(_hash_args URL_HASH SHA256=${OPENSSL_DEP_SHA256})
    endif()
    ExternalProject_Add(openssl_ext
        URL ${OPENSSL_DEP_URL}
        ${_hash_args}
        DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
        PREFIX ${CMAKE_BINARY_DIR}/ep/openssl
        INSTALL_DIR ${DEPS_PREFIX}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./config shared no-tests --prefix=${DEPS_PREFIX} --openssldir=${DEPS_PREFIX}/ssl
        BUILD_COMMAND make -j${ODPS_NPROC}
        INSTALL_COMMAND make install_sw
    )
    list(APPEND ODPS_EXT_TARGETS openssl_ext)
    set(OPENSSL_PREFIX ${DEPS_PREFIX})
endif()
