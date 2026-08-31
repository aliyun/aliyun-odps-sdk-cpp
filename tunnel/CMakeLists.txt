file(GLOB ODPS_TUNNEL_CPPSRC CONFIGURE_DEPENDS *.cpp)

add_library(odps_sdk_tunnel_object OBJECT ${ODPS_TUNNEL_CPPSRC})
add_dependencies(odps_sdk_tunnel_object build_sdk_header ${ODPS_EXT_TARGETS})

add_library(odps_sdk_tunnel SHARED $<TARGET_OBJECTS:odps_sdk_tunnel_object>)
target_link_libraries(odps_sdk_tunnel odps_sdk_core odps_sdk_common curl dl ssl rt protobuf crypto lz4 z zstd ${LINK_FLAGS})
if(WITH_ARROW)
    target_link_libraries(odps_sdk_tunnel arrow)
endif()

add_library(odps_sdk_tunnel_static STATIC $<TARGET_OBJECTS:odps_sdk_tunnel_object>)
