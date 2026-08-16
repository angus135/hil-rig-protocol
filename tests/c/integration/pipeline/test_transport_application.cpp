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

void InitializeAndEstablish( TransportPairHarness& pair, const std::uint16_t host_sequence = 10u,
                             const std::uint16_t rig_sequence = 500u )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x81230000 ), host_sequence ),
        TransportTestEndpointConfig::Rig( rig_sequence ) );
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

void TransferApplicationAndAck( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto data_transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( data_transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( data_transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( data_transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *data_transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *data_transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto ack_direction = direction == TransportTestDirection::HostToRig
                                   ? TransportTestDirection::RigToHost
                                   : TransportTestDirection::HostToRig;
    const auto ack_transfer  = pair.TransferOneOutput( ack_direction );
    ASSERT_EQ( ack_transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( ack_transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( ack_transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *ack_transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *ack_transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

void ExpectSingleConfirmedEvent( TransportTestEndpoint& endpoint )
{
    const auto event = endpoint.ReadEvent();
    ASSERT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    EXPECT_EQ( event.event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.failure, HIL_TRANSPORT_FAILURE_NONE );
    EXPECT_EQ( endpoint.ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void RunOneWayDelivery( const TransportTestDirection     direction,
                        const std::vector<std::uint8_t>& payload )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    TransportTestEndpoint& receiver =
        direction == TransportTestDirection::HostToRig ? pair.Rig() : pair.Host();

    ASSERT_EQ( sender.SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto pending = sender.GetStatus();
    ASSERT_EQ( pending.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending.snapshot.reliable_delivery_pending, 1u );

    TransferApplicationAndAck( pair, direction );

    const auto read = receiver.ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    EXPECT_EQ( receiver.ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    ExpectSingleConfirmedEvent( sender );

    const auto sender_status   = sender.GetStatus();
    const auto receiver_status = receiver.GetStatus();
    ASSERT_EQ( sender_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( receiver_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( sender_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( receiver_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( sender_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( receiver_status.snapshot.application_message_pending, 0u );
}

}  // namespace

TEST( TransportIntegrationApplication, HostToRigDeliversSmallApplicationExactlyOnce )
{
    RunOneWayDelivery( TransportTestDirection::HostToRig, { 0x10u } );
}

TEST( TransportIntegrationApplication, RigToHostDeliversRepresentativeApplicationExactlyOnce )
{
    RunOneWayDelivery( TransportTestDirection::RigToHost,
                       { 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u } );
}

TEST( TransportIntegrationApplication, MaximumConfiguredApplicationSizeTransfersInEitherDirection )
{
    std::vector<std::uint8_t> maximum( HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE );
    for ( std::size_t index = 0u; index < maximum.size(); ++index )
    {
        maximum[index] = static_cast<std::uint8_t>( index & 0xFFu );
    }

    RunOneWayDelivery( TransportTestDirection::HostToRig, maximum );
    RunOneWayDelivery( TransportTestDirection::RigToHost, maximum );
}

TEST( TransportIntegrationApplication,
      SequentialMessagesAdvanceWithoutCrossTransactionContamination )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, 100u, 1000u );

    const std::vector<std::vector<std::uint8_t>> payloads{
        { 0x31u, 0x32u }, { 0x40u, 0x41u, 0x42u }, { 0x50u }, { 0x60u, 0x61u, 0x62u, 0x63u } };

    for ( std::size_t index = 0u; index < payloads.size(); ++index )
    {
        const bool host_to_rig = ( index % 2u ) == 0u;
        auto&      sender      = host_to_rig ? pair.Host() : pair.Rig();
        auto&      receiver    = host_to_rig ? pair.Rig() : pair.Host();
        const auto direction =
            host_to_rig ? TransportTestDirection::HostToRig : TransportTestDirection::RigToHost;

        ASSERT_EQ( sender.SubmitApplication( payloads[index] ), HIL_TRANSPORT_STATUS_OK );
        TransferApplicationAndAck( pair, direction );

        const auto read = receiver.ReadApplication();
        ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( read.bytes, payloads[index] );
        ExpectSingleConfirmedEvent( sender );
    }
}

TEST( TransportIntegrationApplication, SuccessfulSubmissionCopiesCallerBufferBeforeReturn )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    std::vector<std::uint8_t> source{ 0x71u, 0x72u, 0x73u, 0x74u };
    const auto                original = source;
    ASSERT_EQ( pair.Host().SubmitApplication( source ), HIL_TRANSPORT_STATUS_OK );
    std::fill( source.begin(), source.end(), 0xEEu );

    TransferApplicationAndAck( pair, TransportTestDirection::HostToRig );
    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, original );
}

TEST( TransportIntegrationApplication,
      PinnedReliableOutputRemainsStableWhenPeerTrafficCreatesControlOutput )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> host_payload{ 0x81u, 0x82u, 0x83u };
    const std::vector<std::uint8_t> rig_payload{ 0x91u, 0x92u, 0x93u, 0x94u };
    ASSERT_EQ( pair.Host().SubmitApplication( host_payload ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().SubmitApplication( rig_payload ), HIL_TRANSPORT_STATUS_OK );

    const auto rig_peek_before = pair.Rig().PeekOutput();
    ASSERT_EQ( rig_peek_before.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( rig_peek_before.bytes.size(), 0u );

    const auto host_data = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_EQ( host_data.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( host_data.delivery.transport_status.has_value() );
    ASSERT_EQ( *host_data.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto rig_peek_after = pair.Rig().PeekOutput();
    ASSERT_EQ( rig_peek_after.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_peek_after.bytes, rig_peek_before.bytes );

    const auto rig_reliable = pair.Link().AcceptOutput( pair.Rig(), pair.RigNow() );
    ASSERT_EQ( rig_reliable.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( rig_reliable.transport_status.has_value() );
    ASSERT_EQ( *rig_reliable.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( rig_reliable.handle.has_value() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *rig_reliable.handle ) );
    const auto rig_data_delivery = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( rig_data_delivery.transport_status.has_value() );
    ASSERT_EQ( *rig_data_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto rig_ack = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( rig_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( rig_ack.delivery.transport_status.has_value() );
    ASSERT_EQ( *rig_ack.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto host_ack = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_EQ( host_ack.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( host_ack.delivery.transport_status.has_value() );
    ASSERT_EQ( *host_ack.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto rig_read  = pair.Rig().ReadApplication();
    const auto host_read = pair.Host().ReadApplication();
    ASSERT_EQ( rig_read.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_read.bytes, host_payload );
    EXPECT_EQ( host_read.bytes, rig_payload );
    ExpectSingleConfirmedEvent( pair.Host() );
    ExpectSingleConfirmedEvent( pair.Rig() );
}
