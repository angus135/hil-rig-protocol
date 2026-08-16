#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportPairInitializationResult;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpoint;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

void AssertPairInitialized( const TransportPairInitializationResult& initialization )
{
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );
}

void DeliverAcceptedInChunks( TransportPairHarness& pair, TransportTestEndpoint& receiver,
                              const hil_rig_protocol::test::TransportTestOutputHandle handle,
                              const std::size_t                                       chunk_size )
{
    ASSERT_GT( chunk_size, 0u );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( handle ) );

    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( receiver.Role() );
    ASSERT_TRUE( direction.has_value() );

    while ( pair.Link().ReadyByteCount( *direction ) != 0u )
    {
        const auto delivery = pair.Link().DeliverReady( receiver, chunk_size );
        ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
        ASSERT_TRUE( delivery.transport_status.has_value() );
        ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
        ASSERT_GT( delivery.bytes_consumed, 0u );
        ASSERT_LE( delivery.bytes_consumed, delivery.bytes_offered );
    }
}

void TransferPendingInChunks( TransportPairHarness& pair, const TransportTestDirection direction,
                              const std::size_t chunk_size )
{
    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    TransportTestEndpoint& receiver =
        direction == TransportTestDirection::HostToRig ? pair.Rig() : pair.Host();
    const std::uint32_t now =
        direction == TransportTestDirection::HostToRig ? pair.HostNow() : pair.RigNow();

    const auto accepted = pair.Link().AcceptOutput( sender, now );
    ASSERT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_GT( accepted.size, 0u );

    DeliverAcceptedInChunks( pair, receiver, *accepted.handle, chunk_size );
}

void EstablishWithChunkSize( const std::size_t chunk_size, const std::uint32_t host_epoch,
                             const std::uint32_t rig_epoch )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0x71110000 ), 100u ),
                                  TransportTestEndpointConfig::Rig( 700u ), host_epoch, rig_epoch );
    AssertPairInitialized( initialization );

    const std::vector<std::uint8_t> pre_session_payload{ 0x11u, 0x22u };
    EXPECT_EQ( pair.Host().SubmitApplication( pre_session_payload ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Rig().SubmitApplication( pre_session_payload ),
               HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferPendingInChunks( pair, TransportTestDirection::HostToRig, chunk_size );

    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferPendingInChunks( pair, TransportTestDirection::RigToHost, chunk_size );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferPendingInChunks( pair, TransportTestDirection::HostToRig, chunk_size );
    TransferPendingInChunks( pair, TransportTestDirection::RigToHost, chunk_size );

    const auto host_status = pair.Host().GetStatus();
    const auto rig_status  = pair.Rig().GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.snapshot.application_message_pending, 0u );
    EXPECT_EQ( rig_status.snapshot.application_message_pending, 0u );
    EXPECT_EQ( host_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( rig_status.snapshot.reliable_delivery_pending, 0u );

    const auto host_events = pair.Host().DrainEvents();
    const auto rig_events  = pair.Rig().DrainEvents();
    ASSERT_EQ( host_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( rig_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( host_events.events.size(), 2u );
    ASSERT_EQ( rig_events.events.size(), 2u );
    EXPECT_EQ( host_events.events[0].type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED );
    EXPECT_EQ( host_events.events[1].type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
    EXPECT_EQ( rig_events.events[0].type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED );
    EXPECT_EQ( rig_events.events[1].type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
}

}  // namespace

TEST( TransportIntegrationSession, CompleteHandshakeEstablishesBothEndpoints )
{
    EstablishWithChunkSize( HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE, 0u, 0u );
}

TEST( TransportIntegrationSession, CompleteHandshakeSurvivesByteAtATimeDelivery )
{
    EstablishWithChunkSize( 1u, 0u, 0u );
}

TEST( TransportIntegrationSession, CompleteHandshakeDoesNotRequireSharedClockEpoch )
{
    EstablishWithChunkSize( 3u, UINT32_C( 0x10203040 ), UINT32_C( 0x90807060 ) );
}
