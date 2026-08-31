set(BOOST_DEP_VERSION 1.75.0)
set(BOOST_DEP_VERSION_US 1_75_0)
# boostorg.jfrog.io has been shut down; use the official boost archive.
set(BOOST_DEP_URL "https://archives.boost.io/release/${BOOST_DEP_VERSION}/source/boost_${BOOST_DEP_VERSION_US}.tar.gz"
    CACHE STRING "boost source URL")
# 留空:官方包的哈希无法在本机核验。本机构建通过预置 _deps_src 中的
# boost_1_75_0.tar.gz 并以 -DBOOST_DEP_SHA256=<预置包哈希> 配置来跳过下载。
set(BOOST_DEP_SHA256 "" CACHE STRING "boost source sha256")

set(_hash_args "")
if(BOOST_DEP_SHA256)
    set(_hash_args URL_HASH SHA256=${BOOST_DEP_SHA256})
endif()

if(WITH_PERF_TOOL)
    # 需要编译 program_options 库
    set(_boost_build ./b2 --with-program_options link=shared variant=release
        cxxflags=${ODPS_DEP_CXX_ABI_FLAGS} stage)
    set(_boost_install ${CMAKE_COMMAND} -E make_directory ${DEPS_PREFIX}/lib &&
        ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/stage/lib ${DEPS_PREFIX}/lib &&
        ${CMAKE_COMMAND} -E make_directory ${DEPS_PREFIX}/include &&
        ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/boost ${DEPS_PREFIX}/include/boost)
    set(_boost_configure ./bootstrap.sh)
else()
    # 仅头文件。ExternalProject 不接受空字符串的 CONFIGURE/BUILD 命令,
    # 用 `cmake -E true` 占位跳过这两步。
    set(_boost_configure ${CMAKE_COMMAND} -E true)
    set(_boost_build ${CMAKE_COMMAND} -E true)
    set(_boost_install ${CMAKE_COMMAND} -E make_directory ${DEPS_PREFIX}/include &&
        ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/boost ${DEPS_PREFIX}/include/boost)
endif()

ExternalProject_Add(boost_ext
    URL ${BOOST_DEP_URL}
    ${_hash_args}
    DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
    PREFIX ${CMAKE_BINARY_DIR}/ep/boost
    INSTALL_DIR ${DEPS_PREFIX}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${_boost_configure}
    BUILD_COMMAND ${_boost_build}
    INSTALL_COMMAND ${_boost_install}
)
list(APPEND ODPS_EXT_TARGETS boost_ext)
