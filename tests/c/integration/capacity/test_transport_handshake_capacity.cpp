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

void AssertInitialized( const hil_rig_protocol::test::TransportPairInitializationResult& result )
{
    ASSERT_EQ( result.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( result.host_status.has_value() );
    ASSERT_TRUE( result.rig_status.has_value() );
    ASSERT_EQ( *result.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *result.rig_status, HIL_TRANSPORT_STATUS_OK );
}

void TransferExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

void DrainExpectEmpty( TransportTestEndpoint& endpoint )
{
    const auto drained = endpoint.DrainEvents();
    ASSERT_EQ( drained.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void FillEventQueueAdaptively( TransportPairHarness& pair, TransportTestEndpoint& endpoint,
                               std::size_t* queued_capacity = nullptr )
{
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( endpoint.Role() );
    ASSERT_TRUE( direction.has_value() );

    const std::vector<std::uint8_t> malformed{ 0x05u, 0x11u, 0x22u, 0x00u };
    bool                            full         = false;
    std::size_t                     queued_count = 0u;
    for ( std::size_t attempt = 0u; attempt < 1024u; ++attempt )
    {
        pair.Link().InjectReadyBytes( *direction, malformed );
        const auto delivery = pair.Link().DeliverReady( endpoint );
        ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
        ASSERT_TRUE( delivery.transport_status.has_value() );
        ASSERT_EQ( delivery.bytes_consumed, malformed.size() );
        ASSERT_EQ( pair.Link().ReadyByteCount( *direction ), 0u );
        if ( *delivery.transport_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
        {
            full = true;
            break;
        }
        ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
        ++queued_count;
    }
    ASSERT_TRUE( full );

    // Free one slot and let the deferred diagnostic occupy it. The queue is now
    // known to be full without relying on its private depth.
    const auto freed = endpoint.ReadEvent();
    ASSERT_EQ( freed.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( freed.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    const auto resumed = pair.Link().DeliverZeroLength( endpoint );
    ASSERT_EQ( resumed.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    ASSERT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( resumed.bytes_consumed, 0u );
    if ( queued_capacity != nullptr )
    {
        *queued_capacity = queued_count;
    }
}

std::size_t CountEvent( const std::vector<HIL_Transport_Event_T>& events,
                        const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(),
                       [type]( const auto& event ) { return event.type == type; } ) );
}

}  // namespace

TEST( TransportIntegrationHandshakeCapacity, FinalAckWaitsTransactionallyForHostEventCapacity )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x123456789ABCDE10 ), 0x1200u ),
        TransportTestEndpointConfig::Rig( 0x3400u ) ) );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );

    const auto before_ack = pair.Host().GetStatus();
    ASSERT_EQ( before_ack.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( before_ack.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    ASSERT_EQ( before_ack.snapshot.reliable_delivery_pending, 1u );
    ASSERT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    FillEventQueueAdaptively( pair, pair.Host() );

    const auto accepted_ack = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( accepted_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted_ack.transport_status.has_value() );
    ASSERT_EQ( *accepted_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted_ack.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted_ack.handle ) );

    const auto blocked = pair.Link().DeliverReady( pair.Host() );
    ASSERT_EQ( blocked.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, blocked.bytes_offered );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::RigToHost ), 0u );

    const auto blocked_status = pair.Host().GetStatus();
    ASSERT_EQ( blocked_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( blocked_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( blocked_status.snapshot.reliable_delivery_pending, 1u );

    for ( std::size_t retry = 0u; retry < 3u; ++retry )
    {
        const auto still_blocked = pair.Link().DeliverZeroLength( pair.Host() );
        ASSERT_EQ( still_blocked.harness_status, TransportTestHarnessStatus::Ok );
        ASSERT_TRUE( still_blocked.transport_status.has_value() );
        EXPECT_EQ( *still_blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
        EXPECT_EQ( still_blocked.bytes_consumed, 0u );
        const auto status = pair.Host().GetStatus();
        ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
        EXPECT_EQ( status.snapshot.reliable_delivery_pending, 1u );
    }

    const auto freed = pair.Host().ReadEvent();
    ASSERT_EQ( freed.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( freed.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    const auto resumed = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_EQ( resumed.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( resumed.bytes_consumed, 0u );

    const auto established = pair.Host().GetStatus();
    ASSERT_EQ( established.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( established.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( established.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( established.snapshot.output_pending, 0u );

    const auto events = pair.Host().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ),
               events.events.size() - 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );

    const auto no_duplicate = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_TRUE( no_duplicate.transport_status.has_value() );
    EXPECT_EQ( *no_duplicate.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportIntegrationHandshakeCapacity,
      RigConfirmAcceptanceWaitsTransactionallyForEventCapacity )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x223456789ABCDE20 ), 0x2200u ),
        TransportTestEndpointConfig::Rig( 0x4400u ) ) );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );

    const auto accepted_confirm = pair.Link().AcceptOutput( pair.Host(), pair.HostNow() );
    ASSERT_EQ( accepted_confirm.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted_confirm.transport_status.has_value() );
    ASSERT_EQ( *accepted_confirm.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted_confirm.handle.has_value() );
    const auto duplicate_confirm = pair.Link().DuplicateAccepted( *accepted_confirm.handle );
    ASSERT_TRUE( duplicate_confirm.has_value() );

    FillEventQueueAdaptively( pair, pair.Rig() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted_confirm.handle ) );
    const auto blocked = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_EQ( blocked.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, blocked.bytes_offered );

    const auto blocked_status = pair.Rig().GetStatus();
    ASSERT_EQ( blocked_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( blocked_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( blocked_status.snapshot.reliable_delivery_pending, 1u );
    EXPECT_EQ( blocked_status.snapshot.output_pending, 0u );
    EXPECT_EQ( pair.Rig().PeekOutput().status, HIL_TRANSPORT_STATUS_NOT_READY );

    for ( std::size_t retry = 0u; retry < 3u; ++retry )
    {
        const auto still_blocked = pair.Link().DeliverZeroLength( pair.Rig() );
        ASSERT_TRUE( still_blocked.transport_status.has_value() );
        EXPECT_EQ( *still_blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
        const auto status = pair.Rig().GetStatus();
        ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
        EXPECT_EQ( status.snapshot.reliable_delivery_pending, 1u );
        EXPECT_EQ( status.snapshot.output_pending, 0u );
    }

    const auto freed = pair.Rig().ReadEvent();
    ASSERT_EQ( freed.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( freed.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    const auto resumed = pair.Link().DeliverZeroLength( pair.Rig() );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto established = pair.Rig().GetStatus();
    ASSERT_EQ( established.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( established.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( established.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( established.snapshot.output_pending, 1u );

    const auto events = pair.Rig().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );

    const auto first_ack = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( first_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( first_ack.transport_status.has_value() );
    ASSERT_EQ( *first_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( first_ack.handle.has_value() );
    hil_rig_protocol::test::TransportTestOutputItem first_ack_item{};
    ASSERT_TRUE( pair.Link().TakeAccepted( *first_ack.handle, first_ack_item ) );

    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *duplicate_confirm ) );
    const auto duplicate_delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( duplicate_delivery.transport_status.has_value() );
    EXPECT_EQ( *duplicate_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );

    const auto repeated_ack = pair.Rig().PeekOutput();
    ASSERT_EQ( repeated_ack.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( repeated_ack.bytes, first_ack_item.bytes );
    ASSERT_EQ( pair.Rig().CommitOutput( pair.RigNow() ), HIL_TRANSPORT_STATUS_OK );

    // Complete the peer side with the original final ACK, then prove normal
    // receive-sequence progression with an Application transfer.
    pair.Link().InjectReadyBytes( TransportTestDirection::RigToHost, first_ack_item.bytes );
    const auto host_establishment = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( host_establishment.transport_status.has_value() );
    ASSERT_EQ( *host_establishment.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    DrainExpectEmpty( pair.Host() );

    const std::vector<std::uint8_t> payload{ 0x31u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    const auto confirmed = pair.Host().ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

TEST( TransportIntegrationHandshakeCapacity, FinalAckWaitsForCallerPinnedConfirmRetry )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x323456789ABCDE30 ), 0x3200u, 10u, 2u ),
        TransportTestEndpointConfig::Rig( 0x5400u, 10u, 2u ) ) );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );

    pair.SetHostTime( 10u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto pinned_retry = pair.Host().PeekOutput();
    ASSERT_EQ( pinned_retry.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( pinned_retry.bytes.empty() );

    const auto ack_transfer = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( ack_transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( ack_transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( ack_transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *ack_transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( *ack_transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( ack_transfer.delivery.bytes_consumed, ack_transfer.delivery.bytes_offered );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::RigToHost ), 0u );

    const auto blocked = pair.Host().GetStatus();
    ASSERT_EQ( blocked.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( blocked.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( blocked.snapshot.reliable_delivery_pending, 1u );
    EXPECT_EQ( blocked.snapshot.output_pending, 1u );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );

    const auto same_owner_bytes = pair.Host().PeekOutput();
    ASSERT_EQ( same_owner_bytes.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( same_owner_bytes.bytes, pinned_retry.bytes );

    for ( std::size_t retry = 0u; retry < 3u; ++retry )
    {
        const auto retained = pair.Link().DeliverZeroLength( pair.Host() );
        ASSERT_TRUE( retained.transport_status.has_value() );
        EXPECT_EQ( *retained.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
        EXPECT_EQ( pair.Host().PeekOutput().bytes, pinned_retry.bytes );
        EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
                   HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    }

    ASSERT_EQ( pair.Host().CommitOutput( 11u ), HIL_TRANSPORT_STATUS_OK );
    const auto resumed = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto established = pair.Host().GetStatus();
    ASSERT_EQ( established.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( established.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( established.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( established.snapshot.output_pending, 0u );

    const auto events = pair.Host().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( events.events.size(), 1u );
    EXPECT_EQ( pair.Host().PeekOutput().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportIntegrationHandshakeCapacity, ApplicationProofWaitsForCallerPinnedConfirmRetry )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x423456789ABCDE40 ), 0x4200u, 10u, 2u ),
        TransportTestEndpointConfig::Rig( 0x6400u, 10u, 2u ) ) );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    DrainExpectEmpty( pair.Rig() );

    // Commit the final ACK to the external link but delay its delivery.
    const auto final_ack = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( final_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( final_ack.transport_status.has_value() );
    ASSERT_EQ( *final_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( final_ack.handle.has_value() );
    hil_rig_protocol::test::TransportTestOutputItem final_ack_item{};
    ASSERT_TRUE( pair.Link().TakeAccepted( *final_ack.handle, final_ack_item ) );

    pair.SetHostTime( 10u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto pinned_retry = pair.Host().PeekOutput();
    ASSERT_EQ( pinned_retry.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( pinned_retry.bytes.empty() );

    const std::vector<std::uint8_t> payload{ 0x00u, 0x51u, 0x00u, 0x52u };
    ASSERT_EQ( pair.Rig().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto application = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( application.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( application.transport_status.has_value() );
    ASSERT_EQ( *application.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( application.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *application.handle ) );

    const auto blocked = pair.Link().DeliverReady( pair.Host() );
    ASSERT_EQ( blocked.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, blocked.bytes_offered );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.application_message_pending, 0u );
    EXPECT_EQ( pair.Host().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Host().PeekOutput().bytes, pinned_retry.bytes );

    const auto still_blocked = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_TRUE( still_blocked.transport_status.has_value() );
    EXPECT_EQ( *still_blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( pair.Host().PeekOutput().bytes, pinned_retry.bytes );

    ASSERT_EQ( pair.Host().CommitOutput( 11u ), HIL_TRANSPORT_STATUS_OK );
    const auto resumed = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    const auto established = pair.Host().GetStatus();
    ASSERT_EQ( established.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( established.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( established.snapshot.application_message_pending, 1u );
    EXPECT_EQ( established.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( established.snapshot.output_pending, 1u );

    const auto received = pair.Host().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
    EXPECT_EQ( pair.Host().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    const auto events = pair.Host().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( events.events.size(), 1u );
    EXPECT_EQ( events.events.front().type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );

    // The output created while processing the retained Application is its ACK.
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    const auto delivered = pair.Rig().ReadEvent();
    ASSERT_EQ( delivered.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( delivered.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    // The delayed final handshake ACK is now an allowed exact duplicate and
    // must not produce another event or disturb the established session.
    pair.Link().InjectReadyBytes( TransportTestDirection::RigToHost, final_ack_item.bytes );
    const auto delayed_ack = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( delayed_ack.transport_status.has_value() );
    EXPECT_EQ( *delayed_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportIntegrationHandshakeCapacity,
      MultiFrameChunkStopsAtRetainedFinalAckAndPreservesExactSuffix )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x523456789ABCDE50 ), 0x5200u ),
        TransportTestEndpointConfig::Rig( 0x7400u ) ) );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );

    const auto final_ack = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( final_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( final_ack.transport_status.has_value() );
    ASSERT_EQ( *final_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( final_ack.handle.has_value() );
    hil_rig_protocol::test::TransportTestOutputItem final_ack_item{};
    ASSERT_TRUE( pair.Link().TakeAccepted( *final_ack.handle, final_ack_item ) );

    std::size_t event_capacity = 0u;
    FillEventQueueAdaptively( pair, pair.Host(), &event_capacity );
    ASSERT_GT( event_capacity, 0u );
    const auto freed_for_first_frame = pair.Host().ReadEvent();
    ASSERT_EQ( freed_for_first_frame.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( freed_for_first_frame.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    const std::vector<std::uint8_t> first_frame{ 0x05u, 0x11u, 0x22u, 0x00u };
    std::vector<std::uint8_t>       chunk = first_frame;
    chunk.insert( chunk.end(), final_ack_item.bytes.begin(), final_ack_item.bytes.end() );
    chunk.insert( chunk.end(), final_ack_item.bytes.begin(), final_ack_item.bytes.end() );
    pair.Link().InjectReadyBytes( TransportTestDirection::RigToHost, chunk );

    const auto blocked = pair.Link().DeliverReady( pair.Host() );
    ASSERT_EQ( blocked.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, first_frame.size() + final_ack_item.bytes.size() );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::RigToHost ),
               final_ack_item.bytes.size() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    const auto freed_for_establishment = pair.Host().ReadEvent();
    ASSERT_EQ( freed_for_establishment.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( freed_for_establishment.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    const auto resumed = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::RigToHost ),
               final_ack_item.bytes.size() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    const auto suffix = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( suffix.transport_status.has_value() );
    EXPECT_EQ( *suffix.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( suffix.bytes_consumed, final_ack_item.bytes.size() );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::RigToHost ), 0u );

    const auto events = pair.Host().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( events.events.size(), event_capacity );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ),
               event_capacity - 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );
}

TEST( TransportIntegrationHandshakeCapacity,
      ForeignSessionAckRemainsInvalidAfterEventCapacityIsReleased )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x623456789ABCDE60 ), 0x6200u ),
        TransportTestEndpointConfig::Rig( 0x8400u ) ) );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );

    TransportPairHarness foreign{};
    AssertInitialized( foreign.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x723456789ABCDE70 ), 0x7200u ),
        TransportTestEndpointConfig::Rig( 0x9400u ) ) );
    ASSERT_EQ( foreign.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( foreign, TransportTestDirection::HostToRig );
    ASSERT_EQ( foreign.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( foreign, TransportTestDirection::RigToHost );
    ASSERT_EQ( foreign.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( foreign, TransportTestDirection::HostToRig );
    const auto foreign_ack = foreign.Link().AcceptOutput( foreign.Rig(), foreign.RigNow() );
    ASSERT_EQ( foreign_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( foreign_ack.transport_status.has_value() );
    ASSERT_EQ( *foreign_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( foreign_ack.handle.has_value() );
    hil_rig_protocol::test::TransportTestOutputItem foreign_ack_item{};
    ASSERT_TRUE( foreign.Link().TakeAccepted( *foreign_ack.handle, foreign_ack_item ) );

    FillEventQueueAdaptively( pair, pair.Host() );
    pair.Link().InjectReadyBytes( TransportTestDirection::RigToHost, foreign_ack_item.bytes );
    const auto blocked = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, foreign_ack_item.bytes.size() );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 1u );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.last_failure, HIL_TRANSPORT_FAILURE_PROTOCOL );

    for ( std::size_t retry = 0u; retry < 2u; ++retry )
    {
        const auto retained = pair.Link().DeliverZeroLength( pair.Host() );
        ASSERT_TRUE( retained.transport_status.has_value() );
        EXPECT_EQ( *retained.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
        EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
                   HIL_TRANSPORT_SESSION_STATE_RECOVERING );
        EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 1u );
    }

    const auto old_events = pair.Host().DrainEvents();
    ASSERT_EQ( old_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_FALSE( old_events.events.empty() );
    EXPECT_EQ( CountEvent( old_events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 0u );

    const auto resumed = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    const auto recovering = pair.Host().GetStatus();
    ASSERT_EQ( recovering.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( recovering.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( recovering.snapshot.last_failure, HIL_TRANSPORT_FAILURE_PROTOCOL );
    EXPECT_EQ( recovering.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( recovering.snapshot.output_pending, 1u );

    const auto recovery_events = pair.Host().DrainEvents();
    ASSERT_EQ( recovery_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    const std::vector<HIL_Transport_Event_Type_T> expected_recovery_events{
        HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
    };
    std::vector<HIL_Transport_Event_Type_T> actual_recovery_events{};
    for ( const auto& event : recovery_events.events )
    {
        actual_recovery_events.push_back( event.type );
    }
    EXPECT_EQ( actual_recovery_events, expected_recovery_events );
    EXPECT_EQ( CountEvent( recovery_events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 0u );

    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    DrainExpectEmpty( pair.Rig() );
    const auto replacement = pair.EstablishCleanSession();
    ASSERT_EQ( replacement.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( replacement.transport_status.has_value() );
    ASSERT_EQ( *replacement.transport_status, HIL_TRANSPORT_STATUS_OK );
    DrainExpectEmpty( pair.Host() );
    DrainExpectEmpty( pair.Rig() );

    const std::vector<std::uint8_t> payload{ 0x61u, 0x62u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    EXPECT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}
