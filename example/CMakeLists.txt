file(GLOB_RECURSE ALL_EXAMPLE_CPPSRC CONFIGURE_DEPENDS *.cpp)

if(NOT WITH_ARROW)
    # arrow 依赖的示例(直接 include <arrow/api.h> 或以 arrow 命名),
    # WITH_ARROW=OFF 时无法编译,排除。
    list(FILTER ALL_EXAMPLE_CPPSRC EXCLUDE REGEX
        "(arrow|instance_direct_read|instance_read|table_preview|table_read|table_write)\\.cpp$")
endif()

set(EXAMPLE_LINK_LIBS rt protobuf max_storage_api
    odps_sdk_tunnel_static odps_sdk_core_static odps_sdk_common_static
    curl ssl dl crypto z pthread zstd lz4)

if(WITH_ARROW)
    set(EXAMPLE_LINK_LIBS ${EXAMPLE_LINK_LIBS} arrow)
endif()

foreach(EACH_EXAMPLE_SRC ${ALL_EXAMPLE_CPPSRC})
    string(FIND ${EACH_EXAMPLE_SRC} arrow CONTAINS_ARROW)
    if(WITH_ARROW OR (CONTAINS_ARROW EQUAL -1))
        get_filename_component(EXEC_NAME ${EACH_EXAMPLE_SRC} NAME_WE)
        add_executable(${EXEC_NAME} ${EACH_EXAMPLE_SRC})
        add_dependencies(${EXEC_NAME} ${ODPS_EXT_TARGETS})
        target_link_libraries(${EXEC_NAME} ${EXAMPLE_LINK_LIBS})
    endif()
endforeach()
