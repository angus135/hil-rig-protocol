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

void InitializeAndEstablish( TransportPairHarness& pair, const std::uint8_t max_retries = 2u )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xD2340000 ), 50u, 10u, max_retries ),
        TransportTestEndpointConfig::Rig( 900u, 10u, max_retries ) );
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

TransportTestOutputItem AcceptAndTake( TransportPairHarness&        pair,
                                       const TransportTestDirection direction,
                                       const std::uint32_t          now_ms )
{
    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    if ( direction == TransportTestDirection::HostToRig )
    {
        pair.SetHostTime( now_ms );
    }
    else
    {
        pair.SetRigTime( now_ms );
    }

    const auto accepted = pair.Link().AcceptOutput( sender, now_ms );
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( accepted.harness_status != TransportTestHarnessStatus::Ok
         || !accepted.transport_status.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK || !accepted.handle.has_value() )
    {
        return {};
    }

    EXPECT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
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

void SaturateEventQueueWithProtocolErrors( TransportPairHarness&  pair,
                                           TransportTestEndpoint& endpoint )
{
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( endpoint.Role() );
    ASSERT_TRUE( direction.has_value() );
    const std::vector<std::uint8_t> malformed{ 0x05u, 0x11u, 0x22u, 0x00u };

    constexpr std::size_t SafetyLimit = 1024u;
    bool                  saturated   = false;
    for ( std::size_t attempt = 0u; attempt < SafetyLimit; ++attempt )
    {
        pair.Link().InjectReadyBytes( *direction, malformed );
        const auto delivery = pair.Link().DeliverReady( endpoint );
        ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
        ASSERT_TRUE( delivery.transport_status.has_value() );
        ASSERT_EQ( delivery.bytes_consumed, malformed.size() );
        ASSERT_EQ( pair.Link().ReadyByteCount( *direction ), 0u );

        if ( *delivery.transport_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
        {
            saturated = true;
            break;
        }
        ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    }
    ASSERT_TRUE( saturated );

    const auto freed = endpoint.ReadEvent();
    ASSERT_EQ( freed.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( freed.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    const auto resumed = pair.Link().DeliverZeroLength( endpoint );
    ASSERT_EQ( resumed.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    ASSERT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( resumed.bytes_consumed, 0u );

    const auto no_deferred_work = pair.Link().DeliverZeroLength( endpoint );
    ASSERT_EQ( no_deferred_work.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( no_deferred_work.transport_status.has_value() );
    ASSERT_EQ( *no_deferred_work.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( no_deferred_work.bytes_consumed, 0u );

    const auto status = endpoint.GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( status.snapshot.event_pending, 1u );
}

std::size_t CountEventType( const std::vector<HIL_Transport_Event_T>& events,
                            const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(), [type]( const HIL_Transport_Event_T& event ) {
            return event.type == type;
        } ) );
}

}  // namespace

TEST( TransportIntegrationCapacity, UnreadApplicationRetainsNextFrameAndExactCallerSuffix )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> first_payload{ 0x11u, 0x12u, 0x13u };
    const std::vector<std::uint8_t> second_payload{ 0x21u, 0x22u, 0x23u, 0x24u };

    ASSERT_EQ( pair.Host().SubmitApplication( first_payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 100u );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );

    // Leave the first received Application message unread while allowing the
    // sender to complete that reliable transaction and submit another one.
    ASSERT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    ASSERT_EQ( pair.Host().SubmitApplication( second_payload ), HIL_TRANSPORT_STATUS_OK );
    const auto second_frame = AcceptAndTake( pair, TransportTestDirection::HostToRig, 110u );
    ASSERT_GT( second_frame.bytes.size(), 0u );

    // Offer a complete blocked frame plus bytes belonging to a later caller-owned
    // stream suffix. Transport may retain the complete frame, but not the suffix.
    const std::vector<std::uint8_t> suffix{ 0x00u, 0x00u };
    pair.Link().InjectReadyBytes( TransportTestDirection::HostToRig, second_frame.bytes );
    pair.Link().InjectReadyBytes( TransportTestDirection::HostToRig, suffix );
    const auto blocked = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_EQ( blocked.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, second_frame.bytes.size() );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::HostToRig ), suffix.size() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.application_message_pending, 1u );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 0u );

    const auto first_read = pair.Rig().ReadApplication();
    ASSERT_EQ( first_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( first_read.bytes, first_payload );

    // No caller bytes are replayed. A zero-length call retries the complete body
    // Transport already owns, now that the Application slot is free.
    const auto resumed = pair.Link().DeliverZeroLength( pair.Rig() );
    ASSERT_EQ( resumed.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( resumed.bytes_consumed, 0u );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::HostToRig ), suffix.size() );

    const auto second_read = pair.Rig().ReadApplication();
    ASSERT_EQ( second_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( second_read.bytes, second_payload );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );

    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    const auto confirmation = pair.Host().ReadEvent();
    ASSERT_EQ( confirmation.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmation.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    // The retained caller suffix can then be consumed independently.
    const auto suffix_delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( suffix_delivery.transport_status.has_value() );
    EXPECT_EQ( *suffix_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( suffix_delivery.bytes_consumed, suffix.size() );
    EXPECT_EQ( pair.Link().ReadyByteCount( TransportTestDirection::HostToRig ), 0u );
}

TEST( TransportIntegrationCapacity, FullEventQueueDefersExactAckUntilCallerFreesEventSlot )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );
    SaturateEventQueueWithProtocolErrors( pair, pair.Host() );

    const std::vector<std::uint8_t> payload{ 0x31u, 0x32u, 0x33u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 200u );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    const auto ack = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( ack.transport_status.has_value() );
    ASSERT_EQ( *ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( ack.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *ack.handle ) );

    const auto blocked = pair.Link().DeliverReady( pair.Host() );
    ASSERT_EQ( blocked.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( blocked.transport_status.has_value() );
    EXPECT_EQ( *blocked.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( blocked.bytes_consumed, blocked.bytes_offered );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );

    const auto freed = pair.Host().ReadEvent();
    ASSERT_EQ( freed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( freed.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    const auto resumed = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_EQ( resumed.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( resumed.transport_status.has_value() );
    EXPECT_EQ( *resumed.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 0u );

    const auto remaining = pair.Host().DrainEvents();
    EXPECT_EQ( remaining.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_GT( CountEventType( remaining.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 0u );
    EXPECT_EQ( CountEventType( remaining.events, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED ), 1u );
    EXPECT_EQ( remaining.events.size(),
               CountEventType( remaining.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR )
                   + CountEventType( remaining.events, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED ) );

    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
}

TEST( TransportIntegrationCapacity, FullEventQueueDefersRetryExhaustionUntilFailureCanBeReported )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, 0u );
    SaturateEventQueueWithProtocolErrors( pair, pair.Host() );

    const std::vector<std::uint8_t> payload{ 0x41u, 0x42u, 0x43u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto dropped = AcceptAndTake( pair, TransportTestDirection::HostToRig, 300u );
    ASSERT_GT( dropped.bytes.size(), 0u );

    pair.SetHostTime( 310u );
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    const auto blocked_status = pair.Host().GetStatus();
    ASSERT_EQ( blocked_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( blocked_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( blocked_status.snapshot.reliable_delivery_pending, 1u );
    EXPECT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );

    const auto freed = pair.Host().ReadEvent();
    ASSERT_EQ( freed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( freed.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    pair.SetHostTime( 311u );
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    const auto failed_status = pair.Host().GetStatus();
    ASSERT_EQ( failed_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( failed_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( failed_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( failed_status.snapshot.last_failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ( failed_status.snapshot.output_pending, 1u );

    const auto remaining = pair.Host().DrainEvents();
    EXPECT_EQ( remaining.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_GT( CountEventType( remaining.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 0u );
    EXPECT_EQ( CountEventType( remaining.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ), 1u );
    EXPECT_EQ( remaining.events.size(),
               CountEventType( remaining.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR )
                   + CountEventType( remaining.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ) );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
}
