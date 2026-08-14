#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

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
               HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED );
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
