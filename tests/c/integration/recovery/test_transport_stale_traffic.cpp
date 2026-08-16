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

void InitializeAndEstablish( TransportPairHarness& pair, const std::uint64_t session_seed,
                             const std::uint16_t initial_sequence = 100u )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( session_seed, initial_sequence ),
        TransportTestEndpointConfig::Rig( initial_sequence ) );
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

    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
}

void DeliverBytesExpectOk( TransportPairHarness& pair, TransportTestEndpoint& receiver,
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

void TransferOutputExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

void CompleteHostToRigMessage( TransportPairHarness&            pair,
                               const std::vector<std::uint8_t>& payload )
{
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    const auto event = pair.Host().ReadEvent();
    ASSERT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

void ExpectProtocolRecoveryWithoutApplicationExposure( TransportTestEndpoint& endpoint )
{
    const auto status = endpoint.GetStatus();
    ASSERT_EQ( status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( status.snapshot.application_message_pending, 0u );
    EXPECT_EQ( status.snapshot.output_pending, 1u );
    EXPECT_EQ( endpoint.ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );

    const auto first  = endpoint.ReadEvent();
    const auto second = endpoint.ReadEvent();
    ASSERT_EQ( first.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( second.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( first.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( second.event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
}

}  // namespace

TEST( TransportIntegrationStaleTraffic,
      UnrelatedEstablishedSessionIdentityForcesRecoveryWithoutExposure )
{
    TransportPairHarness target{};
    TransportPairHarness foreign{};
    InitializeAndEstablish( target, UINT64_C( 0xE1000001 ), 100u );
    InitializeAndEstablish( foreign, UINT64_C( 0xE2000002 ), 100u );

    const std::vector<std::uint8_t> foreign_payload{ 0x11u, 0x22u, 0x33u };
    ASSERT_EQ( foreign.Host().SubmitApplication( foreign_payload ), HIL_TRANSPORT_STATUS_OK );
    const auto foreign_frame = AcceptAndTake( foreign, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( foreign_frame.bytes.size(), 0u );

    DeliverBytesExpectOk( target, target.Rig(), foreign_frame.bytes );
    ExpectProtocolRecoveryWithoutApplicationExposure( target.Rig() );

    // The recovery output is real public output and must bring the peer out of
    // the now-uncertain old session before replacement establishment proceeds.
    TransferOutputExpectOk( target, TransportTestDirection::RigToHost );
    const auto host_status = target.Host().GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_NE( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    const auto replacement = target.EstablishCleanSession();
    ASSERT_EQ( replacement.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( replacement.transport_status.has_value() );
    ASSERT_EQ( *replacement.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( target.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( target.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    CompleteHostToRigMessage( target, { 0x44u, 0x55u } );
}

TEST( TransportIntegrationStaleTraffic,
      OlderCurrentSessionApplicationAfterNewerDeliveryForcesRecovery )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, UINT64_C( 0xE3000003 ), 200u );

    const std::vector<std::uint8_t> first_payload{ 0x31u, 0x32u };
    ASSERT_EQ( pair.Host().SubmitApplication( first_payload ), HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_frame = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( delayed_old_frame.bytes.size(), 0u );
    DeliverBytesExpectOk( pair, pair.Rig(), delayed_old_frame.bytes );
    ASSERT_EQ( pair.Rig().ReadApplication().bytes, first_payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    CompleteHostToRigMessage( pair, { 0x41u, 0x42u, 0x43u } );

    // The first frame is now older than the exact duplicate sequence. It must
    // not be treated as another legitimate retransmission of the latest frame.
    DeliverBytesExpectOk( pair, pair.Rig(), delayed_old_frame.bytes );
    ExpectProtocolRecoveryWithoutApplicationExposure( pair.Rig() );
}

TEST( TransportIntegrationStaleTraffic, NormalApplicationDeliveryCrossesUint16SequenceWrap )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair, UINT64_C( 0xE4000004 ), UINT16_MAX - 1u );

    CompleteHostToRigMessage( pair, { 0x51u } );
    CompleteHostToRigMessage( pair, { 0x52u, 0x53u } );
    CompleteHostToRigMessage( pair, { 0x54u, 0x55u, 0x56u } );

    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( pair.Host().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
}
