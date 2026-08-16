#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;

/**
 * @brief Exercise integration-support ownership primitives without adding a new protocol scenario.
 *
 * @details This test exists only to protect the reusable harness structure. It confirms
 * that a healthy transfer moves the exact newly accepted ordinal even when older traffic
 * is queued, and exercises the opaque hold/release/duplicate/corrupt operations without
 * interpreting or delivering the modified frame. Production protocol behaviour remains
 * covered by the existing integration tests.
 */
TEST( TransportIntegrationHarnessStructure, PreservesAcceptedItemIdentityAndOpaqueLinkOwnership )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xABCD ), 10u ),
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
    auto& link = pair.Link();

    const auto no_input = link.DeliverReady( host );
    EXPECT_EQ( no_input.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_EQ( no_input.transport_status.has_value(), false );

    const std::array<std::uint8_t, 2u> host_payload{ 0x11u, 0x12u };
    const std::array<std::uint8_t, 2u> rig_payload{ 0x21u, 0x22u };
    ASSERT_EQ( host.SubmitApplication( host_payload.data(), host_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig.SubmitApplication( rig_payload.data(), rig_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );

    pair.SetTime( 10u );
    const auto old_host_output = link.AcceptOutput( host, pair.Now() );
    ASSERT_EQ( old_host_output.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( old_host_output.transport_status.has_value() );
    ASSERT_EQ( *old_host_output.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( link.AcceptedItemCount( TransportTestDirection::HostToRig ), 1u );

    pair.SetTime( 11u );
    const auto rig_transfer = pair.TransferOneOutput( TransportTestDirection::RigToHost );
    ASSERT_EQ( rig_transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( rig_transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( rig_transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *rig_transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *rig_transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    const auto output_query = host.QueryOutputSize();
    ASSERT_EQ( output_query.status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    ASSERT_GT( output_query.required_size, 0u );

    pair.SetTime( 12u );
    const auto host_control_transfer = pair.TransferOneOutput( TransportTestDirection::HostToRig );
    ASSERT_EQ( host_control_transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( host_control_transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( host_control_transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *host_control_transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *host_control_transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );

    /* The older host Application output must still be queued, not substituted for the ACK. */
    ASSERT_EQ( link.AcceptedItemCount( TransportTestDirection::HostToRig ), 1u );

    const auto application_query = host.QueryApplicationSize();
    ASSERT_EQ( application_query.status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    ASSERT_EQ( application_query.required_size, rig_payload.size() );
    const auto application_read = host.ReadApplication( application_query.required_size );
    ASSERT_EQ( application_read.status, HIL_TRANSPORT_STATUS_OK );

    ASSERT_TRUE( link.HoldNextAccepted( TransportTestDirection::HostToRig ) );
    EXPECT_EQ( link.AcceptedItemCount( TransportTestDirection::HostToRig ), 0u );
    EXPECT_EQ( link.HeldItemCount( TransportTestDirection::HostToRig ), 1u );
    ASSERT_TRUE( link.ReleaseOldestHeld( TransportTestDirection::HostToRig ) );
    ASSERT_TRUE( link.DuplicateNextAccepted( TransportTestDirection::HostToRig ) );
    EXPECT_EQ( link.AcceptedItemCount( TransportTestDirection::HostToRig ), 2u );
    ASSERT_TRUE( link.CorruptNextAcceptedByte( TransportTestDirection::HostToRig, 0u, 0x01u ) );
    ASSERT_TRUE( link.DropNextAccepted( TransportTestDirection::HostToRig ) );
    ASSERT_TRUE( link.DropNextAccepted( TransportTestDirection::HostToRig ) );
    EXPECT_EQ( link.AcceptedItemCount( TransportTestDirection::HostToRig ), 0u );

    TransportPairHarness invalid_pair{};
    auto invalid_host_config = TransportTestEndpointConfig::Host( UINT64_C( 0xBCDE ), 20u );
    invalid_host_config.role = HIL_TRANSPORT_ROLE_RIG;
    const auto invalid_initialization =
        invalid_pair.Initialize( invalid_host_config, TransportTestEndpointConfig::Rig( 600u ) );
    EXPECT_EQ( invalid_initialization.harness_status,
               TransportTestHarnessStatus::InvalidPairRoles );
    EXPECT_EQ( invalid_initialization.host_status.has_value(), false );
    EXPECT_EQ( invalid_initialization.rig_status.has_value(), false );
}

}  // namespace
