file(GLOB_RECURSE ODPS_CORE_CPPSRC CONFIGURE_DEPENDS *.cpp)

add_library(odps_sdk_core_object OBJECT ${ODPS_CORE_CPPSRC} ${ODPS_THIRDPARTY_BUILTIN_CPPSRC})
add_dependencies(odps_sdk_core_object build_sdk_header ${ODPS_EXT_TARGETS})

add_library(odps_sdk_core SHARED $<TARGET_OBJECTS:odps_sdk_core_object>)
target_link_libraries(odps_sdk_core curl rt odps_sdk_common ${LINK_FLAGS})

add_library(odps_sdk_core_static STATIC $<TARGET_OBJECTS:odps_sdk_core_object>)
