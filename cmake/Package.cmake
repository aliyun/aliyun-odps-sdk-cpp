set(PACKAGE_NAME aliyun_odps_sdk_cpp_${PACKAGE_TIMESTAMP}_release)
set(PACKAGE_TEMP_DIR ${CMAKE_SOURCE_DIR}/package/temp/${PACKAGE_NAME}/aliyun_odps_sdk_cpp)
file(MAKE_DIRECTORY ${PACKAGE_TEMP_DIR})
message(STATUS "Packaging Dir is ${PACKAGE_TEMP_DIR}")

# SDK 库(动态+静态)
install(TARGETS
    odps_sdk_common odps_sdk_common_static
    odps_sdk_core odps_sdk_core_static
    odps_sdk_tunnel odps_sdk_tunnel_static
    max_storage_api max_storage_api_static
    DESTINATION ${PACKAGE_TEMP_DIR}/lib)

# 依赖 .so(从 deps 前缀一并带上)
install(DIRECTORY ${DEPS_PREFIX}/lib/ DESTINATION ${PACKAGE_TEMP_DIR}/lib
        FILES_MATCHING PATTERN "*.so*" )

# 公共头文件
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include DESTINATION ${PACKAGE_TEMP_DIR})

# 构建信息
install(FILES ${CMAKE_SOURCE_DIR}/build_sdk_version DESTINATION ${PACKAGE_TEMP_DIR} RENAME BUILD_INFO)

# 许可证与第三方声明(发布包附带依赖 .so,需随附许可文本)
install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${CMAKE_SOURCE_DIR}/NOTICE
    ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md
    DESTINATION ${PACKAGE_TEMP_DIR})

install(CODE "execute_process(COMMAND tar zcvf ${PACKAGE_NAME}.tar.gz -C temp/${PACKAGE_NAME} aliyun_odps_sdk_cpp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/package)")
