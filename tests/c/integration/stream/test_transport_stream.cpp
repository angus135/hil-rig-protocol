#include <algorithm>
#include <array>
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
    const auto initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xC2340000 ), 40u ),
                                  TransportTestEndpointConfig::Rig( 800u ) );
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
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
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

void DeliverAcceptedWithPattern( TransportPairHarness& pair, TransportTestEndpoint& receiver,
                                 const hil_rig_protocol::test::TransportTestOutputHandle handle,
                                 const std::vector<std::size_t>&                         chunks )
{
    ASSERT_FALSE( chunks.empty() );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( handle ) );
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( receiver.Role() );
    ASSERT_TRUE( direction.has_value() );

    std::size_t chunk_index = 0u;
    while ( pair.Link().ReadyByteCount( *direction ) != 0u )
    {
        const std::size_t chunk = chunks[chunk_index % chunks.size()];
        ASSERT_GT( chunk, 0u );
        const auto delivery = pair.Link().DeliverReady( receiver, chunk );
        ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
        ASSERT_TRUE( delivery.transport_status.has_value() );
        ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
        ASSERT_GT( delivery.bytes_consumed, 0u );
        ASSERT_EQ( delivery.bytes_consumed, delivery.bytes_offered );
        ++chunk_index;
    }
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

TEST( TransportIntegrationStream, ApplicationFrameCanArriveOneByteAtATime )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x11u, 0x22u, 0x33u, 0x44u, 0x55u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto accepted = pair.Link().AcceptOutput( pair.Host(), 100u );
    ASSERT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );

    DeliverAcceptedWithPattern( pair, pair.Rig(), *accepted.handle, { 1u } );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );

    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
}

TEST( TransportIntegrationStream, ApplicationFrameSurvivesDeterministicIrregularChunking )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    std::vector<std::uint8_t> payload( 173u );
    for ( std::size_t index = 0u; index < payload.size(); ++index )
    {
        payload[index] = static_cast<std::uint8_t>( ( index * 17u ) & 0xFFu );
    }
    ASSERT_EQ( pair.Rig().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto accepted = pair.Link().AcceptOutput( pair.Rig(), 100u );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( accepted.handle.has_value() );

    DeliverAcceptedWithPattern( pair, pair.Host(), *accepted.handle, { 2u, 7u, 3u, 11u, 1u } );
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    const auto read = pair.Host().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
}

TEST( TransportIntegrationStream, AckAndCrossedApplicationCanBeJoinedIntoOneReceiveCall )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> host_payload{ 0x31u, 0x32u };
    const std::vector<std::uint8_t> rig_payload{ 0x41u, 0x42u, 0x43u };
    ASSERT_EQ( pair.Host().SubmitApplication( host_payload ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Rig().SubmitApplication( rig_payload ), HIL_TRANSPORT_STATUS_OK );

    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );

    const auto rig_ack = pair.Link().AcceptOutput( pair.Rig(), 101u );
    ASSERT_TRUE( rig_ack.transport_status.has_value() );
    ASSERT_EQ( *rig_ack.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( rig_ack.handle.has_value() );
    const auto rig_data = pair.Link().AcceptOutput( pair.Rig(), 102u );
    ASSERT_TRUE( rig_data.transport_status.has_value() );
    ASSERT_EQ( *rig_data.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( rig_data.handle.has_value() );

    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *rig_ack.handle ) );
    ASSERT_TRUE( pair.Link().QueueAcceptedForDelivery( *rig_data.handle ) );
    const auto joined = pair.Link().DeliverReady( pair.Host() );
    ASSERT_EQ( joined.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( joined.transport_status.has_value() );
    ASSERT_EQ( *joined.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( joined.bytes_consumed, joined.bytes_offered );

    const auto host_status = pair.Host().GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( host_status.snapshot.application_message_pending, 1u );
    EXPECT_EQ( host_status.snapshot.output_pending, 1u );

    const auto host_read = pair.Host().ReadApplication();
    const auto rig_read  = pair.Rig().ReadApplication();
    ASSERT_EQ( host_read.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_read.bytes, rig_payload );
    EXPECT_EQ( rig_read.bytes, host_payload );

    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.reliable_delivery_pending, 0u );
}

TEST( TransportIntegrationStream, LeadingAndRepeatedDelimitersDoNotChangeValidApplicationOutcome )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x71u, 0x72u, 0x73u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto item = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( item.bytes.size(), 0u );

    const auto                         direction = TransportTestDirection::HostToRig;
    const std::array<std::uint8_t, 4u> delimiters{ 0u, 0u, 0u, 0u };
    pair.Link().InjectReadyBytes( direction, delimiters.data(), delimiters.size() );
    pair.Link().InjectReadyBytes( direction, item.bytes );
    const auto delivery = pair.Link().DeliverReady( pair.Rig() );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivery.bytes_consumed, delivery.bytes_offered );

    const auto read = pair.Rig().ReadApplication();
    ASSERT_EQ( read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( read.bytes, payload );
    EXPECT_EQ( pair.Rig().ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
}
