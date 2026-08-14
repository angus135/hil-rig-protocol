/**
 * @file transport_profile_mvp.c
 * @brief MVP wire storage and private output lifecycles plus remaining stubs.
 *
 * @details The MVP uses simple session establishment, one complete
 * Application message per frame, framing plus integrity, and one outstanding
 * reliable transmission. Workspace sizing, initialization, the wire path, the
 * reliable encoded-output lifecycle, separate private one-item control storage,
 * public output arbitration, session lifecycle coordination, link-state
 * recovery, semantic handshake progression, and Process scheduling are
 * implemented. Public byte reception and Application-message orchestration
 * remain stubs.
 */
#include "../transport_profile.h"

#include "../transport_internal.h"
#include "transport_control_output_mvp.h"
#include "transport_events_mvp.h"
#include "transport_frame_codec_mvp.h"
#include "transport_handshake_mvp.h"
#include "transport_output_mvp.h"
#include "transport_reliability_mvp.h"
#include "transport_session_mvp.h"
#include "transport_types_mvp.h"

#include <stdint.h>
#include <string.h>

static int HIL_TRANSPORT_MVP_Checked_Add( size_t left, size_t right, size_t* result )
{
    if ( ( result == NULL ) || ( left > ( SIZE_MAX - right ) ) )
    {
        return 0;
    }
    *result = left + right;
    return 1;
}

static int HIL_TRANSPORT_MVP_Storage_Overlaps( const void* object, size_t object_size,
                                               const HIL_Transport_Storage_T* storage )
{
    const uintptr_t object_start    = ( uintptr_t )object;
    const uintptr_t workspace_start = ( uintptr_t )storage->workspace;
    uintptr_t       object_end;
    uintptr_t       workspace_end;

    if ( ( object_start > ( UINTPTR_MAX - object_size ) )
         || ( workspace_start > ( UINTPTR_MAX - storage->workspace_size ) ) )
    {
        return 1;
    }
    object_end    = object_start + object_size;
    workspace_end = workspace_start + storage->workspace_size;
    return ( object_start < workspace_end ) && ( workspace_start < object_end );
}

static HIL_Transport_Mvp_Root_T*
HIL_TRANSPORT_MVP_Root_From_Context( const HIL_Transport_Context_T* context )
{
    if ( ( context == NULL ) || ( context->implementation == NULL )
         || ( context->implementation_size < sizeof( HIL_Transport_Mvp_Root_T ) )
         || ( context->initialization_cookie != HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE ) )
    {
        return NULL;
    }
    return ( HIL_Transport_Mvp_Root_T* )context->implementation;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Validate_Config_And_Size( const HIL_Transport_Config_T* config,
                                            size_t*                       required_size )
{
    size_t encoded_minimum;
    size_t raw_scratch_size;
    size_t total_size;

    if ( ( config == NULL ) || ( required_size == NULL )
         || ( config->max_application_message_size == 0u )
         || ( config->max_encoded_frame_size == 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( config->connection_timeout_ms != 0u )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }

    encoded_minimum = HIL_TRANSPORT_MVP_Max_Encoded_Size( config->max_application_message_size );
    if ( ( encoded_minimum == 0u ) || ( config->max_encoded_frame_size < encoded_minimum ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }
    if ( !HIL_TRANSPORT_MVP_Checked_Add( config->max_application_message_size,
                                         HIL_TRANSPORT_MVP_RAW_OVERHEAD, &raw_scratch_size ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }

    total_size = sizeof( HIL_Transport_Mvp_Root_T );
    if ( !HIL_TRANSPORT_MVP_Checked_Add( total_size, config->max_application_message_size,
                                         &total_size )
         || !HIL_TRANSPORT_MVP_Checked_Add( total_size, config->max_encoded_frame_size,
                                            &total_size )
         || !HIL_TRANSPORT_MVP_Checked_Add( total_size, config->max_encoded_frame_size - 1u,
                                            &total_size )
         || !HIL_TRANSPORT_MVP_Checked_Add( total_size, raw_scratch_size, &total_size )
         || !HIL_TRANSPORT_MVP_Checked_Add( total_size, config->max_application_message_size,
                                            &total_size ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }

    *required_size = total_size;
    return HIL_TRANSPORT_STATUS_OK;
}

void HIL_TRANSPORT_PROFILE_Default_Config( HIL_Transport_Config_T* config )
{
    if ( config == NULL )
    {
        return;
    }

    config->max_application_message_size = HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE;
    config->max_encoded_frame_size       = HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;
    config->session_seed                 = HIL_TRANSPORT_SESSION_SEED_INVALID;
    config->initial_reliable_sequence    = 0u;
    config->connection_timeout_ms        = 0u;
    config->retransmit_timeout_ms        = 0u;
    config->max_retries                  = 0u;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Required_Storage_Size( const HIL_Transport_Config_T* config,
                                             size_t*                       required_size )
{
    if ( required_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *required_size = 0u;
    return HIL_TRANSPORT_MVP_Validate_Config_And_Size( config, required_size );
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Init( HIL_Transport_Context_T*       context,
                                                   HIL_Transport_Role_T           role,
                                                   const HIL_Transport_Config_T*  config,
                                                   const HIL_Transport_Storage_T* storage )
{
    HIL_Transport_Status_T    status;
    HIL_Transport_Mvp_Root_T* root;
    uint8_t*                  next_region;
    size_t                    required_size = 0u;
    size_t                    raw_scratch_size;

    if ( ( context == NULL ) || ( config == NULL ) || ( storage == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( context->implementation != NULL ) || ( context->implementation_size != 0u )
         || ( context->initialization_cookie != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( role != HIL_TRANSPORT_ROLE_HOST ) && ( role != HIL_TRANSPORT_ROLE_RIG ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( ( role == HIL_TRANSPORT_ROLE_HOST )
           && ( ( config->session_seed == HIL_TRANSPORT_SESSION_SEED_INVALID )
                || ( config->session_seed == HIL_TRANSPORT_SESSION_SEED_RESERVED ) ) )
         || ( ( role == HIL_TRANSPORT_ROLE_RIG )
              && ( config->session_seed != HIL_TRANSPORT_SESSION_SEED_INVALID ) ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Validate_Config_And_Size( config, &required_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( storage->workspace == NULL )
         || ( ( ( uintptr_t )storage->workspace % HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( storage->workspace_size < required_size )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }
    if ( HIL_TRANSPORT_MVP_Storage_Overlaps( context, sizeof( *context ), storage )
         || HIL_TRANSPORT_MVP_Storage_Overlaps( config, sizeof( *config ), storage )
         || HIL_TRANSPORT_MVP_Storage_Overlaps( storage, sizeof( *storage ), storage ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    raw_scratch_size = config->max_application_message_size + HIL_TRANSPORT_MVP_RAW_OVERHEAD;
    memset( storage->workspace, 0, required_size );
    root        = ( HIL_Transport_Mvp_Root_T* )storage->workspace;
    next_region = storage->workspace + sizeof( *root );

    root->base.config         = *config;
    root->base.role           = role;
    root->base.link_state     = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    root->base.session_state  = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
    root->base.operating_mode = HIL_TRANSPORT_OPERATING_MODE_NORMAL;

    status = HIL_TRANSPORT_MVP_Session_Init( &root->session, role, config->session_seed,
                                             config->initial_reliable_sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        memset( storage->workspace, 0, required_size );
        return status;
    }
    root->output_selection     = HIL_TRANSPORT_MVP_OUTPUT_NONE;
    root->control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE;
    root->control_output_size  = 0u;

    root->submitted_message = next_region;
    next_region += config->max_application_message_size;
    root->encoded_output          = next_region;
    root->encoded_output_capacity = config->max_encoded_frame_size;
    next_region += config->max_encoded_frame_size;
    status = HIL_TRANSPORT_Parser_Init( &root->parser, next_region,
                                        config->max_encoded_frame_size - 1u );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        memset( storage->workspace, 0, required_size );
        return status;
    }
    next_region += config->max_encoded_frame_size - 1u;
    root->codec_scratch      = next_region;
    root->codec_scratch_size = raw_scratch_size;
    next_region += raw_scratch_size;
    root->received_message = next_region;

    status = HIL_TRANSPORT_MVP_Events_Reset( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        memset( storage->workspace, 0, required_size );
        return status;
    }

    context->implementation        = root;
    context->implementation_size   = required_size;
    context->initialization_cookie = HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Reset( HIL_Transport_Context_T* context )
{
    HIL_Transport_Mvp_Root_T* root = HIL_TRANSPORT_MVP_Root_From_Context( context );

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Session_Explicit_Reset( root );
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Notify_Link_State( HIL_Transport_Context_T*   context,
                                         HIL_Transport_Link_State_T link_state, uint32_t now_ms )
{
    HIL_Transport_Mvp_Root_T* root = HIL_TRANSPORT_MVP_Root_From_Context( context );

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( link_state != HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         && ( link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    ( void )now_ms;
    return HIL_TRANSPORT_MVP_Session_Notify_Link_State( root, link_state );
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Submit_Application_Data( HIL_Transport_Context_T* context,
                                               const uint8_t* payload, size_t payload_len )
{
    /*
     * TODO: Validate the pointer/size and complete one-frame bound. Require public
     * session state ESTABLISHED; otherwise return NOT_READY, retain no input
     * pointer, and change nothing. The MVP does not queue before establishment.
     * Reserve its sole message/reliable capacity before sequence allocation,
     * copy synchronously, and publish atomically. A second reliable item is
     * forbidden while the first is prepared, pinned, awaiting ACK, or retrying.
     */
    ( void )context;
    ( void )payload;
    ( void )payload_len;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Receive_Bytes( HIL_Transport_Context_T* context,
                                                            const uint8_t* data, size_t data_len,
                                                            size_t* bytes_consumed )
{
    /*
     * TODO: Clear and require bytes_consumed, validate the borrowed chunk, accept
     * arbitrary boundaries, and advance the exact count per accepted byte.
     * Preserve a retryable suffix on temporary capacity exhaustion; consume
     * malformed data only through a known resynchronization boundary; never
     * overwrite unread output or expose partial Application messages. Map
     * malformed/integrity/incompatible/stale private classifications to one
     * public PROTOCOL_ERROR event after consuming through the appropriate
     * resynchronization boundary; map capacity to CAPACITY_EXHAUSTED. Deliver an
     * expected reliable frame once, re-ACK its exact duplicate without
     * redelivery, and restart the complete session for incompatible traffic. A
     * private invariant sets FAULT/INTERNAL failure and returns INTERNAL_ERROR.
     */
    ( void )context;
    ( void )data;
    ( void )data_len;
    if ( bytes_consumed != NULL )
    {
        *bytes_consumed = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Process( HIL_Transport_Context_T* context, uint32_t now_ms,
                               HIL_Transport_Operating_Mode_T operating_mode )
{
    HIL_Transport_Mvp_Root_T*                 root;
    HIL_Transport_Mvp_Reliability_Outcome_T   reliability_outcome;
    HIL_Transport_Status_T                    status;
    uint8_t                                   control_pending;
    HIL_Transport_Mvp_Frame_Type_T            retained_type;

    root = HIL_TRANSPORT_MVP_Root_From_Context( context );
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( operating_mode < HIL_TRANSPORT_OPERATING_MODE_NORMAL )
         || ( operating_mode > HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    root->base.operating_mode       = operating_mode;
    root->base.operating_mode_valid = 1u;
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( root->base.link_state != root->session.link_state )
    {
        root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
        root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
        root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
        root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( root->base.link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_RECOVERING )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_RECOVERING ) )
    {
        if ( root->base.session_state != root->session.state )
        {
            root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
            root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
            root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
            root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
            return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
        }
        status = HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( root, &control_pending );
        if ( status != HIL_TRANSPORT_STATUS_OK )
        {
            return status;
        }
        if ( control_pending != 0u )
        {
            return HIL_TRANSPORT_STATUS_OK;
        }
        status = HIL_TRANSPORT_MVP_Session_Begin_Establishment( root );
        if ( status != HIL_TRANSPORT_STATUS_OK )
        {
            return status;
        }
    }

    status = HIL_TRANSPORT_MVP_Handshake_Process( root, now_ms );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Reliability_Process_Pending( root, now_ms,
                                                            &reliability_outcome );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    retained_type = root->session.retained_reliable_frame_type;
    if ( ( reliability_outcome == HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED )
         || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        if ( ( retained_type == HIL_TRANSPORT_MVP_FRAME_INITIATE )
             || ( retained_type == HIL_TRANSPORT_MVP_FRAME_RESPONSE )
             || ( retained_type == HIL_TRANSPORT_MVP_FRAME_CONFIRM ) )
        {
            return HIL_TRANSPORT_MVP_Session_Abandon( root, HIL_TRANSPORT_FAILURE_DELIVERY );
        }
    }
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Peek_Output( HIL_Transport_Context_T* context,
                                                          uint8_t*                 out_buffer,
                                                          size_t                   out_buffer_size,
                                                          size_t*                  output_size )
{
    HIL_Transport_Mvp_Root_T* root;

    if ( output_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0u;
    root         = HIL_TRANSPORT_MVP_Root_From_Context( context );
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Output_Peek_Output( root, out_buffer, out_buffer_size, output_size );
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Commit_Output( HIL_Transport_Context_T* context,
                                                            uint32_t                 now_ms )
{
    HIL_Transport_Mvp_Root_T* root = HIL_TRANSPORT_MVP_Root_From_Context( context );

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Output_Commit_Output( root, now_ms );
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Read_Application_Data( HIL_Transport_Context_T* context, uint8_t* out_buffer,
                                             size_t out_buffer_size, size_t* message_size )
{
    /*
     * TODO: Clear and require message_size, expose only the oldest complete
     * message, support a size query, leave it unchanged on insufficient output,
     * and release private storage only after a complete successful copy. Report
     * Transport delivery only, never Application semantic acceptance.
     */
    ( void )context;
    ( void )out_buffer;
    ( void )out_buffer_size;
    if ( message_size != NULL )
    {
        *message_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Read_Event( HIL_Transport_Context_T* context,
                                                         HIL_Transport_Event_T*   event )
{
    HIL_Transport_Mvp_Root_T* root = HIL_TRANSPORT_MVP_Root_From_Context( context );

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Events_Read( root, event );
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Get_Status( const HIL_Transport_Context_T*   context,
                                                         HIL_Transport_Status_Snapshot_T* status )
{
    HIL_Transport_Mvp_Root_T* root;
    HIL_Transport_Status_T    result;
    uint8_t                   output_pending;
    uint8_t                   delivery_pending;
    uint8_t                   event_pending;

    if ( status == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *status = ( HIL_Transport_Status_Snapshot_T ){ 0 };
    root    = HIL_TRANSPORT_MVP_Root_From_Context( context );
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    result =
        HIL_TRANSPORT_MVP_Output_Get_Pending_Status( root, &output_pending, &delivery_pending );
    if ( result != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }
    result = HIL_TRANSPORT_MVP_Events_Get_Pending_Status( root, &event_pending );
    if ( result != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }

    status->role                        = root->base.role;
    status->link_state                  = root->base.link_state;
    status->session_state               = root->base.session_state;
    status->operating_mode              = root->base.operating_mode;
    status->operating_mode_valid        = root->base.operating_mode_valid;
    status->output_pending              = output_pending;
    status->application_message_pending = root->received_message_pending;
    status->event_pending               = event_pending;
    status->reliable_delivery_pending   = delivery_pending;
    status->last_failure                = root->base.last_failure;
    return HIL_TRANSPORT_STATUS_OK;
}
