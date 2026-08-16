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
constexpr std::uint8_t  MaxRetries     = 2u;

void InitializePair( TransportPairHarness& pair )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xD102030405060708 ), 100u, RetryTimeoutMs,
                                           MaxRetries ),
        TransportTestEndpointConfig::Rig( 700u, RetryTimeoutMs, MaxRetries ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

std::vector<std::uint8_t> PeekAndCommit( TransportTestEndpoint& endpoint,
                                         const std::uint32_t    now_ms )
{
    const auto output = endpoint.PeekOutput();
    EXPECT_EQ( output.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_GT( output.bytes.size(), 0u );
    if ( output.status != HIL_TRANSPORT_STATUS_OK )
    {
        return {};
    }
    EXPECT_EQ( endpoint.CommitOutput( now_ms ), HIL_TRANSPORT_STATUS_OK );
    return output.bytes;
}

HIL_Transport_Status_T DeliverBytes( TransportTestEndpoint&           receiver,
                                     const std::vector<std::uint8_t>& bytes )
{
    const auto result = receiver.ReceiveBytes( bytes );
    EXPECT_EQ( result.bytes_consumed, bytes.size() );
    return result.status;
}

std::size_t CountEvent( const std::vector<HIL_Transport_Event_T>& events,
                        const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(), [type]( const HIL_Transport_Event_T& event ) {
            return event.type == type;
        } ) );
}

void ExpectConnectingRetainsReliableItem( TransportTestEndpoint& endpoint,
                                          const std::uint8_t     output_pending )
{
    const auto status = endpoint.GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.snapshot.reliable_delivery_pending, 1u );
    EXPECT_EQ( status.snapshot.output_pending, output_pending );
    EXPECT_EQ( status.snapshot.last_failure, HIL_TRANSPORT_FAILURE_NONE );
}

void ExerciseAllRetriesThenExhaust( TransportTestEndpoint&           endpoint,
                                    const std::vector<std::uint8_t>& original )
{
    ASSERT_GT( original.size(), 0u );
    ExpectConnectingRetainsReliableItem( endpoint, 0u );

    ASSERT_EQ( endpoint.Process( 109u ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingRetainsReliableItem( endpoint, 0u );

    ASSERT_EQ( endpoint.Process( 110u ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingRetainsReliableItem( endpoint, 1u );
    const auto first_retry = endpoint.PeekOutput();
    ASSERT_EQ( first_retry.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( first_retry.bytes, original );
    const auto repeated_first_peek = endpoint.PeekOutput();
    ASSERT_EQ( repeated_first_peek.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( repeated_first_peek.bytes, original );

    /* Peeking pins the same retry but does not commit or spend it. */
    ASSERT_EQ( endpoint.Process( 1000u ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingRetainsReliableItem( endpoint, 1u );
    EXPECT_EQ( endpoint.PeekOutput().bytes, original );
    ASSERT_EQ( endpoint.CommitOutput( 110u ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( endpoint.Process( 119u ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingRetainsReliableItem( endpoint, 0u );
    ASSERT_EQ( endpoint.Process( 120u ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingRetainsReliableItem( endpoint, 1u );
    const auto second_retry = endpoint.PeekOutput();
    ASSERT_EQ( second_retry.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( second_retry.bytes, original );
    EXPECT_EQ( endpoint.PeekOutput().bytes, original );
    ASSERT_EQ( endpoint.CommitOutput( 120u ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( endpoint.Process( 129u ), HIL_TRANSPORT_STATUS_OK );
    ExpectConnectingRetainsReliableItem( endpoint, 0u );
    ASSERT_EQ( endpoint.Process( 130u ), HIL_TRANSPORT_STATUS_OK );
}

void ExpectHandshakeExhaustionRecovery( TransportTestEndpoint& endpoint )
{
    const auto status = endpoint.GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( status.snapshot.last_failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ( status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( status.snapshot.output_pending, 1u );

    const auto events = endpoint.DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ), 0u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED ), 0u );
    ASSERT_EQ( events.events.size(), 1u );
    EXPECT_EQ( events.events.front().status, HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    EXPECT_EQ( events.events.front().failure, HIL_TRANSPORT_FAILURE_DELIVERY );
}

TransportTestOutputItem CommitRecoveryReset( TransportPairHarness&        pair,
                                             const TransportTestDirection direction,
                                             const std::uint32_t          now_ms )
{
    TransportTestEndpoint& owner =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    const auto first_peek = owner.PeekOutput();
    EXPECT_EQ( first_peek.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_GT( first_peek.bytes.size(), 0u );
    const auto second_peek = owner.PeekOutput();
    EXPECT_EQ( second_peek.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( second_peek.bytes, first_peek.bytes );

    /* The RESET barrier prevents replacement establishment until it is committed. */
    EXPECT_EQ( owner.Process( now_ms - 1u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( owner.GetStatus().snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( owner.PeekOutput().bytes, first_peek.bytes );

    const auto accepted = pair.Link().AcceptOutput( owner, now_ms );
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( accepted.harness_status != TransportTestHarnessStatus::Ok
         || !accepted.transport_status.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK || !accepted.handle.has_value() )
    {
        return {};
    }

    TransportTestOutputItem reset{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, reset ) );
    EXPECT_EQ( reset.bytes, first_peek.bytes );
    return reset;
}

void ExpectPeerAcceptedRecoveryReset( TransportTestEndpoint& peer )
{
    const auto events = peer.DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ), 0u );
}

void TransferOutputExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( transfer.delivery.bytes_consumed, transfer.delivery.bytes_offered );
}

void CompleteReplacementHandshakeAndApplication( TransportPairHarness&            pair,
                                                 const std::vector<std::uint8_t>& failed_initiate )
{
    pair.SetBothTimes( 200u );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );

    const auto replacement_initiate = pair.Host().PeekOutput();
    ASSERT_EQ( replacement_initiate.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( replacement_initiate.bytes.size(), 0u );
    EXPECT_NE( replacement_initiate.bytes, failed_initiate );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );

    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    const auto host_events = pair.Host().DrainEvents();
    const auto rig_events  = pair.Rig().DrainEvents();
    ASSERT_EQ( host_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( rig_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( rig_events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ), 0u );
    EXPECT_EQ( CountEvent( rig_events.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ), 0u );

    const std::vector<std::uint8_t> payload{ 0x71u, 0x72u, 0x73u, 0x74u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    const auto confirmed = pair.Host().ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

std::vector<std::uint8_t> PublishInitialInitiate( TransportPairHarness& pair )
{
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    auto initiate = PeekAndCommit( pair.Host(), 100u );
    EXPECT_FALSE( initiate.empty() );
    EXPECT_EQ( pair.Host().PeekOutput().status, HIL_TRANSPORT_STATUS_NOT_READY );
    return initiate;
}

std::vector<std::uint8_t> PublishInitialResponse( TransportPairHarness&            pair,
                                                  const std::vector<std::uint8_t>& initiate )
{
    EXPECT_EQ( DeliverBytes( pair.Rig(), initiate ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    auto response = PeekAndCommit( pair.Rig(), 100u );
    EXPECT_FALSE( response.empty() );
    EXPECT_EQ( pair.Rig().PeekOutput().status, HIL_TRANSPORT_STATUS_NOT_READY );
    return response;
}

std::vector<std::uint8_t> PublishInitialConfirm( TransportPairHarness&            pair,
                                                 const std::vector<std::uint8_t>& response )
{
    EXPECT_EQ( DeliverBytes( pair.Host(), response ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    auto confirm = PeekAndCommit( pair.Host(), 100u );
    EXPECT_FALSE( confirm.empty() );
    EXPECT_EQ( pair.Host().PeekOutput().status, HIL_TRANSPORT_STATUS_NOT_READY );
    return confirm;
}

}  // namespace

TEST( TransportIntegrationHandshakeExhaustion,
      InitiateRetriesPreserveBytesAndExhaustIntoResetBeforeFreshSession )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitialInitiate( pair );

    ExerciseAllRetriesThenExhaust( pair.Host(), initiate );
    ExpectHandshakeExhaustionRecovery( pair.Host() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 0u );

    const auto reset = CommitRecoveryReset( pair, TransportTestDirection::HostToRig, 132u );
    ASSERT_FALSE( reset.bytes.empty() );
    /* The rig never adopted the failed identity, so this best-effort RESET may be dropped. */
    CompleteReplacementHandshakeAndApplication( pair, initiate );
}

TEST( TransportIntegrationHandshakeExhaustion,
      ResponseRetriesPreserveBytesAndResetFailedSessionBeforeReplacement )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitialInitiate( pair );
    const auto response = PublishInitialResponse( pair, initiate );

    ExerciseAllRetriesThenExhaust( pair.Rig(), response );
    ExpectHandshakeExhaustionRecovery( pair.Rig() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );

    const auto reset = CommitRecoveryReset( pair, TransportTestDirection::RigToHost, 132u );
    ASSERT_FALSE( reset.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair.Host(), reset.bytes ), HIL_TRANSPORT_STATUS_OK );
    ExpectPeerAcceptedRecoveryReset( pair.Host() );
    CompleteReplacementHandshakeAndApplication( pair, initiate );
}

TEST( TransportIntegrationHandshakeExhaustion,
      ConfirmRetriesPreserveBytesWithoutFalseEstablishmentThenRecover )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto initiate = PublishInitialInitiate( pair );
    const auto response = PublishInitialResponse( pair, initiate );
    const auto confirm  = PublishInitialConfirm( pair, response );

    ExerciseAllRetriesThenExhaust( pair.Host(), confirm );
    ExpectHandshakeExhaustionRecovery( pair.Host() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 0u );
    EXPECT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );

    const auto reset = CommitRecoveryReset( pair, TransportTestDirection::HostToRig, 132u );
    ASSERT_FALSE( reset.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair.Rig(), reset.bytes ), HIL_TRANSPORT_STATUS_OK );
    ExpectPeerAcceptedRecoveryReset( pair.Rig() );
    CompleteReplacementHandshakeAndApplication( pair, initiate );
}
