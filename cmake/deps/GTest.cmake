set(GTEST_DEP_VERSION 1.10.0)
set(GTEST_DEP_URL "https://github.com/google/googletest/archive/refs/tags/release-${GTEST_DEP_VERSION}.tar.gz"
    CACHE STRING "googletest source URL")
set(GTEST_DEP_SHA256
    "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb"
    CACHE STRING "googletest source sha256")

set(_hash_args "")
if(GTEST_DEP_SHA256)
    set(_hash_args URL_HASH SHA256=${GTEST_DEP_SHA256})
endif()
ExternalProject_Add(gtest_ext
    URL ${GTEST_DEP_URL}
    ${_hash_args}
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
    PREFIX ${CMAKE_BINARY_DIR}/ep/gtest
    INSTALL_DIR ${DEPS_PREFIX}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_SHARED_LIBS=ON
        -DBUILD_GMOCK=ON
        -DINSTALL_GTEST=ON
        -DCMAKE_CXX_FLAGS=${ODPS_DEP_CXX_ABI_FLAGS}
        ${ODPS_DEP_TOOLCHAIN_ARGS}
)
list(APPEND ODPS_EXT_TARGETS gtest_ext)
