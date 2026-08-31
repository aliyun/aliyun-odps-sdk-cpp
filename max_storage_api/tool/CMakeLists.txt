if(NOT WITH_PERF_TOOL)
    return()
endif()

file(GLOB_RECURSE ALL_TOOL_CPPSRC CONFIGURE_DEPENDS *.cpp)
list(APPEND ALL_TOOL_CPPSRC ${CMAKE_SOURCE_DIR}/test/common/test_util.cpp)

if(WITH_ARROW)
    set(TOOL_LINK_LIBS_ARROW arrow)
endif()

set(TOOL_LINK_LIBS rt odps_sdk_tunnel_static odps_sdk_core_static odps_sdk_common_static
    max_storage_api_static ${TOOL_LINK_LIBS_ARROW}
    protobuf curl ssl crypto z pthread zstd lz4 boost_program_options)

add_executable(tool ${ALL_TOOL_CPPSRC})
target_link_libraries(tool ${TOOL_LINK_LIBS})
