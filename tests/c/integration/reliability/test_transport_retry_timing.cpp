#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

void InitializeAndEstablish( TransportPairHarness& pair, const std::uint32_t timeout_ms = 10u,
                             const std::uint8_t  max_retries = 2u,
                             const std::uint32_t host_epoch  = 0u,
                             const std::uint32_t rig_epoch   = 0u )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xB2340000 ), 30u, timeout_ms, max_retries ),
        TransportTestEndpointConfig::Rig( 700u, timeout_ms, max_retries ), host_epoch, rig_epoch );
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

}  // namespace

TEST( TransportIntegrationRetryTiming, PeekDoesNotStartTimerAndCommitStartsIt )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x10u, 0x20u, 0x30u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );

    const auto initial = pair.Host().PeekOutput();
    ASSERT_EQ( initial.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( initial.bytes.size(), 0u );

    pair.SetHostTime( 500u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto still_pinned = pair.Host().PeekOutput();
    ASSERT_EQ( still_pinned.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( still_pinned.bytes, initial.bytes );

    ASSERT_EQ( pair.Host().CommitOutput( 500u ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 509u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );

    pair.SetHostTime( 510u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 1u );
    const auto retry = pair.Host().PeekOutput();
    ASSERT_EQ( retry.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( retry.bytes, initial.bytes );
}

TEST( TransportIntegrationRetryTiming, AckArrivingWhileRetryReadyCancelsUnpinnedRetry )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x41u, 0x42u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 100u );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().GetStatus().snapshot.output_pending, 1u );

    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    const auto status = pair.Host().GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.output_pending, 0u );
    EXPECT_EQ( status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    const auto event = pair.Host().ReadEvent();
    ASSERT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

TEST( TransportIntegrationRetryTiming, AckForPeekedRetryIsRetainedUntilCallerCommitsPinnedBytes )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x51u, 0x52u, 0x53u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 100u );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = pair.Host().PeekOutput();
    ASSERT_EQ( retry.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( retry.bytes.size(), 0u );

    const auto ack_transfer = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( ack_transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( ack_transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( ack_transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *ack_transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( *ack_transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( ack_transfer.delivery.bytes_consumed, ack_transfer.delivery.bytes_offered );

    const auto blocked = pair.Host().GetStatus();
    ASSERT_EQ( blocked.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( blocked.snapshot.reliable_delivery_pending, 1u );
    EXPECT_EQ( blocked.snapshot.output_pending, 1u );

    ASSERT_EQ( pair.Host().CommitOutput( 111u ), HIL_TRANSPORT_STATUS_OK );
    const auto retry_retained_input = pair.Link().DeliverZeroLength( pair.Host() );
    ASSERT_EQ( retry_retained_input.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( retry_retained_input.transport_status.has_value() );
    ASSERT_EQ( *retry_retained_input.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( retry_retained_input.bytes_consumed, 0u );

    const auto completed = pair.Host().GetStatus();
    ASSERT_EQ( completed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( completed.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( completed.snapshot.output_pending, 0u );
    const auto event = pair.Host().ReadEvent();
    ASSERT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

TEST( TransportIntegrationRetryTiming,
      RetransmissionTimeoutWorksAcrossUint32WrapWithIndependentPeerClock )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, 10u, 2u, UINT32_MAX - 100u, 123456u );

    const std::vector<std::uint8_t> payload{ 0x61u, 0x62u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );

    pair.SetHostTime( UINT32_MAX - 5u );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    pair.SetHostTime( 3u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );

    pair.SetHostTime( 4u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 1u );

    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

TEST( TransportIntegrationRetryTiming, ZeroRetransmissionTimeoutDisablesTimerDrivenRetryAndFailure )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, 0u, 2u );

    const std::vector<std::uint8_t> payload{ 0x71u, 0x72u, 0x73u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto output = pair.Host().PeekOutput();
    ASSERT_EQ( output.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( output.bytes.size(), 0u );
    ASSERT_EQ( pair.Host().CommitOutput( 100u ), HIL_TRANSPORT_STATUS_OK );

    pair.SetHostTime( UINT32_MAX );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto status = pair.Host().GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( status.snapshot.reliable_delivery_pending, 1u );
    EXPECT_EQ( status.snapshot.output_pending, 0u );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}
