file(GLOB MAX_STORAGE_API_CPPSRC CONFIGURE_DEPENDS *.cpp)

add_library(max_storage_api_object OBJECT ${MAX_STORAGE_API_CPPSRC})
add_dependencies(max_storage_api_object build_sdk_header ${ODPS_EXT_TARGETS})

add_library(max_storage_api_static STATIC $<TARGET_OBJECTS:max_storage_api_object>)
set_target_properties(max_storage_api_static PROPERTIES OUTPUT_NAME max_storage_api)

if(WITH_PERF_TOOL)
    add_subdirectory(tool)
endif()

add_library(max_storage_api SHARED $<TARGET_OBJECTS:max_storage_api_object>)
target_link_libraries(max_storage_api odps_sdk_tunnel odps_sdk_core odps_sdk_common)
target_link_libraries(max_storage_api_static odps_sdk_tunnel_static odps_sdk_core_static odps_sdk_common_static)
