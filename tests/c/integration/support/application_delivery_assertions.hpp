#ifndef HIL_RIG_PROTOCOL_TESTS_APPLICATION_DELIVERY_ASSERTIONS_HPP
#define HIL_RIG_PROTOCOL_TESTS_APPLICATION_DELIVERY_ASSERTIONS_HPP

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace hil_rig_protocol::test {

// Call with ASSERT_NO_FATAL_FAILURE so a failed prerequisite also stops the caller.
inline void TransferOutputExpectOk( TransportPairHarness&        pair,
                                    const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

inline void ExpectDeliveryConfirmed( TransportTestEndpoint& sender )
{
    const auto confirmed = sender.ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    ASSERT_EQ( confirmed.event.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( confirmed.event.failure, HIL_TRANSPORT_FAILURE_NONE );
    ASSERT_EQ( sender.ReadEvent().status, HIL_TRANSPORT_STATUS_NOT_READY );
}

inline void DeliverApplicationAndConfirm( TransportPairHarness&            pair,
                                          const TransportTestDirection     direction,
                                          const std::vector<std::uint8_t>& application_bytes,
                                          std::vector<std::uint8_t>&       delivered_bytes )
{
    TransportTestEndpoint& sender =
        direction == TransportTestDirection::HostToRig ? pair.Host() : pair.Rig();
    TransportTestEndpoint& receiver =
        direction == TransportTestDirection::HostToRig ? pair.Rig() : pair.Host();
    const auto ack_direction = direction == TransportTestDirection::HostToRig
                                   ? TransportTestDirection::RigToHost
                                   : TransportTestDirection::HostToRig;

    ASSERT_EQ( sender.SubmitApplication( application_bytes ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_NO_FATAL_FAILURE( TransferOutputExpectOk( pair, direction ) );

    const auto received = receiver.ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( received.bytes, application_bytes );

    ASSERT_NO_FATAL_FAILURE( TransferOutputExpectOk( pair, ack_direction ) );
    ASSERT_NO_FATAL_FAILURE( ExpectDeliveryConfirmed( sender ) );
    delivered_bytes = received.bytes;
}

}  // namespace hil_rig_protocol::test

#endif
