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
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xF3000003 ), 500u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
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

void TransferOutputExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

void ReestablishAfterCommittedReset( TransportPairHarness& pair )
{
    const auto replacement = pair.EstablishCleanSession();
    ASSERT_EQ( replacement.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( replacement.transport_status.has_value() );
    ASSERT_EQ( *replacement.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void VerifyFreshDelivery( TransportPairHarness& pair )
{
    const std::vector<std::uint8_t> payload{ 0x61u, 0x62u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    EXPECT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

}  // namespace

TEST( TransportIntegrationReset, ExplicitResetClearsCrossedSessionWorkAndEventsBeforeReplacement )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> host_payload{ 0x11u, 0x12u };
    const std::vector<std::uint8_t> rig_payload{ 0x21u, 0x22u, 0x23u };
    ASSERT_EQ( pair.Host().SubmitApplication( host_payload ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().SubmitApplication( rig_payload ), HIL_TRANSPORT_STATUS_OK );

    // Accept both reliable items before either peer can generate a control ACK.
    // This preserves the genuinely crossed state rather than allowing normal
    // control-first arbitration to complete one sender early.
    const auto host_item = pair.Link().AcceptOutput( pair.Host(), 100u );
    const auto rig_item  = pair.Link().AcceptOutput( pair.Rig(), 100u );
    ASSERT_TRUE( host_item.transport_status.has_value() );
    ASSERT_TRUE( rig_item.transport_status.has_value() );
    ASSERT_EQ( *host_item.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *rig_item.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( host_item.handle.has_value() );
    ASSERT_TRUE( rig_item.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *host_item.handle ) );
    auto delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *rig_item.handle ) );
    delivery = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    // Leave both generated ACKs unaccepted. Host now owns an outbound reliable
    // transaction, an unread inbound Application message, and a pending control
    // ACK at the same time.
    ASSERT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ASSERT_EQ( pair.Host().GetStatus().snapshot.application_message_pending, 1u );
    ASSERT_EQ( pair.Host().GetStatus().snapshot.output_pending, 1u );

    ASSERT_EQ( pair.Host().Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset_status = pair.Host().GetStatus();
    ASSERT_EQ( reset_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( reset_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( reset_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( reset_status.snapshot.application_message_pending, 0u );
    EXPECT_EQ( reset_status.snapshot.event_pending, 0u );
    EXPECT_EQ( reset_status.snapshot.output_pending, 1u );
    EXPECT_EQ( reset_status.snapshot.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( pair.Host().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );

    // Discard pre-reset simulated link traffic. Only the reset output produced by
    // the public Reset call is allowed to synchronize the peer.
    pair.Link().Clear();
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    ReestablishAfterCommittedReset( pair );
    VerifyFreshDelivery( pair );
}

TEST( TransportIntegrationReset, ExplicitResetDiscardsPartialParserStateBeforeFreshSession )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x31u, 0x32u, 0x33u, 0x34u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto accepted = pair.Link().AcceptOutput( pair.Host(), 100u );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted.handle ) );
    const auto partial = pair.Link().DeliverReady( pair.Rig(), accepted.size / 2u );
    ASSERT_TRUE( partial.transport_status.has_value() );
    ASSERT_EQ( *partial.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( pair.Link().ReadyByteCount( TransportTestDirection::HostToRig ), 0u );

    ASSERT_EQ( pair.Rig().Reset(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.application_message_pending, 0u );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.event_pending, 0u );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 1u );

    pair.Link().Clear();
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    ReestablishAfterCommittedReset( pair );
    VerifyFreshDelivery( pair );
}

TEST( TransportIntegrationReset, DroppedBestEffortResetRecoversThroughPhysicalReconnect )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    ASSERT_EQ( pair.Rig().Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = pair.Link().AcceptOutput( pair.Rig(), 100u );
    ASSERT_TRUE( reset.transport_status.has_value() );
    ASSERT_EQ( *reset.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( reset.handle.has_value() );
    ASSERT_TRUE( pair.Link().DropAccepted( *reset.handle ) );

    // The RESET is deliberately best-effort. Once it was accepted by the local
    // caller but lost externally, the rig can wait for replacement establishment
    // while the host still believes the old session is established. No autonomous
    // convergence is required by the MVP.
    pair.SetRigTime( 101u );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_NE( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    // Physical link loss is the documented hard recovery boundary. It discards
    // any uncertain external bytes and forces both endpoints into a new session.
    pair.Link().Clear();
    ASSERT_EQ( pair.Host().NotifyLink( HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 110u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().NotifyLink( HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 110u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Host().NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, 111u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, 111u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    pair.SetBothTimes( 111u );
    ReestablishAfterCommittedReset( pair );
    VerifyFreshDelivery( pair );
}
