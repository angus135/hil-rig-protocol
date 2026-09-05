#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/application_test_codec.hpp"
#include "support/application_delivery_assertions.hpp"
#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::ApplicationConfigurationsEqual;
using hil_rig_protocol::test::ApplicationInstructionsEqual;
using hil_rig_protocol::test::ApplicationResultsEqual;
using hil_rig_protocol::test::ApplicationTestCodec;
using hil_rig_protocol::test::ApplicationTestIdsEqual;
using hil_rig_protocol::test::DeliverApplicationAndConfirm;
using hil_rig_protocol::test::kApplicationExecutionControlCompleteSize;
using hil_rig_protocol::test::kApplicationInstructionCompleteSize;
using hil_rig_protocol::test::kApplicationResultCompleteSize;
using hil_rig_protocol::test::MakeApplicationConfigurationMessage;
using hil_rig_protocol::test::MakeApplicationInstructionMessage;
using hil_rig_protocol::test::MakeApplicationResultMessage;
using hil_rig_protocol::test::MakeApplicationStartMessage;
using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

struct TestOwnedSemanticCheckpoints
{
    bool                 configuration_accepted = false;
    std::array<bool, 3u> tick_accepted{};
    bool                 complete_test_accepted = false;
    bool                 execution_completed    = false;
};

void InitializeApplicationCodec( ApplicationTestCodec& codec )
{
    ASSERT_EQ( codec.Initialize( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
                                 HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE, 3u ),
               HIL_APPLICATION_STATUS_OK );
}

void InitializeAndEstablish( TransportPairHarness& pair )
{
    auto host_config = TransportTestEndpointConfig::Host( UINT64_C( 0xA4400002 ), 40u, 10u, 2u );
    auto rig_config  = TransportTestEndpointConfig::Rig( 741u, 10u, 2u );

    const auto initialization = pair.InitializeConnected( host_config, rig_config );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );

    const auto establishment = pair.EstablishCleanSession();
    ASSERT_EQ( establishment.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( establishment.transport_status.has_value() );
    ASSERT_EQ( *establishment.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void ExpectSuccessfulFixedDecode( const hil_rig_protocol::test::ApplicationDecodeResult& decode )
{
    ASSERT_EQ( decode.storage_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decode.encoded_validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decode.decode_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decode.required_storage_size, 0u );
    ASSERT_EQ( decode.validation_storage_size, 0u );
    ASSERT_EQ( decode.used_storage_size, 0u );
}

}  // namespace

TEST( ApplicationFixedSubsetMessagePath,
      OrderedSupportedMessagesUseOnlyTestOwnedSemanticCheckpoints )
{
    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    ASSERT_NO_FATAL_FAILURE( InitializeApplicationCodec( host_codec ) );
    ASSERT_NO_FATAL_FAILURE( InitializeApplicationCodec( rig_codec ) );

    TransportPairHarness pair{};
    ASSERT_NO_FATAL_FAILURE( InitializeAndEstablish( pair ) );

    TestOwnedSemanticCheckpoints checkpoints{};

    constexpr std::array<std::uint8_t, 3u> extension{ 0x41u, 0x50u, 0x50u };
    const auto                             configuration = MakeApplicationConfigurationMessage(
        extension.data(), static_cast<std::uint8_t>( extension.size() ) );
    const auto encoded_configuration = host_codec.EncodeSupportedMessage( configuration );
    ASSERT_EQ( encoded_configuration.validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_configuration.sizing_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_configuration.encoding_status, HIL_APPLICATION_STATUS_OK );
    std::vector<std::uint8_t> delivered_configuration;
    ASSERT_NO_FATAL_FAILURE( DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig,
                                                           encoded_configuration.bytes,
                                                           delivered_configuration ) );
    const auto decoded_configuration = rig_codec.DecodeMessage( delivered_configuration );
    ASSERT_EQ( decoded_configuration.storage_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded_configuration.encoded_validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded_configuration.decode_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_TRUE( ApplicationConfigurationsEqual( configuration, rig_codec.DecodedMessage() ) );

    // Test orchestration marks semantic acceptance here. This is not an encoded
    // Application Response and Transport DELIVERY_CONFIRMED above is not treated
    // as Application acceptance.
    checkpoints.configuration_accepted = true;

    for ( std::uint32_t tick = 0u; tick < 3u; ++tick )
    {
        ASSERT_TRUE( checkpoints.configuration_accepted );
        const auto instruction         = MakeApplicationInstructionMessage( tick );
        const auto encoded_instruction = host_codec.EncodeSupportedMessage( instruction );
        ASSERT_EQ( encoded_instruction.validation_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded_instruction.sizing_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded_instruction.encoding_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded_instruction.bytes.size(), kApplicationInstructionCompleteSize );

        std::vector<std::uint8_t> delivered_instruction;
        ASSERT_NO_FATAL_FAILURE(
            DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig,
                                          encoded_instruction.bytes, delivered_instruction ) );
        const auto decoded_instruction = rig_codec.DecodeMessage( delivered_instruction );
        ASSERT_NO_FATAL_FAILURE( ExpectSuccessfulFixedDecode( decoded_instruction ) );
        ASSERT_TRUE( ApplicationInstructionsEqual( instruction, rig_codec.DecodedMessage() ) );

        // The endpoint-integration layer will eventually own this decision. The
        // Application codec deliberately retains no accepted-tick state.
        checkpoints.tick_accepted[tick] = true;
    }

    ASSERT_TRUE( checkpoints.tick_accepted[0] );
    ASSERT_TRUE( checkpoints.tick_accepted[1] );
    ASSERT_TRUE( checkpoints.tick_accepted[2] );
    checkpoints.complete_test_accepted = true;

    ASSERT_TRUE( checkpoints.complete_test_accepted );
    const auto start = MakeApplicationStartMessage();

    // Execution Control typed Encoded_Size is intentionally incomplete. Exercise
    // the existing public validation and encode path with caller-owned capacity.
    const auto encoded_start = host_codec.EncodeMessageWithCapacity( start, 64u );
    ASSERT_EQ( encoded_start.validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_start.encoding_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_start.output_size, kApplicationExecutionControlCompleteSize );
    ASSERT_EQ( encoded_start.bytes.size(), kApplicationExecutionControlCompleteSize );

    std::vector<std::uint8_t> delivered_start;
    ASSERT_NO_FATAL_FAILURE( DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig,
                                                           encoded_start.bytes, delivered_start ) );
    const auto decoded_start = rig_codec.DecodeMessage( delivered_start );
    ASSERT_NO_FATAL_FAILURE( ExpectSuccessfulFixedDecode( decoded_start ) );
    ASSERT_EQ( rig_codec.DecodedMessage().type, HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL );
    EXPECT_TRUE( ApplicationTestIdsEqual( start.test_id, rig_codec.DecodedMessage().test_id ) );
    EXPECT_EQ( rig_codec.DecodedMessage().body.execution_control.command,
               HIL_APPLICATION_CONTROL_START );
    EXPECT_EQ( rig_codec.DecodedMessage().body.execution_control.flags, 0u );

    // This completion checkpoint is test-owned orchestration only. It does not
    // model a production lifecycle handler or manufacture an Application Response.
    checkpoints.execution_completed = true;

    for ( std::uint32_t tick = 0u; tick < 3u; ++tick )
    {
        ASSERT_TRUE( checkpoints.execution_completed );
        const auto result         = MakeApplicationResultMessage( tick );
        const auto encoded_result = rig_codec.EncodeSupportedMessage( result );
        ASSERT_EQ( encoded_result.validation_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded_result.sizing_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded_result.encoding_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded_result.bytes.size(), kApplicationResultCompleteSize );

        std::vector<std::uint8_t> delivered_result;
        ASSERT_NO_FATAL_FAILURE( DeliverApplicationAndConfirm(
            pair, TransportTestDirection::RigToHost, encoded_result.bytes, delivered_result ) );
        const auto decoded_result = host_codec.DecodeMessage( delivered_result );
        ASSERT_NO_FATAL_FAILURE( ExpectSuccessfulFixedDecode( decoded_result ) );
        ASSERT_TRUE( ApplicationResultsEqual( result, host_codec.DecodedMessage() ) );
    }

    // This is intentionally only a fixed-subset message path. A complete
    // response-gated Application transaction remains deferred until Responses
    // and the remaining control sizing are implemented.
}
