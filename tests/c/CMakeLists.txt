enable_language(CXX)

include(FetchContent)

if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gmock OR NOT TARGET GTest::gtest_main)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

set(HIL_RIG_PROTOCOL_UNIT_TEST_SOURCES
    unit/test_placeholder.cpp
)

set(HIL_RIG_PROTOCOL_INTEGRATION_TEST_SOURCES
    integration/test_placeholder.cpp
)

add_executable(hil_rig_protocol_unit_tests
    ${HIL_RIG_PROTOCOL_UNIT_TEST_SOURCES}
)
target_compile_features(hil_rig_protocol_unit_tests PRIVATE cxx_std_17)
target_link_libraries(hil_rig_protocol_unit_tests
    PRIVATE
        hil_rig_protocol::hil_rig_protocol
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
)

add_executable(hil_rig_protocol_integration_tests
    ${HIL_RIG_PROTOCOL_INTEGRATION_TEST_SOURCES}
)
target_compile_features(hil_rig_protocol_integration_tests PRIVATE cxx_std_17)
target_link_libraries(hil_rig_protocol_integration_tests
    PRIVATE
        hil_rig_protocol::hil_rig_protocol
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
)

if(COMMAND hil_rig_protocol_apply_warnings)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_unit_tests)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_integration_tests)
endif()

if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_unit_tests)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_integration_tests)
endif()

if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_unit_tests)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_integration_tests)
endif()

add_test(
    NAME hil_rig_protocol_unit_tests
    COMMAND hil_rig_protocol_unit_tests
)

add_test(
    NAME hil_rig_protocol_integration_tests
    COMMAND hil_rig_protocol_integration_tests
)
