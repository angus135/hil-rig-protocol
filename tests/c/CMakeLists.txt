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
    unit/test_version.cpp
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
    integration/pipeline/test_transport_session.cpp
    integration/pipeline/test_transport_application.cpp
    integration/pipeline/test_transport_public_ownership.cpp
    integration/support/test_transport_harness.cpp
    integration/reliability/test_transport_handshake_corruption.cpp
    integration/reliability/test_transport_handshake_duplicates.cpp
    integration/reliability/test_transport_handshake_exhaustion.cpp
    integration/reliability/test_transport_reliability.cpp
    integration/reliability/test_transport_retry_timing.cpp
    integration/stream/test_transport_stream.cpp
    integration/malformed_frames/test_transport_malformed_frames.cpp
    integration/capacity/test_transport_capacity.cpp
    integration/capacity/test_transport_handshake_capacity.cpp
    integration/configuration/test_transport_reduced_configuration.cpp
    integration/wire/test_transport_golden_vectors.cpp
    integration/lifecycle/test_transport_link_lifecycle.cpp
    integration/lifecycle/test_transport_reset.cpp
    integration/recovery/test_transport_stale_traffic.cpp
    integration/recovery/test_transport_recovery.cpp
)

# Keep the scenario list explicit, while making it impossible for a newly added
# integration test source to remain silently uncompiled.
file(GLOB_RECURSE HIL_RIG_PROTOCOL_DISCOVERED_INTEGRATION_TEST_SOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "integration/test_*.cpp"
)
set(HIL_RIG_PROTOCOL_INTENTIONALLY_EXCLUDED_INTEGRATION_TEST_SOURCES)
set(HIL_RIG_PROTOCOL_REGISTERED_INTEGRATION_TEST_SOURCES
    ${HIL_RIG_PROTOCOL_INTEGRATION_TEST_SOURCES}
    ${HIL_RIG_PROTOCOL_INTENTIONALLY_EXCLUDED_INTEGRATION_TEST_SOURCES}
)
foreach(integration_test_source IN LISTS HIL_RIG_PROTOCOL_DISCOVERED_INTEGRATION_TEST_SOURCES)
    if(NOT integration_test_source IN_LIST HIL_RIG_PROTOCOL_REGISTERED_INTEGRATION_TEST_SOURCES)
        message(FATAL_ERROR
            "Unregistered integration test source: ${integration_test_source}. "
            "Add it to HIL_RIG_PROTOCOL_INTEGRATION_TEST_SOURCES or the intentional exclusion list."
        )
    endif()
endforeach()

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
target_compile_definitions(hil_rig_protocol_unit_tests
    PRIVATE
        HIL_RIG_PROTOCOL_TEST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        HIL_RIG_PROTOCOL_TEST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        HIL_RIG_PROTOCOL_TEST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        HIL_RIG_PROTOCOL_TEST_VERSION_STRING="${PROJECT_VERSION}"
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

include(GoogleTest)

gtest_discover_tests(hil_rig_protocol_unit_tests
    TEST_PREFIX "unit::"
    PROPERTIES LABELS "unit"
)

gtest_discover_tests(hil_rig_protocol_integration_tests
    TEST_PREFIX "integration::"
    PROPERTIES LABELS "integration"
)

add_test(
    NAME hil_rig_protocol_c_api_compile_test
    COMMAND hil_rig_protocol_c_api_compile_test
)
set_tests_properties(hil_rig_protocol_c_api_compile_test PROPERTIES LABELS "c-api")

add_subdirectory(application)
