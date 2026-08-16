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
        TransportTestEndpointConfig::Host( UINT64_C( 0x92340000 ), 20u, 10u, max_retries ),
        TransportTestEndpointConfig::Rig( 600u, 10u, max_retries ) );
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
    EXPECT_GT( accepted.size, 0u );
    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
}

void DeliverBytes( TransportPairHarness& pair, TransportTestEndpoint& receiver,
                   const std::vector<std::uint8_t>& bytes )
{
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( receiver.Role() );
    ASSERT_TRUE( direction.has_value() );
    pair.Link().InjectReadyBytes( *direction, bytes );
    const auto delivery = pair.Link().DeliverReady( receiver );
    ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivery.bytes_consumed, bytes.size() );
}

void TransferOutput( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

void ExpectDeliveryConfirmedOnly( TransportTestEndpoint& sender )
{
    const auto event = sender.ReadEvent();
    ASSERT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    EXPECT_EQ( sender.ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void RunLostApplicationFrame( const TransportTestDirection direction )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    TransportTestEndpoint& receiver =
        direction == TransportTestDirection::HostToRig ? pair.Rig() : pair.Host();
    const auto ack_direction = direction == TransportTestDirection::HostToRig
                                   ? TransportTestDirection::RigToHost
                                   : TransportTestDirection::HostToRig;

    const std::vector<std::uint8_t> payload{ 0x11u, 0x22u, 0x33u, 0x44u };
    ASSERT_EQ( sender.SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );

    const auto initial = AcceptAndTake( pair, direction, 100u );
    ASSERT_GT( initial.bytes.size(), 0u );

    if ( direction == TransportTestDirection::HostToRig )
    {
        pair.SetHostTime( 109u );
        ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );
        pair.SetHostTime( 110u );
        ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    }
    else
    {
        pair.SetRigTime( 109u );
        ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( pair.Rig().GetStatus().snapshot.output_pending, 0u );
        pair.SetRigTime( 110u );
        ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    }

    const auto retry = AcceptAndTake( pair, direction, 110u );
    ASSERT_GT( retry.bytes.size(), 0u );
    EXPECT_EQ( retry.bytes, initial.bytes );
    DeliverBytes( pair, receiver, retry.bytes );
    TransferOutput( pair, ack_direction );

    const auto read = receiver.ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    EXPECT_EQ( receiver.ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    ExpectDeliveryConfirmedOnly( sender );
    EXPECT_EQ( sender.GetStatus().snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( receiver.GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

void RunLostAcknowledgement( const TransportTestDirection direction )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    TransportTestEndpoint& receiver =
        direction == TransportTestDirection::HostToRig ? pair.Rig() : pair.Host();
    const auto ack_direction = direction == TransportTestDirection::HostToRig
                                   ? TransportTestDirection::RigToHost
                                   : TransportTestDirection::HostToRig;

    const std::vector<std::uint8_t> payload{ 0x51u, 0x52u, 0x53u };
    ASSERT_EQ( sender.SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );

    const auto initial_peek = sender.PeekOutput();
    ASSERT_EQ( initial_peek.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( initial_peek.bytes.size(), 0u );

    if ( direction == TransportTestDirection::HostToRig )
    {
        pair.SetHostTime( 200u );
    }
    else
    {
        pair.SetRigTime( 200u );
    }
    TransferOutput( pair, direction );

    const auto first_read = receiver.ReadApplication();
    ASSERT_EQ( first_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( first_read.bytes, payload );

    const auto lost_ack = AcceptAndTake( pair, ack_direction, 201u );
    ASSERT_GT( lost_ack.bytes.size(), 0u );

    if ( direction == TransportTestDirection::HostToRig )
    {
        pair.SetHostTime( 210u );
        ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    }
    else
    {
        pair.SetRigTime( 210u );
        ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    }

    const auto retry = AcceptAndTake( pair, direction, 210u );
    ASSERT_GT( retry.bytes.size(), 0u );
    EXPECT_EQ( retry.bytes, initial_peek.bytes );
    DeliverBytes( pair, receiver, retry.bytes );

    EXPECT_EQ( receiver.ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    TransferOutput( pair, ack_direction );
    ExpectDeliveryConfirmedOnly( sender );
    EXPECT_EQ( sender.GetStatus().snapshot.reliable_delivery_pending, 0u );
}

void ContinueHandshakeAfterInitiate( TransportPairHarness& pair )
{
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferOutput( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutput( pair, TransportTestDirection::HostToRig );
    TransferOutput( pair, TransportTestDirection::RigToHost );

    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

}  // namespace

TEST( TransportIntegrationReliability,
      LostHostApplicationFrameRetransmitsIdenticalBytesAndDeliversOnce )
{
    RunLostApplicationFrame( TransportTestDirection::HostToRig );
}

TEST( TransportIntegrationReliability,
      LostRigApplicationFrameRetransmitsIdenticalBytesAndDeliversOnce )
{
    RunLostApplicationFrame( TransportTestDirection::RigToHost );
}

TEST( TransportIntegrationReliability, LostHostApplicationAckReacksDuplicateWithoutRedelivery )
{
    RunLostAcknowledgement( TransportTestDirection::HostToRig );
}

TEST( TransportIntegrationReliability, LostRigApplicationAckReacksDuplicateWithoutRedelivery )
{
    RunLostAcknowledgement( TransportTestDirection::RigToHost );
}

TEST( TransportIntegrationReliability, ApplicationRetryExhaustionReportsFailureAndStartsRecovery )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, 1u );

    const std::vector<std::uint8_t> payload{ 0x61u, 0x62u, 0x63u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( AcceptAndTake( pair, TransportTestDirection::HostToRig, 300u ).bytes.size(), 0u );

    pair.SetHostTime( 310u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( AcceptAndTake( pair, TransportTestDirection::HostToRig, 310u ).bytes.size(), 0u );

    pair.SetHostTime( 320u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_DELIVERY_FAILED );

    const auto status = pair.Host().GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( status.snapshot.last_failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ( status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( status.snapshot.output_pending, 1u );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_NOT_READY );

    const auto delivery_failed = pair.Host().ReadEvent();
    const auto session_reset   = pair.Host().ReadEvent();
    ASSERT_EQ( delivery_failed.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( session_reset.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( delivery_failed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_FAILED );
    EXPECT_EQ( delivery_failed.event.status, HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    EXPECT_EQ( delivery_failed.event.failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ( session_reset.event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
    EXPECT_EQ( session_reset.event.failure, HIL_TRANSPORT_FAILURE_DELIVERY );
}

TEST( TransportIntegrationReliability, LostInitiateIsRetriedAndHandshakeCompletes )
{
    TransportPairHarness pair{};
    const auto           initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xA100 ), 10u, 10u, 1u ),
        TransportTestEndpointConfig::Rig( 500u, 10u, 1u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );

    const auto initial = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( initial.bytes.size(), 0u );
    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = AcceptAndTake( pair, TransportTestDirection::HostToRig, 110u );
    EXPECT_EQ( retry.bytes, initial.bytes );
    DeliverBytes( pair, pair.Rig(), retry.bytes );
    ContinueHandshakeAfterInitiate( pair );
}

TEST( TransportIntegrationReliability, LostResponseIsRetriedAndHandshakeCompletes )
{
    TransportPairHarness pair{};
    const auto           initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xA200 ), 10u, 10u, 1u ),
        TransportTestEndpointConfig::Rig( 500u, 10u, 1u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutput( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );

    const auto initial = AcceptAndTake( pair, TransportTestDirection::RigToHost, 100u );
    ASSERT_GT( initial.bytes.size(), 0u );
    pair.SetRigTime( 110u );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = AcceptAndTake( pair, TransportTestDirection::RigToHost, 110u );
    EXPECT_EQ( retry.bytes, initial.bytes );
    DeliverBytes( pair, pair.Host(), retry.bytes );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutput( pair, TransportTestDirection::HostToRig );
    TransferOutput( pair, TransportTestDirection::RigToHost );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

TEST( TransportIntegrationReliability, LostConfirmIsRetriedAndHandshakeCompletes )
{
    TransportPairHarness pair{};
    const auto           initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xA300 ), 10u, 10u, 1u ),
        TransportTestEndpointConfig::Rig( 500u, 10u, 1u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutput( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferOutput( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );

    const auto initial = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( initial.bytes.size(), 0u );
    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = AcceptAndTake( pair, TransportTestDirection::HostToRig, 110u );
    EXPECT_EQ( retry.bytes, initial.bytes );
    DeliverBytes( pair, pair.Rig(), retry.bytes );
    TransferOutput( pair, TransportTestDirection::RigToHost );

    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}
