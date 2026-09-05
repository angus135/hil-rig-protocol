add_executable(hil_rig_protocol_application_tests
    test_application_api.cpp
    test_application_workflow.cpp
    test_application_codec.cpp
    test_application_fixed_io.cpp
    test_application_test_configuration.cpp
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
set_tests_properties(hil_rig_protocol_application_tests PROPERTIES LABELS "unit")

add_executable(hil_rig_protocol_application_public_c_compile_test
    test_application_test_configuration_public_c.c
)
target_compile_features(hil_rig_protocol_application_public_c_compile_test PRIVATE c_std_11)
target_link_libraries(hil_rig_protocol_application_public_c_compile_test
    PRIVATE hil_rig_protocol::hil_rig_protocol
)
if(COMMAND hil_rig_protocol_apply_warnings)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_application_public_c_compile_test)
endif()
if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_application_public_c_compile_test)
endif()
if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_application_public_c_compile_test)
endif()
add_test(
    NAME hil_rig_protocol_application_public_c_compile_test
    COMMAND hil_rig_protocol_application_public_c_compile_test
)
set_tests_properties(hil_rig_protocol_application_public_c_compile_test PROPERTIES LABELS "c-api")
