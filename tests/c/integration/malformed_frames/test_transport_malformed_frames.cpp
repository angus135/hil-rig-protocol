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

void InitializeAndEstablish( TransportPairHarness& pair )
{
    const auto initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xD2340000 ), 50u, 10u, 2u ),
        TransportTestEndpointConfig::Rig( 900u, 10u, 2u ) );
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
    auto&      sender   = direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    const auto accepted = pair.Link().AcceptOutput( sender, now_ms );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( !accepted.transport_status.has_value() || !accepted.handle.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK )
    {
        return {};
    }
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

void ExpectProtocolError( TransportTestEndpoint& endpoint )
{
    const auto event = endpoint.ReadEvent();
    ASSERT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( event.event.failure, HIL_TRANSPORT_FAILURE_PROTOCOL );
}

}  // namespace

TEST( TransportIntegrationMalformedFrames,
      CorruptedApplicationIsRejectedThenOriginalRetryDeliversOnce )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x11u, 0x12u, 0x13u, 0x14u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 100u );
    const auto accepted = pair.Link().AcceptOutput( pair.Host(), 100u );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_GT( accepted.size, 3u );
    ASSERT_TRUE( pair.Link().CorruptAcceptedByte( *accepted.handle, 2u, 0x01u ) );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *accepted.handle ) );
    const auto corrupt_delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( corrupt_delivery.transport_status.has_value() );
    ASSERT_EQ( *corrupt_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    ExpectProtocolError( pair.Rig() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );

    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportIntegrationMalformedFrames,
      CorruptedAckCannotFalselyCompleteDeliveryAndDuplicateIsReacked )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x21u, 0x22u, 0x23u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    pair.SetHostTime( 200u );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    const auto ack = pair.Link().AcceptOutput( pair.Rig(), 201u );
    ASSERT_TRUE( ack.transport_status.has_value() );
    ASSERT_EQ( *ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( ack.handle.has_value() );
    ASSERT_GT( ack.size, 3u );
    ASSERT_TRUE( pair.Link().CorruptAcceptedByte( *ack.handle, 2u, 0x01u ) );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *ack.handle ) );
    const auto corrupt_delivery = pair.Link().DeliverReady( pair.Host() );
    ASSERT_TRUE( corrupt_delivery.transport_status.has_value() );
    ASSERT_EQ( *corrupt_delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );
    ExpectProtocolError( pair.Host() );

    pair.SetHostTime( 210u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );

    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    const auto confirmed = pair.Host().ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

TEST( TransportIntegrationMalformedFrames,
      MalformedCobsThenValidApplicationInSameChunkResynchronizes )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x31u, 0x32u, 0x33u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto valid = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( valid.bytes.size(), 0u );

    const std::vector<std::uint8_t> malformed{ 0x05u, 0x11u, 0x22u, 0x00u };
    pair.Link().InjectReadyBytes( TransportTestDirection::HostToRig, malformed );
    pair.Link().InjectReadyBytes( TransportTestDirection::HostToRig, valid.bytes );
    const auto delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivery.bytes_consumed, delivery.bytes_offered );

    ExpectProtocolError( pair.Rig() );
    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
}

TEST( TransportIntegrationMalformedFrames,
      OversizedBodyThenValidApplicationInSameChunkResynchronizes )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x41u, 0x42u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto valid = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( valid.bytes.size(), 0u );

    std::vector<std::uint8_t> oversized( HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE + 8u, 0x11u );
    oversized.push_back( 0u );
    pair.Link().InjectReadyBytes( TransportTestDirection::HostToRig, oversized );
    pair.Link().InjectReadyBytes( TransportTestDirection::HostToRig, valid.bytes );
    const auto delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivery.bytes_consumed, delivery.bytes_offered );

    ExpectProtocolError( pair.Rig() );
    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
}
