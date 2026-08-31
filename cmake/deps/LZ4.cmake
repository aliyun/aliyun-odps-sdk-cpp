set(LZ4_DEP_VERSION 1.9.3)
set(LZ4_DEP_URL "https://github.com/lz4/lz4/archive/refs/tags/v${LZ4_DEP_VERSION}.tar.gz"
    CACHE STRING "lz4 source URL")
set(LZ4_DEP_SHA256
    "030644df4611007ff7dc962d981f390361e6c97a34e5cbc393ddfbe019ffe2c1"
    CACHE STRING "lz4 source sha256")

set(LZ4_FETCHED FALSE)
if(NOT ODPS_FORCE_FETCH_DEPS AND NOT FORCE_FETCH_LZ4)
    find_library(LZ4_SYS_LIB lz4)
    find_path(LZ4_SYS_INC lz4.h)
endif()

if(LZ4_SYS_LIB AND LZ4_SYS_INC)
    message(STATUS "Using system lz4: ${LZ4_SYS_LIB}")
else()
    set(LZ4_FETCHED TRUE)
    set(_hash_args "")
    if(LZ4_DEP_SHA256)
        set(_hash_args URL_HASH SHA256=${LZ4_DEP_SHA256})
    endif()
    ExternalProject_Add(lz4_ext
        URL ${LZ4_DEP_URL}
        ${_hash_args}
        DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
        PREFIX ${CMAKE_BINARY_DIR}/ep/lz4
        INSTALL_DIR ${DEPS_PREFIX}
        SOURCE_SUBDIR build/cmake
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX}
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_BUILD_TYPE=Release
            -DBUILD_SHARED_LIBS=ON
            -DBUILD_STATIC_LIBS=OFF
            ${ODPS_DEP_TOOLCHAIN_ARGS}
    )
    list(APPEND ODPS_EXT_TARGETS lz4_ext)
endif()
