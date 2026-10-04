/** @file test_application_run_report.cpp
 * @brief Report terminal relationships, explicit wire widths and extension ownership.
 */
#include <array>
#include <algorithm>
#include <gtest/gtest.h>
#include "hil_rig_protocol/application/application.h"

namespace {

class ApplicationRunReport : public ::testing::Test
{
protected:
    HIL_Application_Context_T context{};
    HIL_Application_Message_T message{};

    void SetUp() override
    {
        HIL_Application_Config_T config{};
        ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
        message.type                = HIL_APPLICATION_MESSAGE_TYPE_RUN_REPORT;
        message.has_test_id         = 1u;
        auto& report                = message.body.run_report;
        report.schema_version       = 1u;
        report.run_outcome          = HIL_APPLICATION_RUN_OUTCOME_SUCCESS;
        report.execution_outcome    = HIL_APPLICATION_EXECUTION_OUTCOME_COMPLETE;
        report.result_status        = HIL_APPLICATION_RUN_RESULT_STATUS_COMPLETE;
        report.valid_sections       = HIL_APPLICATION_RUN_REPORT_VALID_TERMINAL;
        report.expected_tick_count  = 100u;
        report.tick_period_us       = 1000u;
        report.result_ticks_emitted = 100u;
    }
};

/** A nonempty extension validates without workspace and decodes into a detached copy. */
TEST_F( ApplicationRunReport, FullWidthTotalsAndExtensionStorage )
{
    std::array<uint8_t, 255> extension{};
    extension.fill( 0xa5 );
    auto& report                           = message.body.run_report;
    report.valid_sections                  = 0x3fu;
    report.last_completed_boundary         = 100u;
    report.isr_timing                      = { 1u, UINT64_C( 0xfedcba9876543210 ), 0u, 0u, 100u };
    report.instruction_buffer              = { 1u, 0u, 0u };
    report.flash.result_pages_drained      = 1u;
    report.flash.result_bytes_drained      = UINT64_MAX;
    report.flash.result_drain_total_cycles = UINT64_C( 0x8000000000000000 );
    report.extension_data                  = { extension.data(), 255u };
    std::array<uint8_t, 455> wire{};
    size_t                   size = 0u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, wire.size() );
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( wire[19], 35u );
    EXPECT_EQ( wire[21], 0xb0u );
    EXPECT_EQ( wire[22], 1u );
    EXPECT_EQ( wire[23 + 7], 0u );
    EXPECT_EQ( wire[23 + 10], 0u );
    EXPECT_EQ( wire[23 + 11], 0u );
    const std::array<uint8_t, 8> total = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
    EXPECT_TRUE( std::equal( total.begin(), total.end(), wire.begin() + 23 + 36 ) );
    EXPECT_EQ( wire[23 + 176], 255u );
    size_t required = 0u;
    ASSERT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, extension.size() );
    HIL_Application_Message_T decoded{};
    std::array<uint8_t, 255>  storage{};
    size_t                    used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.data(), storage.size() - 1, &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.run_report.extension_data.data, storage.data() );
    EXPECT_EQ( decoded.body.run_report.isr_timing.total_cycles, report.isr_timing.total_cycles );
    EXPECT_EQ( decoded.body.run_report.flash.result_bytes_drained, UINT64_MAX );
    wire[200] = 0u;
    EXPECT_EQ( storage[0], 0xa5 );
    report.extension_data.size = 256u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
}

/** Every shorter complete input is truncated; internal length/reserved/schema errors stay distinct.
 */
TEST_F( ApplicationRunReport, MalformedBodiesNeverPublishMessage )
{
    std::array<uint8_t, 201> wire{};
    size_t                   size = 0u;
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( size, 200u );
    for ( size_t length = 0; length < size; ++length )
    {
        HIL_Application_Message_T decoded{};
        size_t                    used = 99u;
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), length, &decoded, nullptr,
                                                   0u, &used ),
                   HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE );
        EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
        EXPECT_EQ( used, 0u );
    }
    size_t required = 0u;
    for ( const auto offset : { 30u, 33u, 34u, 35u } )
    {
        auto invalid    = wire;
        invalid[offset] = 0xff;
        EXPECT_EQ(
            HIL_APPLICATION_Validate_Encoded_Message( &context, invalid.data(), size, &required ),
            HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( required, 0u );
    }
    wire[199] = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), size, &required ),
               HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    wire[199] = 0u;
    wire[23]  = 2u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), size, &required ),
               HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE );
    message.has_test_id = 0u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );
}

/** A pre-execution failure and partial or unavailable streams are valid terminal reports. */
TEST_F( ApplicationRunReport, TerminalAndSamplingRelationships )
{
    auto& report = message.body.run_report;
    report.valid_sections |= HIL_APPLICATION_RUN_REPORT_VALID_LAST_COMPLETED_BOUNDARY;
    report.last_completed_boundary = 99u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    report.last_completed_boundary = 100u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
    report.run_outcome             = HIL_APPLICATION_RUN_OUTCOME_FAILED;
    report.execution_outcome       = HIL_APPLICATION_EXECUTION_OUTCOME_NOT_STARTED;
    report.result_status           = HIL_APPLICATION_RUN_RESULT_STATUS_UNAVAILABLE;
    report.result_ticks_emitted    = 0u;
    report.valid_sections          = HIL_APPLICATION_RUN_REPORT_VALID_TERMINAL;
    report.last_completed_boundary = 0u;
    report.failure_source          = HIL_APPLICATION_FAILURE_SOURCE_DRIVER_LIFECYCLE;
    report.failure_stage           = HIL_APPLICATION_FAILURE_STAGE_PREPARATION;
    report.failure_reason          = HIL_APPLICATION_FAILURE_REASON_DRIVER_START_FAILED;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
    report.valid_sections |= HIL_APPLICATION_RUN_REPORT_VALID_ISR_TIMING;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    report.execution_outcome       = HIL_APPLICATION_EXECUTION_OUTCOME_FAILED;
    report.isr_timing.total_cycles = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    report.isr_timing.sample_count     = 1u;
    report.isr_timing.maximum_boundary = 0u;
    report.result_status               = HIL_APPLICATION_RUN_RESULT_STATUS_PARTIAL;
    report.result_ticks_emitted        = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
    report.result_ticks_emitted = 100u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    report.result_status = HIL_APPLICATION_RUN_RESULT_STATUS_UNAVAILABLE;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
}

}  // namespace
