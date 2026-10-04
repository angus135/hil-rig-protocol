/** @file test_application_rig_status.cpp
 * @brief Public status/query wire, readiness, and failed-decode contracts.
 */
#include <array>
#include <gtest/gtest.h>
#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {

class ApplicationRigStatus : public ::testing::Test
{
protected:
    HIL_Application_Context_T context{};
    HIL_Application_Message_T message{};
    std::array<uint8_t, 35>   wire{};

    void SetUp() override
    {
        HIL_Application_Config_T config{};
        ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
        message.type                           = HIL_APPLICATION_MESSAGE_TYPE_RIG_STATUS;
        message.body.rig_status.schema_version = 1u;
        message.body.rig_status.origin         = HIL_APPLICATION_STATUS_ORIGIN_QUERY_RESPONSE;
        message.body.rig_status.state          = HIL_APPLICATION_RIG_STATE_IDLE;
        message.body.rig_status.flags          = HIL_APPLICATION_RIG_STATUS_READY_FOR_NEW_TEST
                                        | HIL_APPLICATION_RIG_STATUS_RESET_PERMITTED;
    }
};

/** The reference layout needs no workspace and preserves optional active IDs. */
TEST_F( ApplicationRigStatus, GoldenBytesAndOptionalTestId )
{
    size_t size = 0u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( size, wire.size() );
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    const std::array<uint8_t, 35> expected = { HIL_RIG_PROTOCOL_VERSION_MAJOR,
                                               HIL_RIG_PROTOCOL_VERSION_MINOR,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               50,
                                               0,
                                               12,
                                               0,
                                               1,
                                               0,
                                               1,
                                               2,
                                               5,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0 };
    EXPECT_EQ( wire, expected );
    size_t storage = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(), &storage ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );
    HIL_Application_Message_T decoded{};
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               nullptr, 0u, &storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.rig_status.flags, 5u );
    message.has_test_id = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );
    message.body.rig_status.flags          = HIL_APPLICATION_RIG_STATUS_RESET_PERMITTED;
    message.body.rig_status.state          = HIL_APPLICATION_RIG_STATE_RECOVERING;
    message.body.rig_status.failure_source = HIL_APPLICATION_FAILURE_SOURCE_HOST_INTERFACE;
    message.body.rig_status.failure_stage  = HIL_APPLICATION_FAILURE_STAGE_CLEANUP;
    message.body.rig_status.failure_reason = HIL_APPLICATION_FAILURE_REASON_APPLICATION_RESET;
    message.test_id.bytes[15]              = 0xff;
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               nullptr, 0u, &storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.test_id.bytes[15], 0xff );
    EXPECT_EQ( decoded.body.rig_status.failure_reason,
               HIL_APPLICATION_FAILURE_REASON_APPLICATION_RESET );
}

/** Reject unknown schemas/enums/bits and invalid provenance without publishing output. */
TEST_F( ApplicationRigStatus, RejectsMalformedSemanticFieldsAndTruncation )
{
    size_t size = 0u;
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    for ( const auto offset : { 25u, 26u, 27u, 31u, 32u, 33u } )
    {
        auto invalid    = wire;
        invalid[offset] = 0xff;
        HIL_Application_Message_T decoded{};
        size_t                    storage = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, invalid.data(),
                                                             invalid.size(), &storage ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, invalid.data(), invalid.size(),
                                                   &decoded, nullptr, 0u, &storage ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
        EXPECT_EQ( storage, 0u );
    }
    auto unsupported = wire;
    unsupported[23]  = 2u;
    size_t storage   = 0u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, unsupported.data(),
                                                         unsupported.size(), &storage ),
               HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE );
    for ( size_t length = 0; length < wire.size(); ++length )
    {
        HIL_Application_Message_T decoded{};
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), length, &decoded, nullptr,
                                                   0u, &storage ),
                   HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE );
        EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    }
    for ( const auto flags : { 1u, 3u, 7u, 9u, 13u } )
    {
        message.body.rig_status.flags = flags;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    }
}

/** GET_STATUS keeps the five-byte request and existing rejected-query Response. */
TEST_F( ApplicationRigStatus, QueryAndRejectedResponseEchoTheCommand )
{
    message.type                = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    message.body.global_control = { HIL_APPLICATION_GLOBAL_CONTROL_GET_STATUS, 0u };
    size_t size                 = 0u;
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 28u );
    EXPECT_EQ( wire[23], 2u );
    message                       = {};
    message.type                  = HIL_APPLICATION_MESSAGE_TYPE_RESPONSE;
    message.body.response.scope   = HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL;
    message.body.response.outcome = HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED;
    message.body.response.reason  = HIL_APPLICATION_RESPONSE_REASON_HARDWARE_NOT_READY;
    message.body.response.global_control_command = HIL_APPLICATION_GLOBAL_CONTROL_GET_STATUS;
    std::array<uint8_t, 64> response{};
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, response.data(), response.size(),
                                               &size ),
               HIL_APPLICATION_STATUS_OK );
    HIL_Application_Message_T decoded{};
    size_t                    storage = 0u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, response.data(), size, &decoded, nullptr,
                                               0u, &storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.response.global_control_command,
               HIL_APPLICATION_GLOBAL_CONTROL_GET_STATUS );
}

}  // namespace
