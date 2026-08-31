set(ARROW_DEP_VERSION 1.0.0)
set(ARROW_DEP_URL "https://archive.apache.org/dist/arrow/arrow-${ARROW_DEP_VERSION}/apache-arrow-${ARROW_DEP_VERSION}.tar.gz"
    CACHE STRING "arrow source URL")
set(ARROW_DEP_SHA256 "" CACHE STRING "arrow source sha256")

set(_hash_args "")
if(ARROW_DEP_SHA256)
    set(_hash_args URL_HASH SHA256=${ARROW_DEP_SHA256})
endif()
ExternalProject_Add(arrow_ext
    URL ${ARROW_DEP_URL}
    ${_hash_args}
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
    PREFIX ${CMAKE_BINARY_DIR}/ep/arrow
    INSTALL_DIR ${DEPS_PREFIX}
    SOURCE_SUBDIR cpp
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX}
        -DCMAKE_BUILD_TYPE=Release
        -DARROW_BUILD_SHARED=ON
        -DARROW_BUILD_STATIC=OFF
        -DARROW_IPC=ON
        -DARROW_COMPUTE=ON
        -DARROW_JEMALLOC=OFF
        -DARROW_MIMALLOC=OFF
        -DARROW_WITH_SNAPPY=OFF
        -DARROW_BUILD_TESTS=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_CXX_FLAGS=${ODPS_DEP_CXX_ABI_FLAGS}
        ${ODPS_DEP_TOOLCHAIN_ARGS}
)
list(APPEND ODPS_EXT_TARGETS arrow_ext)
