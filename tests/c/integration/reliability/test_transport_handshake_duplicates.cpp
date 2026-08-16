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

void InitializePair( TransportPairHarness& pair )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xA102030405060708 ), 100u, 10u, 2u ),
        TransportTestEndpointConfig::Rig( 700u, 10u, 2u ) );
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

    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
}

HIL_Transport_Status_T DeliverBytes( TransportPairHarness& pair, TransportTestEndpoint& receiver,
                                     const std::vector<std::uint8_t>& bytes )
{
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( receiver.Role() );
    EXPECT_TRUE( direction.has_value() );
    if ( !direction.has_value() )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    pair.Link().InjectReadyBytes( *direction, bytes );
    const auto delivery = pair.Link().DeliverReady( receiver );
    EXPECT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( delivery.transport_status.has_value() );
    EXPECT_EQ( delivery.bytes_consumed, bytes.size() );
    return delivery.transport_status.value_or( HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
}

std::size_t CountEvent( const std::vector<HIL_Transport_Event_T>& events,
                        const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(), [type]( const HIL_Transport_Event_T& event ) {
            return event.type == type;
        } ) );
}

void ExpectExactlyOneEstablishment( TransportTestEndpoint& endpoint )
{
    const auto events = endpoint.DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 0u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );
}

struct HandshakeTraffic
{
    std::vector<std::uint8_t> initiate{};
    std::vector<std::uint8_t> response{};
    std::vector<std::uint8_t> confirm{};
    std::vector<std::uint8_t> final_ack{};
};

HandshakeTraffic AdvanceThroughFinalAck( TransportPairHarness& pair, const std::uint32_t base_time )
{
    HandshakeTraffic traffic{};
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    traffic.initiate = TakeOutput( pair, TransportTestDirection::HostToRig, base_time ).bytes;
    EXPECT_FALSE( traffic.initiate.empty() );
    EXPECT_EQ( DeliverBytes( pair, pair.Rig(), traffic.initiate ), HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    traffic.response = TakeOutput( pair, TransportTestDirection::RigToHost, base_time + 1u ).bytes;
    EXPECT_FALSE( traffic.response.empty() );
    EXPECT_EQ( DeliverBytes( pair, pair.Host(), traffic.response ), HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    traffic.confirm = TakeOutput( pair, TransportTestDirection::HostToRig, base_time + 2u ).bytes;
    EXPECT_FALSE( traffic.confirm.empty() );
    EXPECT_EQ( DeliverBytes( pair, pair.Rig(), traffic.confirm ), HIL_TRANSPORT_STATUS_OK );

    traffic.final_ack = TakeOutput( pair, TransportTestDirection::RigToHost, base_time + 3u ).bytes;
    EXPECT_FALSE( traffic.final_ack.empty() );
    return traffic;
}

void CommitHostResetAndPrepareReplacement( TransportPairHarness& pair, const std::uint32_t now_ms )
{
    ASSERT_EQ( pair.Host().Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOutput( pair, TransportTestDirection::HostToRig, now_ms );
    ASSERT_FALSE( reset.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), reset.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    pair.SetBothTimes( now_ms + 1u );
}

void CompleteFreshHostToRigApplication( TransportPairHarness& pair )
{
    const std::vector<std::uint8_t> payload{ 0x71u, 0x72u, 0x73u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto application = TakeOutput( pair, TransportTestDirection::HostToRig, 500u );
    ASSERT_FALSE( application.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), application.bytes ), HIL_TRANSPORT_STATUS_OK );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
    const auto ack = TakeOutput( pair, TransportTestDirection::RigToHost, 501u );
    ASSERT_FALSE( ack.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), ack.bytes ), HIL_TRANSPORT_STATUS_OK );
}

TEST( TransportIntegrationHandshakeDuplicates,
      JoinedDuplicateInitiatesAreCoalescedBeforeResponsePublication )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOutput( pair, TransportTestDirection::HostToRig, 10u );
    ASSERT_FALSE( initiate.bytes.empty() );

    std::vector<std::uint8_t> joined = initiate.bytes;
    joined.insert( joined.end(), initiate.bytes.begin(), initiate.bytes.end() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), joined ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response = TakeOutput( pair, TransportTestDirection::RigToHost, 11u );
    ASSERT_FALSE( response.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), response.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto confirm = TakeOutput( pair, TransportTestDirection::HostToRig, 12u );
    ASSERT_FALSE( confirm.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), confirm.bytes ), HIL_TRANSPORT_STATUS_OK );
    const auto ack = TakeOutput( pair, TransportTestDirection::RigToHost, 13u );
    ASSERT_FALSE( ack.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), ack.bytes ), HIL_TRANSPORT_STATUS_OK );

    ExpectExactlyOneEstablishment( pair.Host() );
    ExpectExactlyOneEstablishment( pair.Rig() );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DuplicateInitiateReplaysCommittedResponseWithoutRestartingAttempt )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( initiate.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate.bytes ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto lost_response = TakeOutput( pair, TransportTestDirection::RigToHost, 21u );
    ASSERT_FALSE( lost_response.bytes.empty() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 0u );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.reliable_delivery_pending, 1u );

    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto replayed_response = TakeOutput( pair, TransportTestDirection::RigToHost, 22u );
    ASSERT_EQ( replayed_response.bytes, lost_response.bytes );

    ASSERT_EQ( DeliverBytes( pair, pair.Host(), replayed_response.bytes ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto confirm = TakeOutput( pair, TransportTestDirection::HostToRig, 23u );
    ASSERT_FALSE( confirm.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), confirm.bytes ), HIL_TRANSPORT_STATUS_OK );
    const auto ack = TakeOutput( pair, TransportTestDirection::RigToHost, 24u );
    ASSERT_FALSE( ack.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), ack.bytes ), HIL_TRANSPORT_STATUS_OK );

    ExpectExactlyOneEstablishment( pair.Host() );
    ExpectExactlyOneEstablishment( pair.Rig() );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DuplicateResponseReplaysConfirmWithoutRestartingAttempt )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response = TakeOutput( pair, TransportTestDirection::RigToHost, 21u );
    ASSERT_FALSE( response.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), response.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto first_confirm = TakeOutput( pair, TransportTestDirection::HostToRig, 22u );
    ASSERT_FALSE( first_confirm.bytes.empty() );

    ASSERT_EQ( DeliverBytes( pair, pair.Host(), response.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto replayed_confirm = TakeOutput( pair, TransportTestDirection::HostToRig, 23u );
    ASSERT_EQ( replayed_confirm.bytes, first_confirm.bytes );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), replayed_confirm.bytes ), HIL_TRANSPORT_STATUS_OK );
    const auto ack = TakeOutput( pair, TransportTestDirection::RigToHost, 24u );
    ASSERT_FALSE( ack.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), ack.bytes ), HIL_TRANSPORT_STATUS_OK );

    ExpectExactlyOneEstablishment( pair.Host() );
    ExpectExactlyOneEstablishment( pair.Rig() );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DuplicateConfirmReissuesFinalAckWithoutDuplicateEstablishment )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto traffic = AdvanceThroughFinalAck( pair, 30u );
    ASSERT_FALSE( traffic.final_ack.empty() );

    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), traffic.confirm ), HIL_TRANSPORT_STATUS_OK );
    const auto replacement_ack = TakeOutput( pair, TransportTestDirection::RigToHost, 34u );
    ASSERT_EQ( replacement_ack.bytes, traffic.final_ack );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), replacement_ack.bytes ), HIL_TRANSPORT_STATUS_OK );

    ExpectExactlyOneEstablishment( pair.Host() );
    ExpectExactlyOneEstablishment( pair.Rig() );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DuplicateFinalAckDoesNotPublishSecondHostEstablishment )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto traffic = AdvanceThroughFinalAck( pair, 40u );
    ASSERT_FALSE( traffic.final_ack.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), traffic.final_ack ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), traffic.final_ack ), HIL_TRANSPORT_STATUS_OK );

    const auto host_status = pair.Host().GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.snapshot.output_pending, 0u );
    EXPECT_EQ( host_status.snapshot.reliable_delivery_pending, 0u );

    const auto host_events = pair.Host().DrainEvents();
    ASSERT_EQ( host_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 0u );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );
    ExpectExactlyOneEstablishment( pair.Rig() );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DelayedResponseFromAbandonedAttemptDoesNotDisruptReplacementHandshake )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate_a = TakeOutput( pair, TransportTestDirection::HostToRig, 50u );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate_a.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response_a = TakeOutput( pair, TransportTestDirection::RigToHost, 51u );
    ASSERT_FALSE( response_a.bytes.empty() );

    CommitHostResetAndPrepareReplacement( pair, 60u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate_b = TakeOutput( pair, TransportTestDirection::HostToRig, 61u );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate_b.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response_b = TakeOutput( pair, TransportTestDirection::RigToHost, 62u );

    ASSERT_EQ( DeliverBytes( pair, pair.Host(), response_a.bytes ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( DeliverBytes( pair, pair.Host(), response_b.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto confirm_b = TakeOutput( pair, TransportTestDirection::HostToRig, 63u );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), confirm_b.bytes ), HIL_TRANSPORT_STATUS_OK );
    const auto ack_b = TakeOutput( pair, TransportTestDirection::RigToHost, 64u );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), ack_b.bytes ), HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    pair.Host().DrainEvents();
    pair.Rig().DrainEvents();
    CompleteFreshHostToRigApplication( pair );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DelayedConfirmFromPreviousSessionDoesNotDisruptEstablishedReplacement )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate_a = TakeOutput( pair, TransportTestDirection::HostToRig, 70u );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate_a.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response_a = TakeOutput( pair, TransportTestDirection::RigToHost, 71u );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), response_a.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto confirm_a = TakeOutput( pair, TransportTestDirection::HostToRig, 72u );
    ASSERT_FALSE( confirm_a.bytes.empty() );

    CommitHostResetAndPrepareReplacement( pair, 80u );
    const auto replacement = pair.EstablishCleanSession();
    ASSERT_EQ( replacement.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( replacement.transport_status.has_value() );
    ASSERT_EQ( *replacement.transport_status, HIL_TRANSPORT_STATUS_OK );
    pair.Host().DrainEvents();
    pair.Rig().DrainEvents();

    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), confirm_a.bytes ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 0u );
    const auto events = pair.Rig().DrainEvents();
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 0u );
    CompleteFreshHostToRigApplication( pair );
}

TEST( TransportIntegrationHandshakeDuplicates,
      DelayedFinalAckFromPreviousAttemptCannotCompleteNewConfirm )
{
    TransportPairHarness pair{};
    InitializePair( pair );
    const auto traffic_a = AdvanceThroughFinalAck( pair, 90u );
    ASSERT_FALSE( traffic_a.final_ack.empty() );

    CommitHostResetAndPrepareReplacement( pair, 100u );
    const auto traffic_b = AdvanceThroughFinalAck( pair, 110u );
    ASSERT_FALSE( traffic_b.final_ack.empty() );
    ASSERT_NE( traffic_b.final_ack, traffic_a.final_ack );

    ASSERT_EQ( DeliverBytes( pair, pair.Host(), traffic_a.final_ack ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), traffic_b.final_ack ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

}  // namespace
