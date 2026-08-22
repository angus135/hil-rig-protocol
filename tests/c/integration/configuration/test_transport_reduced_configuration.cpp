#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

void AssertInitialized( const hil_rig_protocol::test::TransportPairInitializationResult& result )
{
    ASSERT_EQ( result.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( result.host_status.has_value() );
    ASSERT_TRUE( result.rig_status.has_value() );
    ASSERT_EQ( *result.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *result.rig_status, HIL_TRANSPORT_STATUS_OK );
}

void TransferExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( transfer.delivery.bytes_consumed, transfer.delivery.bytes_offered );
}

void EstablishAndDrain( TransportPairHarness& pair )
{
    const auto established = pair.EstablishCleanSession();
    ASSERT_EQ( established.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( established.transport_status.has_value() );
    ASSERT_EQ( *established.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void DeliverHostApplication( TransportPairHarness& pair, const std::vector<std::uint8_t>& payload )
{
    ASSERT_EQ( pair.Host().SubmitApplication( payload ), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, payload );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    const auto confirmed = pair.Host().ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}

std::size_t FindMinimumEncodedCapacity( const std::size_t application_capacity )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.max_application_message_size = application_capacity;

    for ( std::size_t encoded = 1u; encoded <= HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;
          ++encoded )
    {
        config.max_encoded_frame_size = encoded;
        std::size_t required_size     = 0u;
        if ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size )
                 == HIL_TRANSPORT_STATUS_OK
             && required_size > 0u )
        {
            return encoded;
        }
    }
    return 0u;
}

std::size_t CountEvent( const std::vector<HIL_Transport_Event_T>& events,
                        const HIL_Transport_Event_Type_T          type )
{
    return static_cast<std::size_t>(
        std::count_if( events.begin(), events.end(),
                       [type]( const auto& event ) { return event.type == type; } ) );
}

}  // namespace

TEST( TransportIntegrationReducedConfiguration,
      MinimumPublicConfigurationSizesAndTransfersOneByteEndToEnd )
{
    const std::size_t minimum_encoded = FindMinimumEncodedCapacity( 1u );
    ASSERT_GT( minimum_encoded, 1u );

    HIL_Transport_Config_T valid{};
    HIL_TRANSPORT_Default_Config( &valid );
    valid.max_application_message_size = 1u;
    valid.max_encoded_frame_size       = minimum_encoded;
    valid.session_seed                 = 1u;

    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &valid, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( required_size, 0u );

    HIL_Transport_Config_T too_small = valid;
    --too_small.max_encoded_frame_size;
    std::size_t invalid_required_size = 123u;
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &too_small, &invalid_required_size ),
               HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );
    EXPECT_EQ( invalid_required_size, 0u );

    // A failed first initialization must leave the public context reusable
    // without re-zeroing it.
    HIL_Transport_Context_T context{};
    const std::size_t       elements =
        ( required_size + sizeof( std::max_align_t ) - 1u ) / sizeof( std::max_align_t );
    std::vector<std::max_align_t> workspace( elements );
    HIL_Transport_Storage_T       storage{};
    storage.workspace      = reinterpret_cast<std::uint8_t*>( workspace.data() );
    storage.workspace_size = required_size;
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &too_small, &storage ),
               HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &valid, &storage ),
               HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Config_T zero_application       = valid;
    zero_application.max_application_message_size = 0u;
    invalid_required_size                         = 123u;
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &zero_application, &invalid_required_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( invalid_required_size, 0u );

    TransportTestEndpointConfig host_config  = TransportTestEndpointConfig::Host( 1u, 0u );
    host_config.max_application_message_size = 1u;
    host_config.max_encoded_frame_size       = minimum_encoded;
    TransportTestEndpointConfig rig_config   = TransportTestEndpointConfig::Rig( 0u );
    rig_config.max_application_message_size  = 1u;
    rig_config.max_encoded_frame_size        = minimum_encoded;

    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected( host_config, rig_config ) );
    EstablishAndDrain( pair );
    DeliverHostApplication( pair, { 0xA5u } );
}

TEST( TransportIntegrationReducedConfiguration,
      ZeroHandshakeRetriesExhaustAfterInitialTransmissionAndRecover )
{
    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0x8123456789ABCDEF ), 100u, 5u, 0u ),
        TransportTestEndpointConfig::Rig( 300u, 5u, 0u ), 90u ) );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );

    pair.SetHostTime( 100u );
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto initial = pair.Link().AcceptOutput( pair.Host(), pair.HostNow() );
    ASSERT_EQ( initial.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initial.transport_status.has_value() );
    ASSERT_EQ( *initial.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( initial.handle.has_value() );
    TransportTestOutputItem dropped_initiate{};
    ASSERT_TRUE( pair.Link().TakeAccepted( *initial.handle, dropped_initiate ) );
    ASSERT_FALSE( dropped_initiate.bytes.empty() );

    pair.SetHostTime( 104u );
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.output_pending, 0u );
    EXPECT_EQ( pair.Host().GetStatus().snapshot.reliable_delivery_pending, 1u );

    pair.SetHostTime( 105u );
    EXPECT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto exhausted = pair.Host().GetStatus();
    ASSERT_EQ( exhausted.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( exhausted.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( exhausted.snapshot.last_failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ( exhausted.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( exhausted.snapshot.output_pending, 1u );

    const auto reset = pair.Host().PeekOutput();
    ASSERT_EQ( reset.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_NE( reset.bytes, dropped_initiate.bytes );
    const auto events = pair.Host().DrainEvents();
    ASSERT_EQ( events.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_SESSION_RESET ), 1u );
    EXPECT_EQ( CountEvent( events.events, HIL_TRANSPORT_EVENT_DELIVERY_FAILED ), 0u );
    ASSERT_EQ( pair.Host().CommitOutput( 106u ), HIL_TRANSPORT_STATUS_OK );

    pair.SetBothTimes( 110u );
    EstablishAndDrain( pair );
    DeliverHostApplication( pair, { 0x31u } );
}

TEST( TransportIntegrationReducedConfiguration,
      SessionSeedBoundariesRejectReservedValuesAndWrapReplacementIdentity )
{
    TransportPairHarness lowest{};
    AssertInitialized( lowest.InitializeConnected( TransportTestEndpointConfig::Host( 1u, 10u ),
                                                   TransportTestEndpointConfig::Rig( 20u ) ) );
    EstablishAndDrain( lowest );
    DeliverHostApplication( lowest, { 0x41u } );

    TransportPairHarness highest{};
    AssertInitialized( highest.InitializeConnected(
        TransportTestEndpointConfig::Host( HIL_TRANSPORT_SESSION_SEED_RESERVED - 1u, 30u ),
        TransportTestEndpointConfig::Rig( 40u ) ) );
    ASSERT_EQ( highest.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( highest.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ( highest.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto highest_initiate = highest.Host().PeekOutput();
    ASSERT_EQ( highest_initiate.status, HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( highest, TransportTestDirection::HostToRig );
    ASSERT_EQ( highest.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( highest, TransportTestDirection::RigToHost );
    ASSERT_EQ( highest.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( highest, TransportTestDirection::HostToRig );
    TransferExpectOk( highest, TransportTestDirection::RigToHost );
    ASSERT_EQ( highest.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    highest.Host().DrainEvents();
    highest.Rig().DrainEvents();

    ASSERT_EQ( highest.Host().Reset(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( highest, TransportTestDirection::HostToRig );
    ASSERT_EQ( highest.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    const auto wrapped_initiate = highest.Host().PeekOutput();
    ASSERT_EQ( wrapped_initiate.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_NE( wrapped_initiate.bytes, highest_initiate.bytes );
    TransferExpectOk( highest, TransportTestDirection::HostToRig );
    ASSERT_EQ( highest.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( highest, TransportTestDirection::RigToHost );
    ASSERT_EQ( highest.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    TransferExpectOk( highest, TransportTestDirection::HostToRig );
    TransferExpectOk( highest, TransportTestDirection::RigToHost );
    EXPECT_EQ( highest.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( highest.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    highest.Host().DrainEvents();
    highest.Rig().DrainEvents();
    DeliverHostApplication( highest, { 0x42u } );

    TransportTestEndpoint invalid_host_zero{};
    EXPECT_EQ( invalid_host_zero.Initialize(
                   TransportTestEndpointConfig::Host( HIL_TRANSPORT_SESSION_SEED_INVALID, 0u ) ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TransportTestEndpoint invalid_host_reserved{};
    EXPECT_EQ( invalid_host_reserved.Initialize(
                   TransportTestEndpointConfig::Host( HIL_TRANSPORT_SESSION_SEED_RESERVED, 0u ) ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TransportTestEndpoint invalid_rig_seed{};
    auto                  rig_with_host_seed = TransportTestEndpointConfig::Rig( 0u );
    rig_with_host_seed.session_seed          = 1u;
    EXPECT_EQ( invalid_rig_seed.Initialize( rig_with_host_seed ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
}

TEST( TransportIntegrationReducedConfiguration,
      ReducedApplicationLimitRejectsPlusOneWithoutOwningWork )
{
    constexpr std::size_t Limit = 7u;
    auto host_config = TransportTestEndpointConfig::Host( UINT64_C( 0x9123456789ABCDEF ), 50u );
    auto rig_config  = TransportTestEndpointConfig::Rig( 60u );
    host_config.max_application_message_size = Limit;
    rig_config.max_application_message_size  = Limit;

    TransportPairHarness pair{};
    AssertInitialized( pair.InitializeConnected( host_config, rig_config ) );
    EstablishAndDrain( pair );

    const std::vector<std::uint8_t> exact{ 0x00u, 0x11u, 0x22u, 0x00u, 0x33u, 0x44u, 0x55u };
    ASSERT_EQ( exact.size(), Limit );
    DeliverHostApplication( pair, exact );

    const std::vector<std::uint8_t> too_large( Limit + 1u, 0xA5u );
    EXPECT_EQ( pair.Host().SubmitApplication( too_large ), HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE );
    const auto rejected = pair.Host().GetStatus();
    ASSERT_EQ( rejected.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rejected.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( rejected.snapshot.output_pending, 0u );
    EXPECT_EQ( pair.Host().PeekOutput().status, HIL_TRANSPORT_STATUS_NOT_READY );

    DeliverHostApplication( pair, { 0x66u } );
}
