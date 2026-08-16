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
    unit/test_transport.cpp
    unit/test_transport_application.cpp
    unit/test_transport_control_output.cpp
    unit/test_transport_events.cpp
    unit/test_transport_handshake.cpp
    unit/test_transport_output.cpp
    unit/test_transport_reliability.cpp
    unit/test_transport_receive.cpp
    unit/test_transport_session.cpp
    unit/test_transport_wire.cpp
)

set(HIL_RIG_PROTOCOL_INTEGRATION_SUPPORT_SOURCES
    integration/support/transport_pair_harness.cpp
    integration/support/transport_test_endpoint.cpp
    integration/support/transport_test_link.cpp
)

# Integration scenarios are grouped by caller-visible behaviour. Keep support
# implementation separate so future error/capacity suites can reuse the same
# public-API-only endpoint, byte-link, and deterministic pair harness.
set(HIL_RIG_PROTOCOL_INTEGRATION_TEST_SOURCES
    integration/test_transport_facade.cpp
    integration/pipeline/test_transport_pipeline.cpp
    integration/support/test_transport_harness.cpp
    integration/recovery/test_transport_recovery.cpp
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
target_include_directories(hil_rig_protocol_unit_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

add_executable(hil_rig_protocol_integration_tests
    ${HIL_RIG_PROTOCOL_INTEGRATION_SUPPORT_SOURCES}
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
# Integration support intentionally sees only public headers plus its own support
# directory. Unlike unit tests, it must not include src/transport/internal/.
target_include_directories(hil_rig_protocol_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/integration
)

add_executable(hil_rig_protocol_c_api_compile_test
    integration/test_transport_public_c.c
)
target_compile_features(hil_rig_protocol_c_api_compile_test PRIVATE c_std_11)
target_link_libraries(hil_rig_protocol_c_api_compile_test
    PRIVATE
        hil_rig_protocol::hil_rig_protocol
)

if(COMMAND hil_rig_protocol_apply_warnings)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_unit_tests)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_integration_tests)
    hil_rig_protocol_apply_warnings(hil_rig_protocol_c_api_compile_test)
endif()

if(COMMAND hil_rig_protocol_apply_sanitizers)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_unit_tests)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_integration_tests)
    hil_rig_protocol_apply_sanitizers(hil_rig_protocol_c_api_compile_test)
endif()

if(COMMAND hil_rig_protocol_apply_coverage)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_unit_tests)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_integration_tests)
    hil_rig_protocol_apply_coverage(hil_rig_protocol_c_api_compile_test)
endif()

add_test(
    NAME hil_rig_protocol_unit_tests
    COMMAND hil_rig_protocol_unit_tests
)

add_test(
    NAME hil_rig_protocol_integration_tests
    COMMAND hil_rig_protocol_integration_tests
)

add_test(
    NAME hil_rig_protocol_c_api_compile_test
    COMMAND hil_rig_protocol_c_api_compile_test
)

add_subdirectory(application)
