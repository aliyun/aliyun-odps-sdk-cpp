file(GLOB_RECURSE ODPS_TESTING_CPPSRC CONFIGURE_DEPENDS *.cpp)

if(NOT WITH_ARROW)
    # arrow 依赖的测试源文件(直接 include <arrow/api.h>),WITH_ARROW=OFF 时
    # 无法编译,从 all_testing 中排除。其余 arrow 相关测试(arrow_test/
    # eos_test/decimal_test)源码内部已有 #ifdef ODPS_SDK_ENABLE_ARROW 保护。
    list(FILTER ODPS_TESTING_CPPSRC EXCLUDE REGEX
        "max_storage_api/(instance_test|preview_test|table_read_test|table_write_test)\\.cpp$")
endif()

add_executable(all_testing ${ODPS_TESTING_CPPSRC})
add_dependencies(all_testing ${ODPS_EXT_TARGETS})
target_link_libraries(all_testing
    max_storage_api_static
    odps_sdk_tunnel_static
    odps_sdk_core_static
    odps_sdk_common_static
    rt protobuf curl ssl dl crypto gmock gtest pthread z zstd lz4
    ${LINK_FLAGS})

if(WITH_ARROW)
    target_link_libraries(all_testing arrow)
endif()
