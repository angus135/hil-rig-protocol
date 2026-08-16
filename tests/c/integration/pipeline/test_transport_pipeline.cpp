#include <algorithm>
#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

}  // namespace

TEST( TransportFacadeApplicationDelivery,
      EstablishedPeersExchangeCrossedMessagesUsingOnlyPublicApi )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xA123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );

    const auto establishment = pair.EstablishCleanSession();
    ASSERT_EQ( establishment.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( establishment.transport_status.has_value() );
    ASSERT_EQ( *establishment.transport_status, HIL_TRANSPORT_STATUS_OK );

    auto& host = pair.Host();
    auto& rig  = pair.Rig();

    const std::array<std::uint8_t, 4u> host_payload{ 1u, 2u, 3u, 4u };
    const std::array<std::uint8_t, 3u> rig_payload{ 9u, 8u, 7u };
    ASSERT_EQ( host.SubmitApplication( host_payload.data(), host_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig.SubmitApplication( rig_payload.data(), rig_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );

    pair.SetBothTimes( 10u );
    auto transfer = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    pair.SetBothTimes( 11u );
    transfer = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    pair.SetBothTimes( 12u );
    transfer = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    pair.SetBothTimes( 13u );
    transfer = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto host_read = host.ReadApplication();
    const auto rig_read  = rig.ReadApplication();
    ASSERT_EQ( host_read.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_read.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_read.bytes.size(), rig_payload.size() );
    EXPECT_EQ( rig_read.bytes.size(), host_payload.size() );
    EXPECT_TRUE( std::equal( rig_payload.begin(), rig_payload.end(), host_read.bytes.begin() ) );
    EXPECT_TRUE( std::equal( host_payload.begin(), host_payload.end(), rig_read.bytes.begin() ) );

    const auto host_status = host.GetStatus();
    const auto rig_status  = rig.GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( rig_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( host_status.snapshot.application_message_pending, 0u );
    EXPECT_EQ( rig_status.snapshot.application_message_pending, 0u );
}
