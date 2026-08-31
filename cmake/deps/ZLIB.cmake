set(ZLIB_DEP_VERSION 1.2.11)
# madler/zlib tag archive (zlib.net fossil URLs are unreachable from this env).
set(ZLIB_DEP_URL "https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_DEP_VERSION}.tar.gz"
    CACHE STRING "zlib source URL")
set(ZLIB_DEP_SHA256
    "629380c90a77b964d896ed37163f5c3a34f6e6d897311f1df2a7016355c45eff"
    CACHE STRING "zlib source sha256 (fill to enable integrity check)")

set(ZLIB_FETCHED FALSE)
if(NOT ODPS_FORCE_FETCH_DEPS AND NOT FORCE_FETCH_ZLIB)
    find_package(ZLIB QUIET)
endif()

if(ZLIB_FOUND)
    message(STATUS "Using system zlib: ${ZLIB_LIBRARIES}")
    set(ZLIB_PREFIX "/usr")
else()
    set(ZLIB_FETCHED TRUE)
    set(_hash_args "")
    if(ZLIB_DEP_SHA256)
        set(_hash_args URL_HASH SHA256=${ZLIB_DEP_SHA256})
    endif()
    # The madler/zlib tag archive has no CMakeLists.txt; it ships a
    # hand-written ./configure. Build in source with it.
    ExternalProject_Add(zlib_ext
        URL ${ZLIB_DEP_URL}
        ${_hash_args}
        DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
        PREFIX ${CMAKE_BINARY_DIR}/ep/zlib
        INSTALL_DIR ${DEPS_PREFIX}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./configure --prefix=${DEPS_PREFIX}
        BUILD_COMMAND make -j${ODPS_NPROC}
        INSTALL_COMMAND make install
    )
    list(APPEND ODPS_EXT_TARGETS zlib_ext)
    set(ZLIB_PREFIX ${DEPS_PREFIX})
endif()
