set(PROTOBUF_DEP_VERSION 3.7.1)
# protocolbuffers/protobuf tag archive (github release-asset downloads are
# blocked in this env). The tag archive is the full repo, so the CMake project
# lives in the cmake/ subdirectory -> SOURCE_SUBDIR cmake below.
set(PROTOBUF_DEP_URL "https://github.com/protocolbuffers/protobuf/archive/refs/tags/v${PROTOBUF_DEP_VERSION}.tar.gz"
    CACHE STRING "protobuf source URL")
set(PROTOBUF_DEP_SHA256
    "f1748989842b46fa208b2a6e4e2785133cfcc3e4d43c17fecb023733f0f5443f"
    CACHE STRING "protobuf source sha256")

set(_hash_args "")
if(PROTOBUF_DEP_SHA256)
    set(_hash_args URL_HASH SHA256=${PROTOBUF_DEP_SHA256})
endif()
set(_protobuf_ext_deps "")
if(ZLIB_FETCHED)
    list(APPEND _protobuf_ext_deps zlib_ext)
endif()
# protobuf's gzip_stream.cc is compiled only when zlib is found (HAVE_ZLIB);
# the SDK requires those Gzip* symbols. CMAKE_PREFIX_PATH points at our deps
# prefix so find_package(ZLIB) picks up the lib built above (same pattern as
# curl_ext); DEPENDS orders protobuf after zlib_ext under parallel make.
ExternalProject_Add(protobuf_ext
    URL ${PROTOBUF_DEP_URL}
    ${_hash_args}
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
    PREFIX ${CMAKE_BINARY_DIR}/ep/protobuf
    INSTALL_DIR ${DEPS_PREFIX}
    DEPENDS ${_protobuf_ext_deps}
    SOURCE_SUBDIR cmake
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${DEPS_PREFIX}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_PREFIX_PATH=${DEPS_PREFIX}
        -DCMAKE_BUILD_TYPE=Release
        -Dprotobuf_BUILD_SHARED_LIBS=ON
        -Dprotobuf_BUILD_TESTS=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_CXX_FLAGS=${ODPS_DEP_CXX_ABI_FLAGS}
        ${ODPS_DEP_TOOLCHAIN_ARGS}
)
list(APPEND ODPS_EXT_TARGETS protobuf_ext)
