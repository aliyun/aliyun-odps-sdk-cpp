file(GLOB ODPS_COMMON_CPPSRC CONFIGURE_DEPENDS *.cpp)

add_library(odps_sdk_common_object OBJECT ${ODPS_COMMON_CPPSRC} ${ODPS_UTIL_CPPSRC})
add_dependencies(odps_sdk_common_object build_sdk_header ${ODPS_EXT_TARGETS})

add_library(odps_sdk_common SHARED $<TARGET_OBJECTS:odps_sdk_common_object>)
target_link_libraries(odps_sdk_common protobuf curl ssl crypto z zstd pthread ${LINK_FLAGS})

add_library(odps_sdk_common_static STATIC $<TARGET_OBJECTS:odps_sdk_common_object>)
