#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <gtest/gtest.h>

/* Public and selected-profile private headers intentionally share this TU. */
#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/common/transport_parser.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"
#include "transport/internal/mvp/transport_session_mvp.h"
#include "transport/internal/mvp/transport_types_mvp.h"

static_assert( HIL_TRANSPORT_SESSION_SEED_INVALID != HIL_TRANSPORT_SESSION_SEED_RESERVED );
static_assert( HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE > 0u );
static_assert( HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE > 0u );
static_assert( HIL_TRANSPORT_WORKSPACE_ALIGNMENT > 0u );
static_assert( ( HIL_TRANSPORT_WORKSPACE_ALIGNMENT & ( HIL_TRANSPORT_WORKSPACE_ALIGNMENT - 1u ) )
               == 0u );
static_assert( HIL_TRANSPORT_WORKSPACE_ALIGNMENT >= alignof( HIL_Transport_Mvp_Root_T ) );
static_assert( std::is_standard_layout_v<HIL_Transport_Event_T> );
static_assert( std::is_standard_layout_v<HIL_Transport_Status_Snapshot_T> );
static_assert( std::is_standard_layout_v<HIL_Transport_Mvp_Root_T> );
static_assert(
    std::is_same_v<decltype( HIL_Transport_Mvp_Root_T::base.role ), HIL_Transport_Role_T> );
static_assert( HIL_TRANSPORT_OPERATING_MODE_NORMAL != HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
static_assert( HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER
               != HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );

constexpr std::array<HIL_Transport_Operating_Mode_T, 3> ValidOperatingModes{
    HIL_TRANSPORT_OPERATING_MODE_NORMAL,
    HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER,
    HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME,
};

TEST( TransportPublicTypes, KeepOperatingModeAndSessionStateSeparate )
{
    HIL_Transport_Operating_Mode_T operating_mode = HIL_TRANSPORT_OPERATING_MODE_NORMAL;
    HIL_Transport_Session_State_T  session_state  = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;

    EXPECT_EQ( operating_mode, HIL_TRANSPORT_OPERATING_MODE_NORMAL );
    EXPECT_EQ( session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( ValidOperatingModes.size(), 3u );
}

TEST( TransportDefaults, InitializeEveryConfigurationFieldDeterministically )
{
    HIL_Transport_Config_T config{};
    config.max_application_message_size = 1u;
    config.max_encoded_frame_size       = 1u;
    config.session_seed                 = UINT64_C( 123 );
    config.initial_reliable_sequence    = 123u;
    config.connection_timeout_ms        = 123u;
    config.retransmit_timeout_ms        = 123u;
    config.max_retries                  = 123u;

    HIL_TRANSPORT_Default_Config( &config );

    EXPECT_EQ( config.max_application_message_size,
               HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE );
    EXPECT_EQ( config.max_encoded_frame_size, HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE );
    EXPECT_EQ( config.session_seed, HIL_TRANSPORT_SESSION_SEED_INVALID );
    EXPECT_EQ( config.initial_reliable_sequence, 0u );
    EXPECT_EQ( config.connection_timeout_ms, 0u );
    EXPECT_EQ( config.retransmit_timeout_ms, 0u );
    EXPECT_EQ( config.max_retries, 0u );

    HIL_TRANSPORT_Default_Config( nullptr );
}

TEST( TransportStorageQuery, DoesNotRequireAnEndpointRoleOrHostSeed )
{
    HIL_Transport_Config_T config{};
    std::size_t            required_size = 99u;

    HIL_TRANSPORT_Default_Config( &config );
    ASSERT_EQ( config.session_seed, HIL_TRANSPORT_SESSION_SEED_INVALID );
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_GT( required_size, 0u );
}

TEST( TransportContextLifecycle, InsufficientFirstInitializationLeavesZeroContext )
{
    HIL_Transport_Context_T                                                      context{};
    HIL_Transport_Config_T                                                       config{};
    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 1024u> workspace{};
    HIL_Transport_Storage_T storage{ workspace.data(), workspace.size() };

    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed = UINT64_C( 0x1234 );

    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( context.implementation, nullptr );
    EXPECT_EQ( context.implementation_size, 0u );
    EXPECT_EQ( context.initialization_cookie, 0u );

    /* A successful context would be reset, never passed to Init again. */
}

TEST( TransportStructuredOutputs, PreserveEventAndClearStatusForInvalidContext )
{
    HIL_Transport_Context_T         context{};
    HIL_Transport_Event_T           event{};
    HIL_Transport_Status_Snapshot_T snapshot{};

    event.type              = HIL_TRANSPORT_EVENT_PROTOCOL_ERROR;
    event.status            = HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    event.failure           = HIL_TRANSPORT_FAILURE_INTERNAL;
    event.required_capacity = 123u;

    snapshot.role                        = HIL_TRANSPORT_ROLE_RIG;
    snapshot.link_state                  = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    snapshot.session_state               = HIL_TRANSPORT_SESSION_STATE_FAULT;
    snapshot.operating_mode              = HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER;
    snapshot.operating_mode_valid        = 1u;
    snapshot.output_pending              = 1u;
    snapshot.application_message_pending = 1u;
    snapshot.event_pending               = 1u;
    snapshot.reliable_delivery_pending   = 1u;
    snapshot.last_failure                = HIL_TRANSPORT_FAILURE_INTERNAL;

    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( event.status, HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    EXPECT_EQ( event.required_capacity, 123u );
    EXPECT_EQ( snapshot.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( snapshot.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( snapshot.operating_mode, HIL_TRANSPORT_OPERATING_MODE_NORMAL );
    EXPECT_EQ( snapshot.operating_mode_valid, 0u );
    EXPECT_EQ( snapshot.output_pending, 0u );
    EXPECT_EQ( snapshot.application_message_pending, 0u );
    EXPECT_EQ( snapshot.event_pending, 0u );
    EXPECT_EQ( snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( snapshot.last_failure, HIL_TRANSPORT_FAILURE_NONE );
}

TEST( TransportSessionInitialization, ValidatesWithoutPartiallyMutatingDestination )
{
    HIL_Transport_Mvp_Session_T session;
    std::memset( &session, 0xA5, sizeof( session ) );
    const HIL_Transport_Mvp_Session_T original = session;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Init( nullptr, HIL_TRANSPORT_ROLE_HOST, 1u, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Init(
                   &session, static_cast<HIL_Transport_Role_T>( 99 ), 1u, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Init(
                   &session, HIL_TRANSPORT_ROLE_HOST, HIL_TRANSPORT_SESSION_SEED_INVALID, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Init(
                   &session, HIL_TRANSPORT_ROLE_HOST, HIL_TRANSPORT_SESSION_SEED_RESERVED, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Init( &session, HIL_TRANSPORT_ROLE_RIG, 1u, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( std::memcmp( &session, &original, sizeof( session ) ), 0 );
}

TEST( TransportSessionInitialization, InitializesHostAndRigFieldsDeterministically )
{
    HIL_Transport_Mvp_Session_T host;
    std::memset( &host, 0xA5, sizeof( host ) );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Init( &host, HIL_TRANSPORT_ROLE_HOST, 7u, UINT16_MAX ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( host.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( host.link_state_observed, 0u );
    EXPECT_EQ( host.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( host.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
    EXPECT_EQ( host.session_identifier, 0u );
    EXPECT_EQ( host.session_identifier_valid, 0u );
    EXPECT_EQ( host.next_host_session_identifier, 7u );
    EXPECT_EQ( host.initial_reliable_sequence, UINT16_MAX );
    EXPECT_EQ( host.next_transmit_sequence, UINT16_MAX );
    EXPECT_EQ( host.expected_receive_sequence, UINT16_MAX );
    EXPECT_EQ( host.retained_transmit_sequence, 0u );
    EXPECT_EQ( host.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( host.last_accepted_receive_sequence, 0u );
    EXPECT_EQ( host.accepted_receive_sequence_valid, 0u );
    EXPECT_EQ( host.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( host.retransmissions_committed, 0u );
    EXPECT_EQ( host.reliable_last_committed_ms, 0u );
    EXPECT_EQ( host.last_valid_receive_ms, 0u );
    EXPECT_EQ( host.last_failure, HIL_TRANSPORT_FAILURE_NONE );

    HIL_Transport_Mvp_Session_T rig;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Init(
                   &rig, HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 0u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.next_host_session_identifier, 0u );
    EXPECT_EQ( rig.initial_reliable_sequence, 0u );
    EXPECT_EQ( rig.next_transmit_sequence, 0u );
    EXPECT_EQ( rig.expected_receive_sequence, 0u );
}
