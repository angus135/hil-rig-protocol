#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpoint;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;
using hil_rig_protocol::test::TransportTestOutputItem;

constexpr std::uint32_t RetryTimeoutMs = 10u;

void InitializePair( TransportPairHarness& pair,
                     const std::uint64_t   seed = UINT64_C( 0xC102030405060708 ) )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( seed, 100u, RetryTimeoutMs, 2u ),
        TransportTestEndpointConfig::Rig( 700u, RetryTimeoutMs, 2u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

TransportTestOutputItem TakeOutput( TransportPairHarness&        pair,
                                    const TransportTestDirection direction,
                                    const std::uint32_t          now_ms )
{
    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    const auto accepted = pair.Link().AcceptOutput( sender, now_ms );
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( accepted.harness_status != TransportTestHarnessStatus::Ok
         || !accepted.transport_status.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK || !accepted.handle.has_value() )
    {
        return {};
    }

    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
}

HIL_Transport_Status_T DeliverBytes( TransportTestEndpoint&           receiver,
                                     const std::vector<std::uint8_t>& bytes )
{
    const auto result = receiver.ReceiveBytes( bytes );
    EXPECT_EQ( result.bytes_consumed, bytes.size() );
    return result.status;
}

std::vector<std::uint8_t> CorruptIntegrity( const std::vector<std::uint8_t>& valid )
{
    EXPECT_GE( valid.size(), 3u );
    EXPECT_EQ( valid.back(), 0u );
    std::vector<std::uint8_t> corrupted = valid;
    if ( corrupted.size() < 3u )
    {
        return corrupted;
    }

    const std::size_t  offset = corrupted.size() - 2u;
    const std::uint8_t mask   = corrupted[offset] == 1u ? 2u : 1u;
    corrupted[offset] ^= mask;
    return corrupted;
}

std::size_t CountEvent( const std::vector<HIL_Transport_Event_T>& events,
                        const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(), [type]( const HIL_Transport_Event_T& event ) {
            return event.type == type;
        } ) );
}

void ExpectProtocolErrorOnly( TransportTestEndpoint& endpoint )
{
    const auto events = endpoint.DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( events.events.size(), 1u );
    EXPECT_EQ( events.events.front().type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( events.events.front().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( events.events.front().failure, HIL_TRANSPORT_FAILURE_PROTOCOL );
    EXPECT_EQ( events.events.front().required_capacity, 0u );
}

void ExpectConnectingWithoutOutput( TransportTestEndpoint& endpoint )
{
    const auto status = endpoint.GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.snapshot.output_pending, 0u );
}

void ExpectEstablishedOnce( TransportTestEndpoint& endpoint )
{
    const auto status = endpoint.GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    const auto events = endpoint.DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 0u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );
}

std::vector<std::uint8_t> PublishInitiate( TransportPairHarness& pair,
                                           const std::uint32_t   commit_time )
{
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    auto initiate = TakeOutput( pair, TransportTestDirection::HostToRig, commit_time ).bytes;
    EXPECT_FALSE( initiate.empty() );
    return initiate;
}

std::vector<std::uint8_t> PublishResponse( TransportPairHarness&            pair,
                                           const std::vector<std::uint8_t>& initiate,
                                           const std::uint32_t              commit_time )
{
    EXPECT_EQ( DeliverBytes( pair.Rig(), initiate ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    auto response = TakeOutput( pair, TransportTestDirection::RigToHost, commit_time ).bytes;
    EXPECT_FALSE( response.empty() );
    return response;
}

std::vector<std::uint8_t> PublishConfirm( TransportPairHarness&            pair,
                                          const std::vector<std::uint8_t>& response,
                                          const std::uint32_t              commit_time )
{
    EXPECT_EQ( DeliverBytes( pair.Host(), response ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    auto confirm = TakeOutput( pair, TransportTestDirection::HostToRig, commit_time ).bytes;
    EXPECT_FALSE( confirm.empty() );
    return confirm;
}

std::vector<std::uint8_t> AcceptConfirmAndPublishFinalAck( TransportPairHarness&            pair,
                                                           const std::vector<std::uint8_t>& confirm,
                                                           const std::uint32_t commit_time )
{
    EXPECT_EQ( DeliverBytes( pair.Rig(), confirm ), HIL_TRANSPORT_STATUS_OK );
    auto final_ack = TakeOutput( pair, TransportTestDirection::RigToHost, commit_time ).bytes;
    EXPECT_FALSE( final_ack.empty() );
    return final_ack;
}

void CompleteHandshakeFromAcceptedInitiate( TransportPairHarness& pair,
                                            const std::uint32_t   base_time )
{
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response = TakeOutput( pair, TransportTestDirection::RigToHost, base_time ).bytes;
    ASSERT_FALSE( response.empty() );
    ASSERT_EQ( DeliverBytes( pair.Host(), response ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto confirm =
        TakeOutput( pair, TransportTestDirection::HostToRig, base_time + 1u ).bytes;
    ASSERT_FALSE( confirm.empty() );
    ASSERT_EQ( DeliverBytes( pair.Rig(), confirm ), HIL_TRANSPORT_STATUS_OK );
    const auto final_ack =
        TakeOutput( pair, TransportTestDirection::RigToHost, base_time + 2u ).bytes;
    ASSERT_FALSE( final_ack.empty() );
    ASSERT_EQ( DeliverBytes( pair.Host(), final_ack ), HIL_TRANSPORT_STATUS_OK );
    ExpectEstablishedOnce( pair.Host() );
    ExpectEstablishedOnce( pair.Rig() );
}

}  // namespace

TEST( TransportIntegrationHandshakeCorruption,
      IncompleteInitiateWaitsForRemainingBytesWithoutDiagnosticOrAdvancement )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitiate( pair, 100u );
    ASSERT_GT( initiate.size(), 1u );

    const auto prefix = pair.Rig().ReceiveBytes( initiate.data(), initiate.size() - 1u );
    ASSERT_EQ( prefix.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( prefix.bytes_consumed, initiate.size() - 1u );
    ExpectConnectingWithoutOutput( pair.Rig() );
    EXPECT_EQ( pair.Rig().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );

    const auto suffix = pair.Rig().ReceiveBytes( initiate.data() + initiate.size() - 1u, 1u );
    ASSERT_EQ( suffix.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( suffix.bytes_consumed, 1u );
    ExpectConnectingWithoutOutput( pair.Rig() );
    EXPECT_EQ( pair.Rig().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
    CompleteHandshakeFromAcceptedInitiate( pair, 110u );
}

TEST( TransportIntegrationHandshakeCorruption,
      StructurallyMalformedInputDoesNotPreventLaterCleanInitiate )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitiate( pair, 100u );

    const std::vector<std::uint8_t> malformed{ 0x05u, 0x11u, 0x22u, 0x00u };
    ASSERT_EQ( DeliverBytes( pair.Rig(), malformed ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingWithoutOutput( pair.Rig() );
    ExpectProtocolErrorOnly( pair.Rig() );

    ASSERT_EQ( DeliverBytes( pair.Rig(), initiate ), HIL_TRANSPORT_STATUS_OK );
    CompleteHandshakeFromAcceptedInitiate( pair, 110u );
}

TEST( TransportIntegrationHandshakeCorruption,
      IntegrityInvalidInitiateDoesNotAdvanceRigAndCleanCopyCompletesHandshake )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitiate( pair, 100u );

    ASSERT_EQ( DeliverBytes( pair.Rig(), CorruptIntegrity( initiate ) ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingWithoutOutput( pair.Rig() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ExpectProtocolErrorOnly( pair.Rig() );

    ASSERT_EQ( DeliverBytes( pair.Rig(), initiate ), HIL_TRANSPORT_STATUS_OK );
    CompleteHandshakeFromAcceptedInitiate( pair, 110u );
}

TEST( TransportIntegrationHandshakeCorruption,
      IntegrityInvalidResponseRetainsInitiateUntilRetryAndCleanResponse )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitiate( pair, 100u );
    const auto response = PublishResponse( pair, initiate, 101u );

    ASSERT_EQ( DeliverBytes( pair.Host(), CorruptIntegrity( response ) ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingWithoutOutput( pair.Host() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ExpectProtocolErrorOnly( pair.Host() );

    pair.SetHostTime( 109u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );
    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = TakeOutput( pair, TransportTestDirection::HostToRig, 110u ).bytes;
    ASSERT_EQ( retry, initiate );

    const auto confirm   = PublishConfirm( pair, response, 111u );
    const auto final_ack = AcceptConfirmAndPublishFinalAck( pair, confirm, 112u );
    ASSERT_EQ( DeliverBytes( pair.Host(), final_ack ), HIL_TRANSPORT_STATUS_OK );
    ExpectEstablishedOnce( pair.Host() );
    ExpectEstablishedOnce( pair.Rig() );
}

TEST( TransportIntegrationHandshakeCorruption,
      IntegrityInvalidConfirmDoesNotEstablishRigOrPublishFinalAck )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitiate( pair, 100u );
    const auto response = PublishResponse( pair, initiate, 101u );
    const auto confirm  = PublishConfirm( pair, response, 102u );

    ASSERT_EQ( DeliverBytes( pair.Rig(), CorruptIntegrity( confirm ) ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingWithoutOutput( pair.Rig() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ExpectProtocolErrorOnly( pair.Rig() );

    const auto final_ack = AcceptConfirmAndPublishFinalAck( pair, confirm, 103u );
    ASSERT_EQ( DeliverBytes( pair.Host(), final_ack ), HIL_TRANSPORT_STATUS_OK );
    ExpectEstablishedOnce( pair.Host() );
    ExpectEstablishedOnce( pair.Rig() );
}

TEST( TransportIntegrationHandshakeCorruption,
      IntegrityInvalidFinalAckRetainsConfirmUntilRetryAndCleanAck )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate  = PublishInitiate( pair, 100u );
    const auto response  = PublishResponse( pair, initiate, 101u );
    const auto confirm   = PublishConfirm( pair, response, 102u );
    const auto final_ack = AcceptConfirmAndPublishFinalAck( pair, confirm, 103u );

    ASSERT_EQ( DeliverBytes( pair.Host(), CorruptIntegrity( final_ack ) ),
               HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingWithoutOutput( pair.Host() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ExpectProtocolErrorOnly( pair.Host() );

    pair.SetHostTime( 111u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );
    pair.SetHostTime( 112u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = TakeOutput( pair, TransportTestDirection::HostToRig, 112u ).bytes;
    ASSERT_EQ( retry, confirm );

    ASSERT_EQ( DeliverBytes( pair.Host(), final_ack ), HIL_TRANSPORT_STATUS_OK );
    ExpectEstablishedOnce( pair.Host() );
    ExpectEstablishedOnce( pair.Rig() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );
}

TEST( TransportIntegrationHandshakeCorruption,
      ValidResponseWithIncompatibleSessionFieldsTriggersDocumentedRecovery )
{
    TransportPairHarness pair{};
    InitializePair( pair, UINT64_C( 0xC102030405060708 ) );
    const auto initiate = PublishInitiate( pair, 100u );
    const auto response = PublishResponse( pair, initiate, 101u );
    ASSERT_FALSE( response.empty() );

    TransportPairHarness foreign_pair{};
    InitializePair( foreign_pair, UINT64_C( 0xC202030405060708 ) );
    const auto foreign_initiate = PublishInitiate( foreign_pair, 100u );
    const auto foreign_response = PublishResponse( foreign_pair, foreign_initiate, 101u );
    ASSERT_FALSE( foreign_response.empty() );
    ASSERT_NE( foreign_response, response );

    ASSERT_EQ( DeliverBytes( pair.Host(), foreign_response ), HIL_TRANSPORT_STATUS_OK );
    const auto recovering = pair.Host().GetStatus();
    ASSERT_EQ( recovering.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( recovering.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( recovering.snapshot.last_failure, HIL_TRANSPORT_FAILURE_PROTOCOL );
    EXPECT_EQ( recovering.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( recovering.snapshot.output_pending, 1u );

    const auto events = pair.Host().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 0u );

    const auto reset = TakeOutput( pair, TransportTestDirection::HostToRig, 110u ).bytes;
    ASSERT_FALSE( reset.empty() );
    ASSERT_EQ( DeliverBytes( pair.Rig(), reset ), HIL_TRANSPORT_STATUS_OK );
    const auto rig_recovery_events = pair.Rig().DrainEvents();
    ASSERT_EQ( rig_recovery_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( rig_recovery_events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 1u );

    pair.SetBothTimes( 120u );
    const auto replacement = pair.EstablishCleanSession();
    ASSERT_EQ( replacement.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( replacement.transport_status.has_value() );
    ASSERT_EQ( *replacement.transport_status, HIL_TRANSPORT_STATUS_OK );
    ExpectEstablishedOnce( pair.Host() );
    ExpectEstablishedOnce( pair.Rig() );
}
