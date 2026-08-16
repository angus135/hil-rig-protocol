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
using hil_rig_protocol::test::TransportTestOutputItem;

void InitializeAndEstablish( TransportPairHarness& pair )
{
    const auto initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xFA000001 ), 700u ),
                                  TransportTestEndpointConfig::Rig( 700u ) );
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

TransportTestOutputItem AcceptAndTake( TransportPairHarness&        pair,
                                       const TransportTestDirection direction,
                                       const std::uint32_t          now_ms )
{
    auto&      sender   = direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    const auto accepted = pair.Link().AcceptOutput( sender, now_ms );
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( !accepted.transport_status.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK || !accepted.handle.has_value() )
    {
        return {};
    }
    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
}

void DeliverBytesExpectOk( TransportPairHarness& pair, const TransportTestDirection direction,
                           hil_rig_protocol::test::TransportTestEndpoint& receiver,
                           const std::vector<std::uint8_t>&               bytes )
{
    pair.Link().InjectReadyBytes( direction, bytes );
    const auto delivered = pair.Link().DeliverReady( receiver );
    ASSERT_EQ( delivered.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( delivered.transport_status.has_value() );
    ASSERT_EQ( *delivered.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivered.bytes_consumed, bytes.size() );
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

TEST( TransportIntegrationPublicOwnership,
      OutputSizeQueriesAndUndersizedPeekDoNotPinReliableAheadOfNewControl )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> host_payload{ 0x11u, 0x12u, 0x13u };
    const std::vector<std::uint8_t> rig_payload{ 0x21u, 0x22u };
    ASSERT_EQ( pair.Host().SubmitApplication( host_payload ), HIL_TRANSPORT_STATUS_OK );

    const auto size_query = pair.Host().QueryOutputSize();
    ASSERT_EQ( size_query.status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    ASSERT_GT( size_query.required_size, 1u );
    EXPECT_TRUE( size_query.bytes.empty() );

    const auto undersized = pair.Host().PeekOutput( size_query.required_size - 1u );
    ASSERT_EQ( undersized.status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( undersized.required_size, size_query.required_size );

    // Create a peer reliable frame only after both unsuccessful host peeks. The
    // resulting ACK should gain normal control priority if neither query pinned
    // the earlier host reliable item.
    ASSERT_EQ( pair.Rig().SubmitApplication( rig_payload ), HIL_TRANSPORT_STATUS_OK );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.Host().GetStatus().snapshot.application_message_pending, 1u );

    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.reliable_delivery_pending, 0u );
    const auto rig_confirmed = pair.Rig().ReadEvent();
    ASSERT_EQ( rig_confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    // The original host reliable item remains intact behind that ACK and can now
    // complete normally.
    TransferOutputExpectOk( pair, TransportTestDirection::HostToRig );
    const auto host_received = pair.Rig().ReadApplication();
    ASSERT_EQ( host_received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_received.bytes, host_payload );
    const auto rig_received = pair.Host().ReadApplication();
    ASSERT_EQ( rig_received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_received.bytes, rig_payload );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    EXPECT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

TEST( TransportIntegrationPublicOwnership,
      ApplicationSizeAndUndersizedReadsPreserveMessageAcrossPeerRetry )
{
    TransportPairHarness pair{};
    InitializeAndEstablish( pair );

    const std::vector<std::uint8_t> payload{ 0x31u, 0x32u, 0x33u, 0x34u, 0x35u };
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    const auto original = AcceptAndTake( pair, TransportTestDirection::HostToRig, 100u );
    ASSERT_GT( original.bytes.size(), 0u );
    DeliverBytesExpectOk( pair, TransportTestDirection::HostToRig, pair.Rig(), original.bytes );

    const auto query = pair.Rig().QueryApplicationSize();
    ASSERT_EQ( query.status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( query.required_size, payload.size() );
    const auto undersized = pair.Rig().ReadApplication( payload.size() - 1u );
    ASSERT_EQ( undersized.status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( undersized.required_size, payload.size() );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.application_message_pending, 1u );

    // Lose the first ACK. The sender retries while the receiver's one Application
    // slot is still occupied by the unread original message.
    const auto lost_ack = AcceptAndTake( pair, TransportTestDirection::RigToHost, 101u );
    ASSERT_GT( lost_ack.bytes.size(), 0u );
    pair.SetHostTime( 110u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto retry = AcceptAndTake( pair, TransportTestDirection::HostToRig, 110u );
    ASSERT_EQ( retry.bytes, original.bytes );
    DeliverBytesExpectOk( pair, TransportTestDirection::HostToRig, pair.Rig(), retry.bytes );

    // The duplicate is re-ACKed but never replaces or duplicates the unread
    // Application message preserved through both unsuccessful read attempts.
    const auto full_read = pair.Rig().ReadApplication();
    ASSERT_EQ( full_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( full_read.bytes, payload );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    TransferOutputExpectOk( pair, TransportTestDirection::RigToHost );
    const auto confirmed = pair.Host().ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}
