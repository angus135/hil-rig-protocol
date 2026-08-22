/**
 * @file transport_profile_mvp.c
 * @brief MVP wire, receive, session, output, and Application-delivery integration.
 *
 * @details The MVP uses simple session establishment, one complete
 * Application message per frame, framing plus integrity, and one outstanding
 * reliable transmission. Workspace sizing, initialization, the wire path, the
 * reliable encoded-output lifecycle, separate private one-item control storage,
 * public output arbitration, session lifecycle coordination, link-state
 * recovery, semantic handshake progression, transactional byte reception, and
 * Process scheduling and bidirectional one-message-at-a-time Application delivery are implemented.
 */
#include "../transport_profile.h"

#include "../transport_internal.h"
#include "transport_application_mvp.h"
#include "transport_control_output_mvp.h"
#include "transport_events_mvp.h"
#include "transport_frame_codec_mvp.h"
#include "transport_handshake_mvp.h"
#include "transport_output_mvp.h"
#include "transport_reliability_mvp.h"
#include "transport_receive_mvp.h"
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

static int
HIL_TRANSPORT_MVP_Profile_Has_Trustworthy_Active_Session( const HIL_Transport_Mvp_Root_T* root )
{
    return ( root != NULL ) && ( root->session.role >= HIL_TRANSPORT_ROLE_HOST )
           && ( root->session.role <= HIL_TRANSPORT_ROLE_RIG )
           && ( root->base.role == root->session.role )
           && ( root->session.link_state == HIL_TRANSPORT_LINK_STATE_CONNECTED )
           && ( root->base.link_state == root->session.link_state )
           && ( root->session.link_state_observed == 1u )
           && ( root->session.state >= HIL_TRANSPORT_SESSION_STATE_CONNECTING )
           && ( root->session.state <= HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
           && ( root->base.session_state == root->session.state )
           && ( root->base.last_failure == root->session.last_failure )
           && HIL_TRANSPORT_MVP_Session_Completed_Confirm_Metadata_Is_Valid( root )
           && ( root->session.session_identifier_valid == 1u )
           && ( root->session.session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
           && ( root->session.session_identifier != HIL_TRANSPORT_SESSION_SEED_RESERVED )
           && ( ( ( root->session.state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
                  && ( root->session.handshake_phase
                       == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED ) )
                || ( ( root->session.state == HIL_TRANSPORT_SESSION_STATE_CONNECTING )
                     && ( root->session.handshake_phase
                          > HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE )
                     && ( root->session.handshake_phase
                          < HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED ) ) );
}

static int HIL_TRANSPORT_MVP_Profile_Has_Trustworthy_Pending_Recovery_Reset(
    const HIL_Transport_Mvp_Root_T* root )
{
    int control_state_is_coherent;

    if ( root == NULL )
    {
        return 0;
    }

    control_state_is_coherent =
        ( ( root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY )
          && ( root->output_selection == HIL_TRANSPORT_MVP_OUTPUT_NONE ) )
        || ( ( root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED )
             && ( root->output_selection == HIL_TRANSPORT_MVP_OUTPUT_CONTROL ) );

    return ( root->recovery_reset_pending == 1u )
           && ( root->recovery_reset_session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
           && ( root->recovery_reset_session_identifier != HIL_TRANSPORT_SESSION_SEED_RESERVED )
           && ( root->session.role >= HIL_TRANSPORT_ROLE_HOST )
           && ( root->session.role <= HIL_TRANSPORT_ROLE_RIG )
           && ( root->base.role == root->session.role )
           && ( root->session.link_state == HIL_TRANSPORT_LINK_STATE_CONNECTED )
           && ( root->base.link_state == root->session.link_state )
           && ( root->session.link_state_observed == 1u )
           && ( root->session.state == HIL_TRANSPORT_SESSION_STATE_RECOVERING )
           && ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_RECOVERING )
           && ( root->base.last_failure == root->session.last_failure )
           && ( root->session.session_identifier_valid == 0u )
           && ( root->session.session_identifier == HIL_TRANSPORT_SESSION_SEED_INVALID )
           && ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE )
           && ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_IDLE )
           && ( root->session.retained_reliable_frame_type == HIL_TRANSPORT_MVP_FRAME_INVALID )
           && ( root->control_output_size > 0u )
           && ( root->control_output_size <= HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY )
           && control_state_is_coherent;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Profile_Validate_Status_Metadata( HIL_Transport_Mvp_Root_T* root )
{
    if ( ( root->base.role < HIL_TRANSPORT_ROLE_HOST )
         || ( root->base.role > HIL_TRANSPORT_ROLE_RIG )
         || ( root->session.role != root->base.role )
         || ( root->base.link_state < HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         || ( root->base.link_state > HIL_TRANSPORT_LINK_STATE_CONNECTED )
         || ( root->session.link_state != root->base.link_state )
         || ( root->base.session_state < HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         || ( root->base.session_state > HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state != root->base.session_state )
         || ( root->base.operating_mode < HIL_TRANSPORT_OPERATING_MODE_NORMAL )
         || ( root->base.operating_mode > HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME )
         || ( root->base.operating_mode_valid > 1u )
         || ( root->base.last_failure < HIL_TRANSPORT_FAILURE_NONE )
         || ( root->base.last_failure > HIL_TRANSPORT_FAILURE_INTERNAL )
         || ( root->session.last_failure != root->base.last_failure )
         || !HIL_TRANSPORT_MVP_Session_Completed_Confirm_Metadata_Is_Valid( root ) )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    return HIL_TRANSPORT_STATUS_OK;
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
    HIL_Transport_Status_T    status;
    uint64_t                  reset_session_identifier = HIL_TRANSPORT_SESSION_SEED_INVALID;
    uint8_t                   notify_peer              = 0u;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    /*
     * Preserve an already pending local recovery RESET across another explicit
     * Reset. Otherwise capture the old identity only from coherent non-FAULT
     * active state. Explicit Reset is also the repair path for private
     * corruption, so untrustworthy metadata must never be copied onto the wire.
     */
    if ( HIL_TRANSPORT_MVP_Profile_Has_Trustworthy_Pending_Recovery_Reset( root ) )
    {
        reset_session_identifier = root->recovery_reset_session_identifier;
        notify_peer              = 1u;
    }
    else if ( HIL_TRANSPORT_MVP_Profile_Has_Trustworthy_Active_Session( root ) )
    {
        reset_session_identifier = root->session.session_identifier;
        notify_peer              = 1u;
    }

    status = HIL_TRANSPORT_MVP_Session_Explicit_Reset( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( notify_peer == 0u )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    root->recently_abandoned_session_identifier       = reset_session_identifier;
    root->recently_abandoned_session_identifier_valid = 1u;

    status = HIL_TRANSPORT_MVP_Handshake_Publish_Reset( root, reset_session_identifier );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    root->recovery_reset_pending            = 1u;
    root->recovery_reset_session_identifier = reset_session_identifier;
    return HIL_TRANSPORT_STATUS_OK;
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
    HIL_Transport_Mvp_Root_T* root = HIL_TRANSPORT_MVP_Root_From_Context( context );

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Application_Submit( root, payload, payload_len );
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Receive_Bytes( HIL_Transport_Context_T* context,
                                                            const uint8_t* data, size_t data_len,
                                                            size_t* bytes_consumed )
{
    HIL_Transport_Mvp_Root_T* root;

    if ( bytes_consumed == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *bytes_consumed = 0u;
    root            = HIL_TRANSPORT_MVP_Root_From_Context( context );
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Receive_Bytes( root, data, data_len, bytes_consumed );
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Process( HIL_Transport_Context_T* context, uint32_t now_ms,
                               HIL_Transport_Operating_Mode_T operating_mode )
{
    HIL_Transport_Mvp_Root_T*               root;
    HIL_Transport_Mvp_Reliability_Outcome_T reliability_outcome;
    HIL_Transport_Status_T                  status;
    size_t                                  receive_bytes_consumed;
    uint8_t                                 control_pending;
    HIL_Transport_Mvp_Frame_Type_T          retained_type;

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

    /*
     * Resolve receive work before crossing a recovery boundary. A deferred
     * protocol diagnostic is ordinary bounded backpressure, not an invariant
     * failure, and must therefore be allowed to drain before the strict fresh
     * establishment checks run.
     */
    if ( ( root->receive_protocol_error_pending != 0u ) || ( root->parser.body_ready != 0u ) )
    {
        status = HIL_TRANSPORT_MVP_Receive_Bytes( root, NULL, 0u, &receive_bytes_consumed );
        if ( status != HIL_TRANSPORT_STATUS_OK )
        {
            return status;
        }
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
        /*
         * A partial parser body can legitimately span caller input calls. Do
         * not turn that recoverable condition into FAULT merely because the
         * recovery RESET has already been committed. The receive path will
         * finish or discard it before attempting establishment.
         */
        if ( ( root->parser.accumulated_size != 0u ) || ( root->parser.discarding != 0u ) )
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
    status = HIL_TRANSPORT_MVP_Reliability_Process_Pending( root, now_ms, &reliability_outcome );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    retained_type = root->session.retained_reliable_frame_type;
    if ( ( reliability_outcome == HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED )
         || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        if ( retained_type == HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
        {
            return HIL_TRANSPORT_MVP_Application_Handle_Retry_Exhaustion( root );
        }
        if ( ( retained_type == HIL_TRANSPORT_MVP_FRAME_INITIATE )
             || ( retained_type == HIL_TRANSPORT_MVP_FRAME_RESPONSE )
             || ( retained_type == HIL_TRANSPORT_MVP_FRAME_CONFIRM ) )
        {
            return HIL_TRANSPORT_MVP_Handshake_Begin_Local_Recovery(
                root, HIL_TRANSPORT_FAILURE_DELIVERY );
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
    HIL_Transport_Mvp_Root_T* root = HIL_TRANSPORT_MVP_Root_From_Context( context );

    if ( root == NULL )
    {
        if ( message_size != NULL )
        {
            *message_size = 0u;
        }
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Application_Read( root, out_buffer, out_buffer_size, message_size );
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
    uint8_t                   application_message_pending;
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
    result = HIL_TRANSPORT_MVP_Profile_Validate_Status_Metadata( root );
    if ( result != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }
    result =
        HIL_TRANSPORT_MVP_Output_Get_Pending_Status( root, &output_pending, &delivery_pending );
    if ( result != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }
    result = HIL_TRANSPORT_MVP_Application_Get_Pending_Status( root, &application_message_pending );
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
    status->application_message_pending = application_message_pending;
    status->event_pending               = event_pending;
    status->reliable_delivery_pending   = delivery_pending;
    status->last_failure                = root->base.last_failure;
    return HIL_TRANSPORT_STATUS_OK;
}
