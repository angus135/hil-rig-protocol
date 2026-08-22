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

TransportTestOutputItem TakeOutput( TransportPairHarness&        pair,
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

HIL_Transport_Status_T DeliverBytes( TransportPairHarness& pair, TransportTestEndpoint& receiver,
                                     const std::vector<std::uint8_t>& bytes )
{
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( receiver.Role() );
    EXPECT_TRUE( direction.has_value() );
    if ( !direction.has_value() )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    pair.Link().InjectReadyBytes( *direction, bytes );
    const auto delivery = pair.Link().DeliverReady( receiver );
    EXPECT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( delivery.transport_status.has_value() );
    EXPECT_EQ( delivery.bytes_consumed, bytes.size() );
    return delivery.transport_status.value_or( HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
}

std::size_t CountEvent( const std::vector<HIL_Transport_Event_T>& events,
                        const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(), [type]( const HIL_Transport_Event_T& event ) {
            return event.type == type;
        } ) );
}

}  // namespace

TEST( TransportIntegrationHandshakeDuplicates,
      DuplicateFinalAckDoesNotPublishSecondHostEstablishment )
{
    TransportPairHarness pair{};
    const auto           initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xA102030405060708 ), 100u, 10u, 2u ),
        TransportTestEndpointConfig::Rig( 700u, 10u, 2u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOutput( pair, TransportTestDirection::HostToRig, 40u );
    ASSERT_FALSE( initiate.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), initiate.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    const auto response = TakeOutput( pair, TransportTestDirection::RigToHost, 41u );
    ASSERT_FALSE( response.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), response.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto confirm = TakeOutput( pair, TransportTestDirection::HostToRig, 42u );
    ASSERT_FALSE( confirm.bytes.empty() );
    ASSERT_EQ( DeliverBytes( pair, pair.Rig(), confirm.bytes ), HIL_TRANSPORT_STATUS_OK );
    const auto final_ack = TakeOutput( pair, TransportTestDirection::RigToHost, 43u );
    ASSERT_FALSE( final_ack.bytes.empty() );

    ASSERT_EQ( DeliverBytes( pair, pair.Host(), final_ack.bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( DeliverBytes( pair, pair.Host(), final_ack.bytes ), HIL_TRANSPORT_STATUS_OK );

    const auto host_events = pair.Host().DrainEvents();
    ASSERT_EQ( host_events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED ), 1u );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR ), 0u );
    EXPECT_EQ( CountEvent( host_events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 0u );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}
