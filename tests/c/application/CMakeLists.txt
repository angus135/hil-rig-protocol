add_executable(hil_rig_protocol_application_tests
    test_application_api.cpp
    test_application_workflow.cpp
)

target_compile_features(hil_rig_protocol_application_tests PRIVATE cxx_std_17)

target_link_libraries(hil_rig_protocol_application_tests
    PRIVATE
        hil_rig_protocol::hil_rig_protocol
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
)

if(COMMAND hil_rig_protocol_apply_warnings)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_application_tests)
endif()

if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_application_tests)
endif()

if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_application_tests)
endif()

add_test(
    NAME hil_rig_protocol_application_tests
    COMMAND hil_rig_protocol_application_tests
)
