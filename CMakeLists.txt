cmake_minimum_required(VERSION 3.22)
project(aliyun-odps-sdk-cpp VERSION 0.50 LANGUAGES C CXX)

# ---- options ----
option(WITH_ARROW "Build with Apache Arrow" OFF)
option(WITH_PERF_TOOL "Build the perf_tool executable" OFF)
option(WITH_RANDOM_UUID "Use random generated UUID instead of boost provided" OFF)
option(ODPS_WERROR "Enable -Werror" ON)
option(ODPS_BUILD_TESTS "Build tests" ON)
option(ODPS_BUILD_EXAMPLES "Build examples" ON)
option(ODPS_FORCE_FETCH_DEPS "Force download+build all deps, ignore system" OFF)

# ---- compiler validation ----
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} is not officially supported; GCC >= 4.9.2 expected.")
endif()
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.2")
    message(FATAL_ERROR "GCC >= 4.9.2 required (found ${CMAKE_CXX_COMPILER_VERSION}). "
        "Set -DCMAKE_CXX_COMPILER=... or CXX env, e.g. /path/to/gcc-4.9.2/bin/g++")
endif()

# ---- architecture ----
execute_process(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE HOST_ARCHITECTURE)
set(ARCH_EXTRA_FLAGS "")
if(HOST_ARCHITECTURE STREQUAL "aarch64")
    set(ARCH_EXTRA_FLAGS -march=armv8-a+crc)
endif()

# ---- ABI flag ----
set(ODPS_CXX_ABI_FLAG "")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0.0")
    set(ODPS_CXX_ABI_FLAG "-D_GLIBCXX_USE_CXX11_ABI=0")
endif()

# ---- base flags ----
set(CC_FLAGS
    -g --std=gnu++14 -Wall -pthread -fPIC
    -DTUNNEL_USE_CRC_32C
    -O3 -fno-strict-aliasing
    ${ARCH_EXTRA_FLAGS}
    ${ODPS_CXX_ABI_FLAG}
)
if(ODPS_WERROR)
    list(APPEND CC_FLAGS -Werror -Wno-non-virtual-dtor -Wno-parentheses)
endif()
if(WITH_ARROW)
    list(APPEND CC_FLAGS -DODPS_SDK_ENABLE_ARROW)
endif()
if(WITH_RANDOM_UUID)
    list(APPEND CC_FLAGS -DSUBSTITUDE_UUID_WITH_RANDOM)
endif()
set(LINK_FLAGS -Wl,-z,defs)

message(STATUS "GCC ${CMAKE_CXX_COMPILER_VERSION}, arch ${HOST_ARCHITECTURE}, ABI flag '${ODPS_CXX_ABI_FLAG}'")

# ---- dependencies (find-or-fetch) ----
include(${CMAKE_SOURCE_DIR}/cmake/OdpsDeps.cmake)

# ---- version/header generation ----
execute_process(COMMAND date +%Y%m%d%H%M%S COMMAND tr -d '\n' OUTPUT_VARIABLE PACKAGE_TIMESTAMP)
add_custom_command(OUTPUT target_always_rebuild COMMAND printf '')
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/build_sdk_version
  COMMAND ${CMAKE_SOURCE_DIR}/cmake/configure_build_files.py ${CMAKE_SOURCE_DIR} ${CMAKE_CXX_COMPILER_VERSION}
  DEPENDS target_always_rebuild)
add_custom_target(build_sdk_version ALL DEPENDS ${CMAKE_SOURCE_DIR}/build_sdk_version)
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/common/sdk_version.h
  COMMAND ${CMAKE_SOURCE_DIR}/cmake/configure_build_headers.py ${CMAKE_SOURCE_DIR}/common
  DEPENDS target_always_rebuild)
add_custom_target(build_sdk_header ALL DEPENDS ${CMAKE_SOURCE_DIR}/common/sdk_version.h)

# ---- bundled sources ----
file(GLOB ODPS_UTIL_CPPSRC CONFIGURE_DEPENDS util/*.cpp)
file(GLOB_RECURSE ODPS_THIRDPARTY_BUILTIN_CPPSRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/thirdparty/common/*.cpp)

# ---- global include dirs ----
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    # nlohmann/json 等以 <nlohmann/json.hpp> 形式引用的 vendored 头文件库
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/thirdparty/common/tinyxml2
    ${CMAKE_SOURCE_DIR}/thirdparty/common/gzip_helper
)

add_definitions(${CC_FLAGS})

# ---- output dirs & rpath ----
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BUILD_RPATH "${DEPS_PREFIX}/lib;${CMAKE_BINARY_DIR}/lib")
# 安装后的 .so 在同目录查找依赖 .so(必须在模块目标创建之前设置才生效)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

# ---- modules ----
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(tunnel)
add_subdirectory(max_storage_api)
if(ODPS_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
if(ODPS_BUILD_TESTS)
    add_subdirectory(test)
endif()

# ---- packaging ----
include(${CMAKE_SOURCE_DIR}/cmake/Package.cmake)
