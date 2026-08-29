add_library(hil_rig_protocol_python_adapter STATIC
    hil_rig_protocol_ffi.c
)

target_compile_features(hil_rig_protocol_python_adapter PRIVATE c_std_11)
target_link_libraries(hil_rig_protocol_python_adapter
    PUBLIC
        hil_rig_protocol::hil_rig_protocol
)
target_include_directories(hil_rig_protocol_python_adapter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(COMMAND hil_rig_protocol_apply_warnings)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_python_adapter)
endif()

if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_python_adapter)
endif()

if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_python_adapter)
endif()

if(HIL_RIG_PROTOCOL_BUILD_TESTS)
    add_executable(hil_rig_protocol_python_adapter_tests
        tests/test_hil_rig_protocol_ffi.c
    )
    target_compile_features(hil_rig_protocol_python_adapter_tests PRIVATE c_std_11)
    target_link_libraries(hil_rig_protocol_python_adapter_tests
        PRIVATE
            hil_rig_protocol_python_adapter
    )

    if(COMMAND hil_rig_protocol_apply_warnings)
        hil_rig_protocol_apply_warnings(hil_rig_protocol_python_adapter_tests)
    endif()

    if(COMMAND hil_rig_protocol_apply_sanitizers)
        hil_rig_protocol_apply_sanitizers(hil_rig_protocol_python_adapter_tests)
    endif()

    if(COMMAND hil_rig_protocol_apply_coverage)
        hil_rig_protocol_apply_coverage(hil_rig_protocol_python_adapter_tests)
    endif()

    add_test(
        NAME hil_rig_protocol_python_adapter_tests
        COMMAND hil_rig_protocol_python_adapter_tests
    )
    set_tests_properties(hil_rig_protocol_python_adapter_tests
        PROPERTIES LABELS "python-adapter"
    )
endif()
