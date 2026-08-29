cmake_minimum_required(VERSION 3.16)

project(hil_rig_protocol_add_subdirectory_consumer LANGUAGES C)

get_filename_component(HIL_RIG_PROTOCOL_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(HIL_RIG_PROTOCOL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HIL_RIG_PROTOCOL_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(HIL_RIG_PROTOCOL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HIL_RIG_PROTOCOL_ENABLE_WARNINGS ON CACHE BOOL "" FORCE)
set(HIL_RIG_PROTOCOL_WARNINGS_AS_ERRORS ON CACHE BOOL "" FORCE)
add_subdirectory("${HIL_RIG_PROTOCOL_SOURCE_DIR}" hil_rig_protocol)

add_executable(hil_rig_protocol_external_consumer main.c)
target_compile_features(hil_rig_protocol_external_consumer PRIVATE c_std_11)
target_link_libraries(hil_rig_protocol_external_consumer
    PRIVATE
        hil_rig_protocol::hil_rig_protocol
)
