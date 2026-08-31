include(ExternalProject)
include(ProcessorCount)
ProcessorCount(ODPS_NPROC)
if(ODPS_NPROC EQUAL 0)
    set(ODPS_NPROC 4)
endif()

set(DEPS_PREFIX ${CMAKE_BINARY_DIR}/deps_install)
set(DEPS_DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/_deps_src)
file(MAKE_DIRECTORY ${DEPS_PREFIX}/include)
file(MAKE_DIRECTORY ${DEPS_PREFIX}/lib)

set(ODPS_EXT_TARGETS "")

# ABI 一致性:注入到每个源码编译的 C++ 依赖
set(ODPS_DEP_CXX_ABI_FLAGS "")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0.0")
    set(ODPS_DEP_CXX_ABI_FLAGS "-D_GLIBCXX_USE_CXX11_ABI=0")
endif()

# 编译器一致性:ExternalProject 子项目各自独立 configure,不会继承顶层编译器,
# 因此显式传入 SDK 自己的编译器。保证源码编译的依赖与 SDK 的 ABI/libstdc++ 运行时
# 始终一致——GCC < 5 时 ODPS_DEP_CXX_ABI_FLAGS 为空,若不固定编译器,宿主默认编译器
# 可能产出新 ABI 符号,导致链接失败/运行时 ABI 混用。(zlib/openssl 走自带 configure
# 脚本且为纯 C,无 C++ ABI 暴露,不在此列。)
set(ODPS_DEP_TOOLCHAIN_ARGS
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})

# 逐个依赖处理(顺序即依赖顺序:openssl/zlib 先于 curl)
include(${CMAKE_CURRENT_LIST_DIR}/deps/ZLIB.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/OpenSSL.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/Zstd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/LZ4.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/Curl.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/Protobuf.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/Boost.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/GTest.cmake)
if(WITH_ARROW)
    include(${CMAKE_CURRENT_LIST_DIR}/deps/Arrow.cmake)
endif()

include_directories(${DEPS_PREFIX}/include)
link_directories(${DEPS_PREFIX}/lib)
message(STATUS "ODPS deps prefix: ${DEPS_PREFIX}")
message(STATUS "ODPS external dep targets: ${ODPS_EXT_TARGETS}")
