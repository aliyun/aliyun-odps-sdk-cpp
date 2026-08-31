set(CURL_DEP_VERSION 7.73.0)
# curl/curl tag archive (curl.se is unreachable from this env). Tag uses
# underscores: curl-7_73_0.
set(CURL_DEP_URL "https://github.com/curl/curl/archive/refs/tags/curl-7_73_0.tar.gz"
    CACHE STRING "curl source URL")
set(CURL_DEP_SHA256
    "552e401e9de5e35c67cddd130da5dcf0455caa1b42fe7ad044efc9c0de822ef6"
    CACHE STRING "curl source sha256")

set(CURL_FETCHED FALSE)
if(NOT ODPS_FORCE_FETCH_DEPS AND NOT FORCE_FETCH_CURL)
    find_package(CURL QUIET)
endif()

if(CURL_FOUND)
    message(STATUS "Using system curl: ${CURL_LIBRARIES}")
else()
    set(CURL_FETCHED TRUE)
    set(_hash_args "")
    if(CURL_DEP_SHA256)
        set(_hash_args URL_HASH SHA256=${CURL_DEP_SHA256})
    endif()
    set(_curl_ext_deps "")
    if(OPENSSL_FETCHED)
        list(APPEND _curl_ext_deps openssl_ext)
    endif()
    if(ZLIB_FETCHED)
        list(APPEND _curl_ext_deps zlib_ext)
    endif()
    # The tag archive has no pre-generated configure and autotools are absent
    # on this host, so build curl with its own CMake build (option names
    # verified against curl-7_73_0/CMakeLists.txt: CMAKE_USE_OPENSSL,
    # CURL_ZLIB, CMAKE_USE_LIBSSH2, CURL_DISABLE_LDAP/LDAPS).
    # CMAKE_PREFIX_PATH points at our deps prefix so find_package(OpenSSL)
    # and find_package(ZLIB) pick up the libs built above.
    ExternalProject_Add(curl_ext
        URL ${CURL_DEP_URL}
        ${_hash_args}
        DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
        PREFIX ${CMAKE_BINARY_DIR}/ep/curl
        INSTALL_DIR ${DEPS_PREFIX}
        DEPENDS ${_curl_ext_deps}
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX}
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_PREFIX_PATH=${DEPS_PREFIX}
            -DCMAKE_BUILD_TYPE=Release
            -DBUILD_SHARED_LIBS=ON
            -DBUILD_CURL_EXE=OFF
            -DCMAKE_USE_OPENSSL=ON
            -DCURL_ZLIB=ON
            -DCMAKE_USE_LIBSSH2=OFF
            -DCURL_DISABLE_LDAP=ON
            -DCURL_DISABLE_LDAPS=ON
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            ${ODPS_DEP_TOOLCHAIN_ARGS}
    )
    list(APPEND ODPS_EXT_TARGETS curl_ext)
endif()
