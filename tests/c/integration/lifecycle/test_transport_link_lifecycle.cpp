#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

void InitializeAndEstablish( TransportPairHarness& pair )
{
    const auto initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xF1000001 ), 300u ),
                                  TransportTestEndpointConfig::Rig( 300u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );
    const auto established = pair.EstablishCleanSession();
    ASSERT_EQ( established.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( established.transport_status.has_value() );
    ASSERT_EQ( *established.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void ExpectDisconnectEvents( hil_rig_protocol::test::TransportTestEndpoint& endpoint )
{
    const auto link_event  = endpoint.ReadEvent();
    const auto reset_event = endpoint.ReadEvent();
    ASSERT_EQ( link_event.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( reset_event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( link_event.event.type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED );
    EXPECT_EQ( reset_event.event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
    EXPECT_EQ( reset_event.event.failure, HIL_TRANSPORT_FAILURE_LINK_LOST );
    EXPECT_EQ( endpoint.ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void HardReconnectAndEstablish( TransportPairHarness& pair, const std::uint32_t now_ms )
{
    pair.Link().Clear();
    ASSERT_EQ( pair.Host().NotifyLink( HIL_TRANSPORT_LINK_STATE_DISCONNECTED, now_ms ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().NotifyLink( HIL_TRANSPORT_LINK_STATE_DISCONNECTED, now_ms ),
               HIL_TRANSPORT_STATUS_OK );
    ExpectDisconnectEvents( pair.Host() );
    ExpectDisconnectEvents( pair.Rig() );

    ASSERT_EQ( pair.Host().NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, now_ms + 1u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, now_ms + 1u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    pair.SetBothTimes( now_ms + 1u );
    const auto replacement = pair.EstablishCleanSession();
    ASSERT_EQ( replacement.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( replacement.transport_status.has_value() );
    ASSERT_EQ( *replacement.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void VerifyFreshHostToRigDelivery( TransportPairHarness& pair )
{
    const std::vector<std::uint8_t> payload{ 0x71u, 0x72u, 0x73u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    auto transfer = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    transfer = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

}  // namespace

TEST( TransportIntegrationLinkLifecycle,
      DisconnectWhileWaitingForAckDiscardsUncertainSendAndUnreadReceive )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x11u, 0x12u, 0x13u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 100u );
    const auto delivered = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_TRUE( delivered.delivery.transport_status.has_value() );
    ASSERT_EQ( *delivered.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ASSERT_EQ( pair.Rig().GetStatus().snapshot.application_message_pending, 1u );

    // Do not accept the rig's ACK. Physical loss makes both sides abandon every
    // session-scoped ownership domain, including the unread receive message.
    HardReconnectAndEstablish( pair, 110u );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.application_message_pending, 0u );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    VerifyFreshHostToRigDelivery( pair );
}

TEST( TransportIntegrationLinkLifecycle,
      DisconnectDiscardsPartialIncomingFrameBeforeReplacementSession )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x21u, 0x22u, 0x23u, 0x24u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 200u );
    const auto accepted = pair.Link().AcceptOutput( pair.Host(), pair.HostNow() );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted.handle ) );
    ASSERT_GT( accepted.size, 2u );

    const auto partial = pair.Link().DeliverReady( pair.Rig(), accepted.size / 2u );
    ASSERT_TRUE( partial.transport_status.has_value() );
    ASSERT_EQ( *partial.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( partial.bytes_consumed, 0u );
    ASSERT_GT( pair.Link().ReadyByteCount( TransportTestDirection::HostToRig ), 0u );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.application_message_pending, 0u );

    HardReconnectAndEstablish( pair, 210u );
    VerifyFreshHostToRigDelivery( pair );
}

TEST( TransportIntegrationLinkLifecycle,
      DisconnectDuringHandshakeRestartsInsteadOfResumingOldAttempt )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xF2000002 ), 400u ),
                                  TransportTestEndpointConfig::Rig( 400u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );

    const auto initiate = pair.Link().AcceptOutput( pair.Host(), 10u );
    ASSERT_TRUE( initiate.transport_status.has_value() );
    ASSERT_EQ( *initiate.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( initiate.handle.has_value() );
    ASSERT_TRUE( pair.Link().HoldAccepted( *initiate.handle ) );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    // Clear initial CONNECTED events before asserting the disconnect event pair.
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    HardReconnectAndEstablish( pair, 20u );
    VerifyFreshHostToRigDelivery( pair );
}
