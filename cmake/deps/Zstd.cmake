set(ZSTD_DEP_VERSION 1.4.5)
# facebook/zstd tag archive (github release-asset downloads are blocked in this env).
set(ZSTD_DEP_URL "https://github.com/facebook/zstd/archive/refs/tags/v${ZSTD_DEP_VERSION}.tar.gz"
    CACHE STRING "zstd source URL")
set(ZSTD_DEP_SHA256
    "734d1f565c42f691f8420c8d06783ad818060fc390dee43ae0a89f86d0a4f8c2"
    CACHE STRING "zstd source sha256")

set(ZSTD_FETCHED FALSE)
if(NOT ODPS_FORCE_FETCH_DEPS AND NOT FORCE_FETCH_ZSTD)
    find_library(ZSTD_SYS_LIB zstd)
    find_path(ZSTD_SYS_INC zstd.h)
endif()

if(ZSTD_SYS_LIB AND ZSTD_SYS_INC)
    message(STATUS "Using system zstd: ${ZSTD_SYS_LIB}")
else()
    set(ZSTD_FETCHED TRUE)
    set(_hash_args "")
    if(ZSTD_DEP_SHA256)
        set(_hash_args URL_HASH SHA256=${ZSTD_DEP_SHA256})
    endif()
    ExternalProject_Add(zstd_ext
        URL ${ZSTD_DEP_URL}
        ${_hash_args}
        DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
        PREFIX ${CMAKE_BINARY_DIR}/ep/zstd
        INSTALL_DIR ${DEPS_PREFIX}
        SOURCE_SUBDIR build/cmake
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX}
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_BUILD_TYPE=Release
            -DZSTD_BUILD_SHARED=ON
            -DZSTD_BUILD_STATIC=OFF
            -DZSTD_BUILD_PROGRAMS=OFF
            -DZSTD_BUILD_TESTS=OFF
            ${ODPS_DEP_TOOLCHAIN_ARGS}
    )
    list(APPEND ODPS_EXT_TARGETS zstd_ext)
endif()
