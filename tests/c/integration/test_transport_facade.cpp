#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/transport/transport.h"

namespace {
constexpr std::size_t MaxMessage    = HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE;
constexpr std::size_t MaxOutput     = HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;
constexpr std::size_t WorkspaceSize = ( MaxMessage * 3u ) + ( MaxOutput * 3u ) + 1024u;

bool ExternalCommunicationAcceptedFrame( const std::uint8_t* frame, std::size_t frame_size )
{
    return frame != nullptr && frame_size != 0u;
}

void CompileIntendedFacadeWorkflow()
{
    /* Both objects are zero-initialized before their first setup call. */
    HIL_Transport_Context_T context{};
    HIL_Transport_Config_T  config{};
    HIL_Transport_Storage_T storage{};

    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, WorkspaceSize>
                                         workspace{};
    std::array<std::uint8_t, MaxMessage> application_message{};
    std::array<std::uint8_t, MaxOutput>  communication_buffer{};

    std::size_t required_workspace = 0u;
    std::size_t output_size        = 0u;
    std::size_t message_size       = 0u;
    std::size_t bytes_consumed     = 0u;

    HIL_Transport_Event_T           event{};
    HIL_Transport_Status_Snapshot_T snapshot{};

    HIL_TRANSPORT_Default_Config( &config );
    config.max_application_message_size = MaxMessage;
    config.max_encoded_frame_size       = MaxOutput;
    config.session_seed                 = UINT64_C( 0x1234 );
    config.initial_reliable_sequence    = 17u;
    config.connection_timeout_ms        = 0u;
    config.retransmit_timeout_ms        = 0u;
    config.max_retries                  = 0u;

    ( void )HIL_TRANSPORT_Required_Storage_Size( &config, &required_workspace );

    storage.workspace      = workspace.data();
    storage.workspace_size = workspace.size();

    ( void )HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage );
    ( void )HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 120u );

    ( void )HIL_TRANSPORT_Process( &context, 123u, HIL_TRANSPORT_OPERATING_MODE_NORMAL );
    ( void )HIL_TRANSPORT_Process( &context, 124u, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
    ( void )HIL_TRANSPORT_Process( &context, 125u, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );

    /* Submission is legal only after the public session reaches ESTABLISHED. */
    ( void )HIL_TRANSPORT_Get_Status( &context, &snapshot );
    if ( snapshot.session_state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
    {
        ( void )HIL_TRANSPORT_Submit_Application_Data( &context, application_message.data(),
                                                       application_message.size() );
    }

    /* External I/O remains separate: peek, offer bytes, then commit on acceptance. */
    ( void )HIL_TRANSPORT_Peek_Output( &context, nullptr, 0u, &output_size );
    const HIL_Transport_Status_T peek_status = HIL_TRANSPORT_Peek_Output(
        &context, communication_buffer.data(), communication_buffer.size(), &output_size );
    if ( peek_status == HIL_TRANSPORT_STATUS_OK
         && ExternalCommunicationAcceptedFrame( communication_buffer.data(), output_size ) )
    {
        ( void )HIL_TRANSPORT_Commit_Output( &context, 126u );
    }

    /* Arbitrary input chunks report the exact accepted prefix. */
    ( void )HIL_TRANSPORT_Receive_Bytes( &context, communication_buffer.data(), 3u,
                                         &bytes_consumed );
    ( void )HIL_TRANSPORT_Receive_Bytes( &context, communication_buffer.data() + 3u,
                                         communication_buffer.size() - 3u, &bytes_consumed );

    /* Only a complete Application message is visible at the public boundary. */
    ( void )HIL_TRANSPORT_Read_Application_Data( &context, nullptr, 0u, &message_size );
    ( void )HIL_TRANSPORT_Read_Application_Data( &context, application_message.data(),
                                                 application_message.size(), &message_size );

    ( void )HIL_TRANSPORT_Read_Event( &context, &event );
    ( void )HIL_TRANSPORT_Reset( &context );

    /* Changing role, configuration, or workspace requires a new zeroed context. */
    HIL_Transport_Context_T replacement_context{};

    ( void )required_workspace;
    ( void )replacement_context;
}

struct PublicEndpoint
{
    HIL_Transport_Context_T context{};
    HIL_Transport_Config_T  config{};
    alignas(
        HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, WorkspaceSize> workspace{};

    void Initialize( HIL_Transport_Role_T role, std::uint64_t seed, std::uint16_t initial_sequence,
                     std::uint32_t retransmit_timeout_ms = 10u, std::uint8_t max_retries = 2u )
    {
        HIL_TRANSPORT_Default_Config( &config );
        config.session_seed              = seed;
        config.initial_reliable_sequence = initial_sequence;
        config.retransmit_timeout_ms     = retransmit_timeout_ms;
        config.max_retries               = max_retries;

        std::size_t required = 0u;
        ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_LE( required, workspace.size() );

        HIL_Transport_Storage_T storage{ workspace.data(), required };

        ASSERT_EQ( HIL_TRANSPORT_Init( &context, role, &config, &storage ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ(
            HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 0u ),
            HIL_TRANSPORT_STATUS_OK );
    }
};

void TransferOneOutput( PublicEndpoint& from, PublicEndpoint& to, std::uint32_t now_ms )
{
    std::array<std::uint8_t, MaxOutput> bytes{};
    std::size_t                         size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Peek_Output( &from.context, bytes.data(), bytes.size(), &size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( size, 0u );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &from.context, now_ms ), HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &to.context, bytes.data(), size, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, size );
}

std::vector<std::uint8_t> TakeOneOutput( PublicEndpoint& endpoint, std::uint32_t now_ms )
{
    std::array<std::uint8_t, MaxOutput> bytes{};
    std::size_t                         size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Peek_Output( &endpoint.context, bytes.data(), bytes.size(), &size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_GT( size, 0u );
    if ( size == 0u || size > bytes.size() )
    {
        return {};
    }
    EXPECT_EQ( HIL_TRANSPORT_Commit_Output( &endpoint.context, now_ms ), HIL_TRANSPORT_STATUS_OK );
    return std::vector<std::uint8_t>( bytes.begin(),
                                      bytes.begin() + static_cast<std::ptrdiff_t>( size ) );
}

void DeliverBytes( PublicEndpoint& endpoint, const std::uint8_t* bytes, std::size_t size )
{
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &endpoint.context, bytes, size, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, size );
}

void DrainEvents( PublicEndpoint& endpoint )
{
    HIL_Transport_Event_T event{};
    while ( HIL_TRANSPORT_Read_Event( &endpoint.context, &event ) == HIL_TRANSPORT_STATUS_OK )
    {
    }
}

void EstablishPublicPair( PublicEndpoint& host, PublicEndpoint& rig )
{
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 2u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( rig, host, 4u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 6u );
    TransferOneOutput( rig, host, 7u );

    HIL_Transport_Status_Snapshot_T host_status{};
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    ASSERT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

}  // namespace

static_assert( std::is_standard_layout_v<HIL_Transport_Context_T> );
static_assert( std::is_standard_layout_v<HIL_Transport_Config_T> );
static_assert( std::is_standard_layout_v<HIL_Transport_Storage_T> );
static_assert( std::is_standard_layout_v<HIL_Transport_Status_Snapshot_T> );

TEST( TransportFacadeApiDesign, IntendedCallerWorkflowCompilesAndLinks )
{
    const auto workflow = &CompileIntendedFacadeWorkflow;
    EXPECT_NE( workflow, nullptr );

    HIL_Transport_Context_T context{};
    std::size_t             bytes_consumed = 99u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &bytes_consumed ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( bytes_consumed, 0u );
}

TEST( TransportFacadeSessionLifecycle, ObservesLinksPublishesEventsAndResetsExplicitly )
{
    HIL_Transport_Context_T context{};
    HIL_Transport_Config_T  config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x1234 );
    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, WorkspaceSize>
        workspace{};
    ASSERT_LE( required_size, workspace.size() );
    HIL_Transport_Storage_T storage{ workspace.data(), required_size };
    ASSERT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
               HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ(
        HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 1u ),
        HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ), HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ( HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 2u ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T snapshot{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( snapshot.link_state, HIL_TRANSPORT_LINK_STATE_CONNECTED );
    EXPECT_EQ( snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( snapshot.last_failure, HIL_TRANSPORT_FAILURE_NONE );
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED );
    EXPECT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_NONE );
    EXPECT_EQ( event.required_capacity, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 3u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ), HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ(
        HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 4u ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( snapshot.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( snapshot.last_failure, HIL_TRANSPORT_FAILURE_LINK_LOST );
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_LINK_LOST );
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
    EXPECT_EQ( event.status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_LINK_LOST );
    EXPECT_EQ(
        HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 5u ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &context, &event ), HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &context ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( snapshot.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( snapshot.event_pending, 0u );
}

TEST( TransportFacadeProcess, RecordsModesPublishesInitiateAndRejectsInvalidModeWithoutProgress )
{
    HIL_Transport_Context_T context{};
    HIL_Transport_Config_T  config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x4321 );
    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, WorkspaceSize>
                            workspace{};
    HIL_Transport_Storage_T storage{ workspace.data(), required_size };
    ASSERT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
               HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T snapshot{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( snapshot.operating_mode, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
    EXPECT_EQ( snapshot.operating_mode_valid, 1u );
    EXPECT_EQ( snapshot.output_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 2u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 3u, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( snapshot.operating_mode, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );
    EXPECT_EQ( snapshot.output_pending, 1u );
    EXPECT_EQ( snapshot.reliable_delivery_pending, 1u );

    HIL_Transport_Operating_Mode_T invalid_mode;
    std::memset( &invalid_mode, 0x7F, sizeof( invalid_mode ) );
    EXPECT_EQ( HIL_TRANSPORT_Process( &context, UINT32_MAX, invalid_mode ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &snapshot ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( snapshot.operating_mode, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );
    EXPECT_EQ( snapshot.output_pending, 1u );
    EXPECT_EQ( snapshot.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeApplicationDelivery,
      EstablishedPeersExchangeCrossedMessagesUsingOnlyPublicApi )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xA123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );

    const std::array<std::uint8_t, 4u> host_payload{ 1u, 2u, 3u, 4u };
    const std::array<std::uint8_t, 3u> rig_payload{ 9u, 8u, 7u };
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &host.context, host_payload.data(),
                                                      host_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &rig.context, rig_payload.data(),
                                                      rig_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );

    TransferOneOutput( host, rig, 10u );
    TransferOneOutput( rig, host, 11u );
    TransferOneOutput( rig, host, 12u );
    TransferOneOutput( host, rig, 13u );

    std::array<std::uint8_t, MaxMessage> host_received{};
    std::array<std::uint8_t, MaxMessage> rig_received{};
    std::size_t                          host_size = 0u;
    std::size_t                          rig_size  = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &host.context, host_received.data(),
                                                    host_received.size(), &host_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &rig.context, rig_received.data(),
                                                    rig_received.size(), &rig_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_size, rig_payload.size() );
    EXPECT_EQ( rig_size, host_payload.size() );
    EXPECT_TRUE( std::equal( rig_payload.begin(), rig_payload.end(), host_received.begin() ) );
    EXPECT_TRUE( std::equal( host_payload.begin(), host_payload.end(), rig_received.begin() ) );

    HIL_Transport_Status_Snapshot_T host_status{};
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.reliable_delivery_pending, 0u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 0u );
    EXPECT_EQ( host_status.application_message_pending, 0u );
    EXPECT_EQ( rig_status.application_message_pending, 0u );
}

TEST( TransportFacadeRecovery, RigDeliveryExhaustionResetsPeerAndReestablishes )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xA123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u, 10u, 0u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> payload{ 9u, 8u, 7u };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &rig.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto dropped_application = TakeOneOutput( rig, 20u );
    ASSERT_FALSE( dropped_application.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 31u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    TransferOneOutput( rig, host, 32u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 33u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 34u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );

    TransferOneOutput( host, rig, 35u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 36u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( rig, host, 37u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 38u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 39u );
    TransferOneOutput( rig, host, 40u );

    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

TEST( TransportFacadeRecovery, RigExplicitResetNotifiesHostAndReestablishes )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xD123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    TransferOneOutput( rig, host, 20u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );

    TransferOneOutput( host, rig, 23u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 24u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( rig, host, 25u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 26u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 27u );
    TransferOneOutput( rig, host, 28u );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

TEST( TransportFacadeRecovery, InFlightPeerTrafficCannotClearPendingRecoveryReset )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xA123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u, 10u, 0u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> host_payload{ 1u, 2u, 3u };
    const std::array<std::uint8_t, 3u> rig_payload{ 4u, 5u, 6u };

    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &host.context, host_payload.data(),
                                                      host_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto delayed_host_application = TakeOneOutput( host, 20u );
    ASSERT_FALSE( delayed_host_application.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &rig.context, rig_payload.data(),
                                                      rig_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto dropped_rig_application = TakeOneOutput( rig, 21u );
    ASSERT_FALSE( dropped_rig_application.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 31u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    ASSERT_EQ( rig_status.output_pending, 1u );

    /*
     * This frame was already accepted by external I/O before the rig abandoned
     * the old session. Receiving it during recovery must not clear RESET(old).
     */
    DeliverBytes( rig, delayed_host_application.data(), delayed_host_application.size() );

    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    const auto recovery_reset = TakeOneOutput( rig, 32u );
    ASSERT_FALSE( recovery_reset.empty() );
    DeliverBytes( host, recovery_reset.data(), recovery_reset.size() );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 33u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportFacadeRecovery, ExplicitResetAfterDeliveryFailureKeepsRecoveryReset )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xA1A3 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u, 10u, 0u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 2u> payload{ 0x11u, 0x22u };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &rig.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto dropped_application = TakeOneOutput( rig, 20u );
    ASSERT_FALSE( dropped_application.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 30u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );

    /* Caller-requested recovery must not erase the RESET produced by exhaustion. */
    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    const auto recovery_reset = TakeOneOutput( rig, 31u );
    ASSERT_FALSE( recovery_reset.empty() );
    DeliverBytes( host, recovery_reset.data(), recovery_reset.size() );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportFacadeRecovery, RepeatedExplicitResetPreservesPeerSynchronization )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xA223 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    /* A second Reset while RESET(old) is still READY must preserve it. */
    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    std::array<std::uint8_t, MaxOutput> first_peek{};
    std::size_t                         first_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Peek_Output( &rig.context, first_peek.data(), first_peek.size(),
                                          &first_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( first_size, 0u );

    /* Explicit Reset may invalidate a prior peek, but it must re-own RESET(old). */
    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    const auto recovery_reset = TakeOneOutput( rig, 20u );
    ASSERT_EQ( recovery_reset.size(), first_size );
    EXPECT_TRUE( std::equal( recovery_reset.begin(), recovery_reset.end(), first_peek.begin() ) );

    DeliverBytes( host, recovery_reset.data(), recovery_reset.size() );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportFacadeRecovery, ResetAndReplacementInitiateCanShareOneReceiveChunk )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xB123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &host.context ), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( host, 20u );
    ASSERT_FALSE( reset.empty() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( host, 22u );
    ASSERT_FALSE( initiate.empty() );

    std::vector<std::uint8_t> combined = reset;
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.output_pending, 0u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, ApplicationCompletesHostHandshakeWhenFinalAckIsLost )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xC123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 2u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( rig, host, 4u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 6u );

    HIL_Transport_Status_Snapshot_T host_status{};
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    ASSERT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    const auto dropped_final_ack = TakeOneOutput( rig, 7u );
    ASSERT_FALSE( dropped_final_ack.empty() );

    const std::array<std::uint8_t, 4u> payload{ 1u, 3u, 5u, 7u };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &rig.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( rig, host, 8u );

    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &host.context, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.application_message_pending, 1u );

    std::array<std::uint8_t, MaxMessage> received{};
    std::size_t                          received_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &host.context, received.data(), received.size(),
                                                    &received_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( received_size, payload.size() );
    EXPECT_TRUE( std::equal( payload.begin(), payload.end(), received.begin() ) );

    TransferOneOutput( host, rig, 9u );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 0u );
}

TEST( TransportFacadeRecovery, RecoveryResetCommitThenReceiveInitiateDoesNotRequireProcessFirst )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE123 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( rig, 20u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( host, reset.data(), reset.size() );

    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( host, 22u );
    ASSERT_FALSE( initiate.empty() );

    /* No Process() call is made on the rig between RESET commit and receive. */
    DeliverBytes( rig, initiate.data(), initiate.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.output_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, DelayedOldApplicationBeforeReplacementInitiateDoesNotAbortHandshake )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE223 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> payload{ 0x21u, 0x22u, 0x23u };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &host.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application = TakeOneOutput( host, 20u );
    ASSERT_FALSE( delayed_old_application.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( rig, 21u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( host, reset.data(), reset.size() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( host, 23u );
    ASSERT_FALSE( initiate.empty() );

    std::vector<std::uint8_t> combined = delayed_old_application;
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.application_message_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 24u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
}

TEST( TransportFacadeRecovery, PartialOldFrameThenReplacementInitiateAdvancesBetweenFrames )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE2A3 ), 10u, 0u, 0u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u, 0u, 0u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> payload{ 0x2Au, 0x2Bu, 0x2Cu };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &host.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application = TakeOneOutput( host, 20u );
    ASSERT_GT( delayed_old_application.size(), 1u );
    ASSERT_EQ( delayed_old_application.back(), 0u );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    /*
     * Leave the parser holding an incomplete frame from the abandoned session.
     * This deliberately prevents receive-entry recovery progression.
     */
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &rig.context, delayed_old_application.data(),
                                            delayed_old_application.size() - 1u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, delayed_old_application.size() - 1u );

    const auto reset = TakeOneOutput( rig, 21u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( host, reset.data(), reset.size() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( host, 23u );
    ASSERT_FALSE( initiate.empty() );

    /*
     * The old frame becomes complete first. Once it is rejected and consumed,
     * recovery must be reconsidered before the following INITIATE is parsed.
     */
    std::vector<std::uint8_t> combined{ delayed_old_application.back() };
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.application_message_pending, 0u );
    EXPECT_EQ( rig_status.output_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 24u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, DiscardedOldBodyThenReplacementInitiateAdvancesBetweenFrames )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE2B3 ), 10u, 0u, 0u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u, 0u, 0u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    /*
     * Fill the parser beyond its retained-body capacity without a delimiter so
     * it is discarding abandoned-session input when RESET is committed.
     */
    const std::vector<std::uint8_t> oversized_old_body( MaxOutput, 0x55u );
    std::size_t                     consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &rig.context, oversized_old_body.data(),
                                            oversized_old_body.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, oversized_old_body.size() );

    const auto reset = TakeOneOutput( rig, 20u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( host, reset.data(), reset.size() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( host, 22u );
    ASSERT_FALSE( initiate.empty() );

    /*
     * The delimiter finishes the discard. After its diagnostic is retained, the
     * following replacement INITIATE in the same chunk must see fresh establishment.
     */
    std::vector<std::uint8_t> combined{ 0u };
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.output_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, DeferredStaleDiagnosticAfterResetCommitDoesNotEnterFault )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE323 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 2u> payload{ 0x31u, 0x32u };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &host.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application = TakeOneOutput( host, 20u );
    ASSERT_FALSE( delayed_old_application.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );

    /* Fill the four-entry event FIFO with stale-session diagnostics. */
    for ( std::size_t index = 0u; index < 4u; ++index )
    {
        DeliverBytes( rig, delayed_old_application.data(), delayed_old_application.size() );
    }

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &rig.context, delayed_old_application.data(),
                                            delayed_old_application.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, delayed_old_application.size() );

    /* Commit RESET while the protocol-error publication is still deferred. */
    const auto reset = TakeOneOutput( rig, 21u );
    ASSERT_FALSE( reset.empty() );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &rig.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    EXPECT_EQ( HIL_TRANSPORT_Process( &rig.context, 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_NE( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
}

TEST( TransportFacadeRecovery, UnboundRigRejectsIrrelevantApplicationWithoutStartingRecovery )
{
    PublicEndpoint source_host;
    PublicEndpoint source_rig;
    source_host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE423 ), 10u );
    source_rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( source_host, source_rig );
    DrainEvents( source_host );
    DrainEvents( source_rig );

    const std::array<std::uint8_t, 2u> payload{ 0x41u, 0x42u };
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &source_host.context, payload.data(),
                                                      payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto unrelated_application = TakeOneOutput( source_host, 20u );
    ASSERT_FALSE( unrelated_application.empty() );

    PublicEndpoint waiting_rig;
    waiting_rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 700u );
    DrainEvents( waiting_rig );

    DeliverBytes( waiting_rig, unrelated_application.data(), unrelated_application.size() );

    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &waiting_rig.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.output_pending, 0u );
    EXPECT_EQ( status.application_message_pending, 0u );
}

TEST( TransportFacadeRecovery, RecentlyAbandonedInitiateIsRejectedUntilPhysicalReconnect )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE523 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto old_initiate = TakeOneOutput( host, 2u );
    ASSERT_FALSE( old_initiate.empty() );
    DeliverBytes( rig, old_initiate.data(), old_initiate.size() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( rig, host, 4u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( host, rig, 6u );
    TransferOneOutput( rig, host, 7u );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( rig, 20u );
    ASSERT_FALSE( reset.empty() );

    /* RESET is committed, so receive auto-enters WAITING_FOR_INITIATE. */
    DeliverBytes( rig, old_initiate.data(), old_initiate.size() );

    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.output_pending, 0u );

    /* Physical link restart is the MVP hard-recovery boundary and clears history. */
    ASSERT_EQ(
        HIL_TRANSPORT_Notify_Link_State( &rig.context, HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 21u ),
        HIL_TRANSPORT_STATUS_OK );
    DrainEvents( rig );
    ASSERT_EQ(
        HIL_TRANSPORT_Notify_Link_State( &rig.context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 22u ),
        HIL_TRANSPORT_STATUS_OK );
    DrainEvents( rig );

    DeliverBytes( rig, old_initiate.data(), old_initiate.size() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.output_pending, 1u );
}

TEST( TransportFacadeRecovery, DelayedOldAckBeforeReplacementInitiateDoesNotAbortHandshake )
{
    PublicEndpoint host;
    PublicEndpoint rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, UINT64_C( 0xE623 ), 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EstablishPublicPair( host, rig );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 2u> payload{ 0x61u, 0x62u };
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &rig.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto old_application = TakeOneOutput( rig, 20u );
    ASSERT_FALSE( old_application.empty() );
    DeliverBytes( host, old_application.data(), old_application.size() );
    const auto delayed_old_ack = TakeOneOutput( host, 21u );
    ASSERT_FALSE( delayed_old_ack.empty() );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &rig.context ), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( rig, 22u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( host, reset.data(), reset.size() );
    ASSERT_EQ( HIL_TRANSPORT_Process( &host.context, 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( host, 24u );
    ASSERT_FALSE( initiate.empty() );

    std::vector<std::uint8_t> combined = delayed_old_ack;
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.application_message_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig.context, 25u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &rig.context, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
}
