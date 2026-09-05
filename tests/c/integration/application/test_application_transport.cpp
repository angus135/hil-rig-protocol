#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/application_test_codec.hpp"
#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::ApplicationAlternateTestId;
using hil_rig_protocol::test::ApplicationConfigurationsEqual;
using hil_rig_protocol::test::ApplicationInstructionsEqual;
using hil_rig_protocol::test::ApplicationResultsEqual;
using hil_rig_protocol::test::ApplicationTestCodec;
using hil_rig_protocol::test::ApplicationTestIdsEqual;
using hil_rig_protocol::test::kApplicationConfigurationBaseCompleteSize;
using hil_rig_protocol::test::kApplicationInstructionCompleteSize;
using hil_rig_protocol::test::kApplicationPayloadLengthOffset;
using hil_rig_protocol::test::kApplicationPayloadOffset;
using hil_rig_protocol::test::kApplicationResultCompleteSize;
using hil_rig_protocol::test::MakeApplicationConfigurationMessage;
using hil_rig_protocol::test::MakeApplicationInstructionMessage;
using hil_rig_protocol::test::MakeApplicationResultMessage;
using hil_rig_protocol::test::MakeMaximumApplicationConfigurationExtension;
using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpoint;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

void InitializeApplicationCodec(
    ApplicationTestCodec& codec,
    const std::size_t     max_message_size  = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
    const std::size_t     max_variable_size = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE,
    const std::uint32_t   max_tick_count    = 3u )
{
    ASSERT_EQ( codec.Initialize( max_message_size, max_variable_size, max_tick_count ),
               HIL_APPLICATION_STATUS_OK );
}

void InitializeAndEstablish( TransportPairHarness& pair,
                             const std::size_t     max_application_message_size =
                                 HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE,
                             const std::uint32_t retransmit_timeout_ms = 10u )
{
    auto host_config =
        TransportTestEndpointConfig::Host( UINT64_C( 0xA4400001 ), 40u, retransmit_timeout_ms, 2u );
    auto rig_config = TransportTestEndpointConfig::Rig( 740u, retransmit_timeout_ms, 2u );
    host_config.max_application_message_size = max_application_message_size;
    rig_config.max_application_message_size  = max_application_message_size;

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

void TransferOutputExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

void ExpectDeliveryConfirmed( TransportTestEndpoint& sender )
{
    const auto confirmed = sender.ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    EXPECT_EQ( confirmed.event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.failure, HIL_TRANSPORT_FAILURE_NONE );
    EXPECT_EQ( sender.ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

std::vector<std::uint8_t>
DeliverApplicationAndConfirm( TransportPairHarness& pair, const TransportTestDirection direction,
                              const std::vector<std::uint8_t>& application_bytes )
{
    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    TransportTestEndpoint& receiver =
        direction == TransportTestDirection::HostToRig ? pair.Rig() : pair.Host();
    const auto ack_direction = direction == TransportTestDirection::HostToRig
                                   ? TransportTestDirection::RigToHost
                                   : TransportTestDirection::HostToRig;

    EXPECT_EQ( sender.SubmitApplication( application_bytes ), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, direction );

    const auto received = receiver.ReadApplication();
    EXPECT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, application_bytes );

    TransferOutputExpectOk( pair, ack_direction );
    ExpectDeliveryConfirmed( sender );
    return received.bytes;
}

void ExpectSuccessfulFixedDecode( const hil_rig_protocol::test::ApplicationDecodeResult& decode )
{
    EXPECT_EQ( decode.storage_status, HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decode.encoded_validation_status, HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decode.decode_status, HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decode.required_storage_size, 0u );
    EXPECT_EQ( decode.validation_storage_size, 0u );
    EXPECT_EQ( decode.used_storage_size, 0u );
    EXPECT_EQ( decode.supplied_storage_size, 0u );
}

}  // namespace

TEST( ApplicationTransportIntegration, RepresentativeConfigurationEndToEndPreservesEveryField )
{
    constexpr std::array<std::uint8_t, 3u> extension{ 0xc3u, 0x5au, 0xe7u };
    const auto                             message = MakeApplicationConfigurationMessage(
        extension.data(), static_cast<std::uint8_t>( extension.size() ) );

    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    InitializeApplicationCodec( host_codec );
    InitializeApplicationCodec( rig_codec );

    const auto encoded = host_codec.EncodeSupportedMessage( message );
    ASSERT_EQ( encoded.validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.sizing_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.encoded_size, 223u );
    ASSERT_EQ( encoded.output_size, 223u );
    ASSERT_EQ( encoded.bytes.size(), 223u );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );
    const auto delivered =
        DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig, encoded.bytes );

    const auto decoded = rig_codec.DecodeMessage( delivered );
    ASSERT_EQ( decoded.storage_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded.required_storage_size, extension.size() );
    ASSERT_EQ( decoded.encoded_validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded.validation_storage_size, extension.size() );
    ASSERT_EQ( decoded.decode_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded.used_storage_size, extension.size() );
    ASSERT_EQ( decoded.supplied_storage_size, extension.size() );
    EXPECT_TRUE( ApplicationConfigurationsEqual( message, rig_codec.DecodedMessage() ) );

    // DELIVERY_CONFIRMED above proves only Transport delivery. No Application
    // acceptance Response is synthesized or inferred by this integration test.
    EXPECT_EQ( pair.Host().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( ApplicationTransportIntegration, MaximumConfigurationUsesExactLimitsAndChunkedByteStream )
{
    const auto extension = MakeMaximumApplicationConfigurationExtension();
    const auto message   = MakeApplicationConfigurationMessage(
        extension.data(), static_cast<std::uint8_t>( extension.size() ) );

    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    InitializeApplicationCodec( host_codec, 475u, 255u, 3u );
    InitializeApplicationCodec( rig_codec, 475u, 255u, 3u );
    EXPECT_EQ( host_codec.Config().max_encoded_message_size, 475u );
    EXPECT_EQ( rig_codec.Config().max_encoded_message_size, 475u );

    const auto encoded = host_codec.EncodeSupportedMessage( message );
    ASSERT_EQ( encoded.validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.sizing_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.encoded_size, 475u );
    ASSERT_EQ( encoded.output_size, 475u );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair, 475u );
    ASSERT_EQ( pair.Host().Config().max_application_message_size, 475u );
    ASSERT_EQ( pair.Rig().Config().max_application_message_size, 475u );
    ASSERT_EQ( pair.Host().SubmitApplication( encoded.bytes ), HIL_TRANSPORT_STATUS_OK );

    const auto accepted = pair.Link().AcceptOutput( pair.Host(), pair.HostNow() );
    ASSERT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted.handle ) );

    for ( const std::size_t chunk : { 1u, 17u, 63u } )
    {
        const auto delivery = pair.Link().DeliverReady( pair.Rig(), chunk );
        ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
        ASSERT_TRUE( delivery.transport_status.has_value() );
        ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( delivery.bytes_consumed, chunk );
        EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    }

    const auto final_delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_EQ( final_delivery.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( final_delivery.transport_status.has_value() );
    ASSERT_EQ( *final_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( final_delivery.bytes_consumed, 0u );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::HostToRig ), 0u );

    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( received.bytes, encoded.bytes );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    ExpectDeliveryConfirmed( pair.Host() );

    const auto decoded = rig_codec.DecodeMessage( received.bytes, 255u );
    ASSERT_EQ( decoded.storage_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded.required_storage_size, 255u );
    ASSERT_EQ( decoded.encoded_validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded.validation_storage_size, 255u );
    ASSERT_EQ( decoded.decode_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( decoded.supplied_storage_size, 255u );
    ASSERT_EQ( decoded.used_storage_size, 255u );
    EXPECT_TRUE( ApplicationConfigurationsEqual( message, rig_codec.DecodedMessage() ) );
}

TEST( ApplicationTransportIntegration, TransportSizeMismatchRejectsOnlyOversizedSubmission )
{
    const auto extension     = MakeMaximumApplicationConfigurationExtension();
    const auto configuration = MakeApplicationConfigurationMessage(
        extension.data(), static_cast<std::uint8_t>( extension.size() ) );

    ApplicationTestCodec codec{};
    InitializeApplicationCodec( codec, 475u, 255u, 3u );
    const auto encoded_configuration = codec.EncodeSupportedMessage( configuration );
    ASSERT_EQ( encoded_configuration.encoding_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_configuration.bytes.size(), 475u );

    TransportPairHarness too_small_pair{};
    InitializeAndEstablish( too_small_pair, 474u );
    EXPECT_EQ( too_small_pair.Host().SubmitApplication( encoded_configuration.bytes ),
               HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE );
    const auto rejected_status = too_small_pair.Host().GetStatus();
    ASSERT_EQ( rejected_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rejected_status.snapshot.output_pending, 0u );
    EXPECT_EQ( rejected_status.snapshot.reliable_delivery_pending, 0u );

    const auto instruction         = MakeApplicationInstructionMessage( 0u );
    const auto encoded_instruction = codec.EncodeSupportedMessage( instruction );
    ASSERT_EQ( encoded_instruction.encoding_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_instruction.bytes.size(), kApplicationInstructionCompleteSize );
    const auto smaller_delivery = DeliverApplicationAndConfirm(
        too_small_pair, TransportTestDirection::HostToRig, encoded_instruction.bytes );
    EXPECT_EQ( smaller_delivery, encoded_instruction.bytes );

    TransportPairHarness exact_pair{};
    InitializeAndEstablish( exact_pair, 475u );
    const auto exact_delivery = DeliverApplicationAndConfirm(
        exact_pair, TransportTestDirection::HostToRig, encoded_configuration.bytes );
    EXPECT_EQ( exact_delivery, encoded_configuration.bytes );
}

TEST( ApplicationTransportIntegration, FixedInstructionsHostToRigPreserveFieldsAcrossTicks )
{
    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    InitializeApplicationCodec( host_codec );
    InitializeApplicationCodec( rig_codec );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    std::uint32_t previous_tick = 0u;
    for ( std::uint32_t tick = 0u; tick < 3u; ++tick )
    {
        const auto message = MakeApplicationInstructionMessage( tick );
        const auto encoded = host_codec.EncodeSupportedMessage( message );
        ASSERT_EQ( encoded.validation_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.sizing_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.encoded_size, kApplicationInstructionCompleteSize );
        ASSERT_EQ( encoded.output_size, kApplicationInstructionCompleteSize );
        ASSERT_EQ( encoded.bytes.size(), kApplicationInstructionCompleteSize );

        const auto delivered =
            DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig, encoded.bytes );
        const auto decoded = rig_codec.DecodeMessage( delivered );
        ExpectSuccessfulFixedDecode( decoded );
        ASSERT_TRUE( ApplicationInstructionsEqual( message, rig_codec.DecodedMessage() ) );
        EXPECT_EQ( rig_codec.DecodedMessage().body.test_instruction.tick_number, tick );
        if ( tick != 0u )
        {
            EXPECT_GT( rig_codec.DecodedMessage().body.test_instruction.tick_number,
                       previous_tick );
        }
        previous_tick = rig_codec.DecodedMessage().body.test_instruction.tick_number;
    }
}

TEST( ApplicationTransportIntegration, FixedResultsRigToHostPreserveConditionsAndFieldsAcrossTicks )
{
    ApplicationTestCodec rig_codec{};
    ApplicationTestCodec host_codec{};
    InitializeApplicationCodec( rig_codec );
    InitializeApplicationCodec( host_codec );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    constexpr std::array<HIL_Application_Result_Condition_T, 3u> expected_conditions{
        HIL_APPLICATION_RESULT_CONDITION_OK,
        HIL_APPLICATION_RESULT_CONDITION_PARTIAL,
        HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM,
    };
    std::uint32_t previous_tick = 0u;
    for ( std::uint32_t tick = 0u; tick < 3u; ++tick )
    {
        const auto message = MakeApplicationResultMessage( tick );
        const auto encoded = rig_codec.EncodeSupportedMessage( message );
        ASSERT_EQ( encoded.validation_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.sizing_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.encoded_size, kApplicationResultCompleteSize );
        ASSERT_EQ( encoded.output_size, kApplicationResultCompleteSize );
        ASSERT_EQ( encoded.bytes.size(), kApplicationResultCompleteSize );

        const auto delivered =
            DeliverApplicationAndConfirm( pair, TransportTestDirection::RigToHost, encoded.bytes );
        const auto decoded = host_codec.DecodeMessage( delivered );
        ExpectSuccessfulFixedDecode( decoded );
        ASSERT_TRUE( ApplicationResultsEqual( message, host_codec.DecodedMessage() ) );
        EXPECT_EQ( host_codec.DecodedMessage().body.test_result.condition,
                   expected_conditions[tick] );
        EXPECT_EQ( host_codec.DecodedMessage().body.test_result.problem_detail,
                   message.body.test_result.problem_detail );
        if ( tick != 0u )
        {
            EXPECT_GT( host_codec.DecodedMessage().body.test_result.tick_number, previous_tick );
        }
        previous_tick = host_codec.DecodedMessage().body.test_result.tick_number;
    }
}

TEST( ApplicationTransportIntegration, TransportValidMalformedApplicationEnvelopeIsSeparated )
{
    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    InitializeApplicationCodec( host_codec );
    InitializeApplicationCodec( rig_codec );

    const auto valid_message = MakeApplicationInstructionMessage( 0u );
    const auto valid_encoded = host_codec.EncodeSupportedMessage( valid_message );
    ASSERT_EQ( valid_encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    auto malformed                                 = valid_encoded.bytes;
    malformed[kApplicationPayloadLengthOffset]     = 49u;
    malformed[kApplicationPayloadLengthOffset + 1] = 0u;

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );
    const auto delivered =
        DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig, malformed );
    ASSERT_EQ( delivered, malformed );

    const auto decoded = rig_codec.DecodeMessage( delivered );
    EXPECT_EQ( decoded.storage_status, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( decoded.encoded_validation_status, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( decoded.decode_status, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( decoded.required_storage_size, 0u );
    EXPECT_EQ( decoded.validation_storage_size, 0u );
    EXPECT_EQ( decoded.used_storage_size, 0u );
    EXPECT_EQ( rig_codec.DecodedMessage().type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    const auto later_message = MakeApplicationInstructionMessage( 1u );
    const auto later_encoded = host_codec.EncodeSupportedMessage( later_message );
    ASSERT_EQ( later_encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    const auto later_delivered = DeliverApplicationAndConfirm(
        pair, TransportTestDirection::HostToRig, later_encoded.bytes );
    const auto later_decoded = rig_codec.DecodeMessage( later_delivered );
    ExpectSuccessfulFixedDecode( later_decoded );
    EXPECT_TRUE( ApplicationInstructionsEqual( later_message, rig_codec.DecodedMessage() ) );
}

TEST( ApplicationTransportIntegration, TransportValidInvalidApplicationBodyFailsOnlyApplication )
{
    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    InitializeApplicationCodec( host_codec );
    InitializeApplicationCodec( rig_codec );

    const auto valid_message = MakeApplicationInstructionMessage( 0u );
    const auto valid_encoded = host_codec.EncodeSupportedMessage( valid_message );
    ASSERT_EQ( valid_encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    auto invalid_body                            = valid_encoded.bytes;
    invalid_body[kApplicationPayloadOffset + 4u] = 2u;

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );
    const auto delivered =
        DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig, invalid_body );
    ASSERT_EQ( delivered, invalid_body );

    const auto decoded = rig_codec.DecodeMessage( delivered );
    EXPECT_EQ( decoded.storage_status, HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.required_storage_size, 0u );
    EXPECT_EQ( decoded.encoded_validation_status, HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.decode_status, HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.validation_storage_size, 0u );
    EXPECT_EQ( decoded.used_storage_size, 0u );
    EXPECT_EQ( rig_codec.DecodedMessage().type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );

    // The codec produces no Response, and this test orchestration does not invent one.
    EXPECT_EQ( pair.Host().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    const auto rig_transport_status = pair.Rig().GetStatus();
    ASSERT_EQ( rig_transport_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_transport_status.snapshot.output_pending, 0u );
    EXPECT_EQ( rig_transport_status.snapshot.reliable_delivery_pending, 0u );

    const auto later_message = MakeApplicationInstructionMessage( 1u );
    const auto later_encoded = host_codec.EncodeSupportedMessage( later_message );
    ASSERT_EQ( later_encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    const auto later_delivered = DeliverApplicationAndConfirm(
        pair, TransportTestDirection::HostToRig, later_encoded.bytes );
    const auto later_decoded = rig_codec.DecodeMessage( later_delivered );
    ExpectSuccessfulFixedDecode( later_decoded );
    EXPECT_TRUE( ApplicationInstructionsEqual( later_message, rig_codec.DecodedMessage() ) );
}

TEST( ApplicationTransportIntegration, TransportCorruptionIsRejectedBeforeApplicationExposure )
{
    ApplicationTestCodec codec{};
    InitializeApplicationCodec( codec );
    const auto message = MakeApplicationInstructionMessage( 0u );
    const auto encoded = codec.EncodeSupportedMessage( message );
    ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );
    ASSERT_EQ( pair.Host().SubmitApplication( encoded.bytes ), HIL_TRANSPORT_STATUS_OK );

    const auto produced = pair.Host().PeekOutput();
    ASSERT_EQ( produced.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( produced.bytes.size(), 3u );
    ASSERT_EQ( produced.bytes.back(), 0u );
    ASSERT_NE( produced.bytes[2], 0u );

    const auto accepted = pair.Link().AcceptOutput( pair.Host(), pair.HostNow() );
    ASSERT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_TRUE( pair.Link().CorruptAcceptedByte( *accepted.handle, 2u, 0x01u ) );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted.handle ) );
    const auto corrupt_delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_EQ( corrupt_delivery.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( corrupt_delivery.transport_status.has_value() );
    ASSERT_EQ( *corrupt_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    const auto error = pair.Rig().ReadEvent();
    ASSERT_EQ( error.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( error.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( error.event.failure, HIL_TRANSPORT_FAILURE_PROTOCOL );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    // Application decode is intentionally not called because Transport exposed no data.
}

TEST( ApplicationTransportIntegration, RetryCarriesIdenticalRealApplicationBytesAndDeliversOnce )
{
    ApplicationTestCodec host_codec{};
    ApplicationTestCodec rig_codec{};
    InitializeApplicationCodec( host_codec );
    InitializeApplicationCodec( rig_codec );
    const auto message = MakeApplicationInstructionMessage( 2u );
    const auto encoded = host_codec.EncodeSupportedMessage( message );
    ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded.bytes.size(), kApplicationInstructionCompleteSize );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair, HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE, 10u );
    ASSERT_EQ( pair.Host().SubmitApplication( encoded.bytes ), HIL_TRANSPORT_STATUS_OK );

    pair.SetHostTime( 100u );
    const auto initial_output = pair.Host().PeekOutput();
    ASSERT_EQ( initial_output.status, HIL_TRANSPORT_STATUS_OK );
    const auto first_commit = pair.Link().AcceptOutput( pair.Host(), pair.HostNow() );
    ASSERT_TRUE( first_commit.transport_status.has_value() );
    ASSERT_EQ( *first_commit.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( first_commit.handle.has_value() );
    ASSERT_TRUE( pair.Link().DropAccepted( *first_commit.handle ) );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );

    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry_output = pair.Host().PeekOutput();
    ASSERT_EQ( retry_output.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( retry_output.bytes, initial_output.bytes );

    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, encoded.bytes );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    ExpectDeliveryConfirmed( pair.Host() );

    const auto decoded = rig_codec.DecodeMessage( received.bytes );
    ExpectSuccessfulFixedDecode( decoded );
    EXPECT_TRUE( ApplicationInstructionsEqual( message, rig_codec.DecodedMessage() ) );
}

TEST( ApplicationTransportIntegration, StatelessCodecLeavesTestIdAndTickSemanticsToEndpoints )
{
    constexpr std::array<std::uint8_t, 3u> extension{ 0x10u, 0x20u, 0x30u };
    ApplicationTestCodec                   host_codec{};
    ApplicationTestCodec                   rig_codec{};
    InitializeApplicationCodec( host_codec );
    InitializeApplicationCodec( rig_codec );

    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const auto configuration = MakeApplicationConfigurationMessage(
        extension.data(), static_cast<std::uint8_t>( extension.size() ) );
    const auto encoded_configuration = host_codec.EncodeSupportedMessage( configuration );
    ASSERT_EQ( encoded_configuration.encoding_status, HIL_APPLICATION_STATUS_OK );
    const auto delivered_configuration = DeliverApplicationAndConfirm(
        pair, TransportTestDirection::HostToRig, encoded_configuration.bytes );
    const auto decoded_configuration = rig_codec.DecodeMessage( delivered_configuration );
    ASSERT_EQ( decoded_configuration.decode_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_TRUE( ApplicationConfigurationsEqual( configuration, rig_codec.DecodedMessage() ) );

    // A firmware endpoint handler must reject this mismatched Test ID against its
    // accepted Configuration. The stateless codec has no active Test ID and accepts it.
    const auto different_id_message =
        MakeApplicationInstructionMessage( 0u, ApplicationAlternateTestId() );
    const auto different_id_encoded = host_codec.EncodeSupportedMessage( different_id_message );
    ASSERT_EQ( different_id_encoded.validation_status, HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( different_id_encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
    const auto different_id_delivered = DeliverApplicationAndConfirm(
        pair, TransportTestDirection::HostToRig, different_id_encoded.bytes );
    const auto different_id_decoded = rig_codec.DecodeMessage( different_id_delivered );
    ExpectSuccessfulFixedDecode( different_id_decoded );
    EXPECT_TRUE( ApplicationInstructionsEqual( different_id_message, rig_codec.DecodedMessage() ) );
    EXPECT_TRUE( ApplicationTestIdsEqual( ApplicationAlternateTestId(),
                                          rig_codec.DecodedMessage().test_id ) );

    // Likewise, endpoint integration owns tick ordering. Both tick 2 followed by
    // tick 1 are structurally in range for this codec configuration and are accepted.
    for ( const std::uint32_t tick : { 2u, 1u } )
    {
        const auto message = MakeApplicationInstructionMessage( tick );
        const auto encoded = host_codec.EncodeSupportedMessage( message );
        ASSERT_EQ( encoded.validation_status, HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( encoded.encoding_status, HIL_APPLICATION_STATUS_OK );
        const auto delivered =
            DeliverApplicationAndConfirm( pair, TransportTestDirection::HostToRig, encoded.bytes );
        const auto decoded = rig_codec.DecodeMessage( delivered );
        ExpectSuccessfulFixedDecode( decoded );
        EXPECT_EQ( rig_codec.DecodedMessage().body.test_instruction.tick_number, tick );
    }
}
