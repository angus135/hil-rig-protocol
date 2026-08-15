/**
 * @file transport_receive_mvp.c
 * @brief Transactional parser-to-session receive orchestration for the MVP.
 */
#include "transport_receive_mvp.h"

#include "transport_events_mvp.h"
#include "transport_frame_codec_mvp.h"
#include "transport_handshake_mvp.h"
#include "transport_session_mvp.h"

typedef enum
{
    HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME = 0,
    HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN,
    HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED
} HIL_Transport_Mvp_Receive_Body_Action_T;

typedef struct
{
    HIL_Transport_Status_T                  status;
    HIL_Transport_Mvp_Receive_Body_Action_T action;
} HIL_Transport_Mvp_Receive_Body_Outcome_T;

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Outcome( HIL_Transport_Status_T                  status,
                                   HIL_Transport_Mvp_Receive_Body_Action_T action )
{
    HIL_Transport_Mvp_Receive_Body_Outcome_T outcome;

    outcome.status = status;
    outcome.action = action;
    return outcome;
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Fault( HIL_Transport_Mvp_Root_T* root )
{
    ( void )HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_INTERNAL_ERROR,
                                              HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED );
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Receive_Validate_Root( HIL_Transport_Mvp_Root_T* root )
{
    const uint8_t*         body;
    size_t                 body_size;
    HIL_Transport_Status_T parser_status;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( ( root->receive_protocol_error_pending > 1u )
         || ( root->base.role < HIL_TRANSPORT_ROLE_HOST )
         || ( root->base.role > HIL_TRANSPORT_ROLE_RIG )
         || ( root->session.role != root->base.role )
         || ( root->base.link_state < HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         || ( root->base.link_state > HIL_TRANSPORT_LINK_STATE_CONNECTED )
         || ( root->session.link_state != root->base.link_state )
         || ( root->base.session_state < HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         || ( root->base.session_state > HIL_TRANSPORT_SESSION_STATE_RECOVERING )
         || ( root->session.state != root->base.session_state )
         || ( root->session.last_failure != root->base.last_failure )
         || ( root->base.config.max_application_message_size == 0u )
         || ( root->base.config.max_encoded_frame_size == 0u ) || ( root->codec_scratch == NULL )
         || ( root->base.config.max_application_message_size
              > ( SIZE_MAX - HIL_TRANSPORT_MVP_RAW_OVERHEAD ) )
         || ( root->codec_scratch_size < ( root->base.config.max_application_message_size
                                           + HIL_TRANSPORT_MVP_RAW_OVERHEAD ) )
         || ( root->parser.scratch_buffer_size
              != ( root->base.config.max_encoded_frame_size - 1u ) ) )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }

    parser_status = HIL_TRANSPORT_Parser_Peek_Body( &root->parser, &body, &body_size );
    if ( ( parser_status != HIL_TRANSPORT_STATUS_OK )
         && ( parser_status != HIL_TRANSPORT_STATUS_NOT_READY ) )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    if ( ( root->receive_protocol_error_pending != 0u )
         && ( ( parser_status == HIL_TRANSPORT_STATUS_OK )
              || ( root->parser.accumulated_size != 0u ) || ( root->parser.discarding != 0u ) ) )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Receive_Publish_Protocol_Error( HIL_Transport_Mvp_Root_T* root )
{
    const HIL_Transport_Event_T event = { HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                          HIL_TRANSPORT_STATUS_NOT_READY,
                                          HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    HIL_Transport_Status_T      status;

    status = HIL_TRANSPORT_MVP_Events_Publish( root, &event );
    if ( ( status != HIL_TRANSPORT_STATUS_OK )
         && ( status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    return status;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Receive_Process_Pending_Error( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T status;

    if ( root->receive_protocol_error_pending == 0u )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Receive_Publish_Protocol_Error( root );
    if ( status == HIL_TRANSPORT_STATUS_OK )
    {
        root->receive_protocol_error_pending = 0u;
    }
    return status;
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T status = HIL_TRANSPORT_MVP_Receive_Publish_Protocol_Error( root );

    if ( status == HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_OK,
                                                  HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
    }
    if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
    {
        return HIL_TRANSPORT_MVP_Receive_Outcome( status, HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN );
    }
    return HIL_TRANSPORT_MVP_Receive_Fault( root );
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T event_status;
    HIL_Transport_Status_T abandon_status;
    HIL_Transport_Status_T reset_status = HIL_TRANSPORT_STATUS_OK;
    uint64_t               failed_session_identifier;
    uint8_t                failed_session_identifier_valid;

    failed_session_identifier = root->session.session_identifier;
    failed_session_identifier_valid =
        ( uint8_t )( ( root->session.session_identifier_valid != 0u )
                     && ( failed_session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
                     && ( failed_session_identifier != HIL_TRANSPORT_SESSION_SEED_RESERVED ) );

    event_status   = HIL_TRANSPORT_MVP_Receive_Publish_Protocol_Error( root );
    abandon_status = HIL_TRANSPORT_MVP_Session_Abandon( root, HIL_TRANSPORT_FAILURE_PROTOCOL );

    /*
     * The existing RESET producer is the wire recovery mechanism for locally
     * detected incompatibility. Abandonment runs first so old control ownership
     * cannot block RESET and Process() waits for its commit before restarting.
     * Accepted peer RESET follows the handshake path and never reaches here.
     */
    if ( failed_session_identifier_valid != 0u )
    {
        reset_status = HIL_TRANSPORT_MVP_Handshake_Publish_Reset( root, failed_session_identifier );
    }

    if ( ( event_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
         || ( abandon_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
         || ( ( reset_status != HIL_TRANSPORT_STATUS_OK )
              && ( reset_status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
    if ( ( event_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
         || ( abandon_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
         || ( reset_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED,
                                                  HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED );
    }
    return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_OK,
                                              HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED );
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Dispatch_Handshake_Frame( HIL_Transport_Mvp_Root_T*        root,
                                                    const HIL_Transport_Mvp_Frame_T* frame )
{
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    HIL_Transport_Status_T                     status;

    status = HIL_TRANSPORT_MVP_Handshake_Handle_Frame( root, frame, &result );
    if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
    {
        return HIL_TRANSPORT_MVP_Receive_Outcome(
            status, root->parser.body_ready != 0u
                        ? HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN
                        : HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED );
    }
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }

    switch ( result )
    {
        case HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED:
        case HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE:
            return HIL_TRANSPORT_MVP_Receive_Outcome(
                HIL_TRANSPORT_STATUS_OK, root->parser.body_ready != 0u
                                             ? HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME
                                             : HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED );
        case HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE:
            return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
        case HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE:
            return HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( root );
        default:
            return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Dispatch_Acknowledgement( HIL_Transport_Mvp_Root_T*        root,
                                                    const HIL_Transport_Mvp_Frame_T* frame )
{
    /* Plan PR 9 adds Application delivery completion at this single routing seam. */
    if ( root->session.retained_reliable_frame_type == HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
    {
        return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
    }
    return HIL_TRANSPORT_MVP_Receive_Dispatch_Handshake_Frame( root, frame );
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Dispatch_Frame( HIL_Transport_Mvp_Root_T*        root,
                                          const HIL_Transport_Mvp_Frame_T* frame )
{
    switch ( frame->type )
    {
        case HIL_TRANSPORT_MVP_FRAME_INITIATE:
        case HIL_TRANSPORT_MVP_FRAME_RESPONSE:
        case HIL_TRANSPORT_MVP_FRAME_CONFIRM:
        case HIL_TRANSPORT_MVP_FRAME_RESET:
            return HIL_TRANSPORT_MVP_Receive_Dispatch_Handshake_Frame( root, frame );
        case HIL_TRANSPORT_MVP_FRAME_ACK:
            return HIL_TRANSPORT_MVP_Receive_Dispatch_Acknowledgement( root, frame );
        case HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE:
            /*
             * Inbound Application ownership belongs to Plan PR 10. Until then,
             * reject one structurally valid frame as protocol input without
             * copying into received_message or making that storage visible.
             */
            return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
        default:
            return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Decode_Retained_Body( HIL_Transport_Mvp_Root_T* root )
{
    const uint8_t*                    encoded_body;
    size_t                            encoded_body_size;
    HIL_Transport_Mvp_Frame_T         frame;
    HIL_Transport_Mvp_Decode_Result_T decode_result;
    HIL_Transport_Status_T            status;

    status = HIL_TRANSPORT_Parser_Peek_Body( &root->parser, &encoded_body, &encoded_body_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
    status = HIL_TRANSPORT_MVP_Decode_Frame_View(
        encoded_body, encoded_body_size, root->codec_scratch, root->codec_scratch_size,
        root->base.config.max_application_message_size, &frame, &decode_result );
    if ( status == HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE )
    {
        return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
    }
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }

    switch ( decode_result )
    {
        case HIL_TRANSPORT_MVP_DECODE_VALID:
            return HIL_TRANSPORT_MVP_Receive_Dispatch_Frame( root, &frame );
        case HIL_TRANSPORT_MVP_DECODE_MALFORMED:
        case HIL_TRANSPORT_MVP_DECODE_INTEGRITY_INVALID:
            return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
        case HIL_TRANSPORT_MVP_DECODE_SESSION_INCOMPATIBLE:
            if ( root->session.session_identifier_valid != 0u )
            {
                return HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( root );
            }
            return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
        default:
            return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Receive_Process_Retained_Body( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Mvp_Receive_Body_Outcome_T outcome =
        HIL_TRANSPORT_MVP_Receive_Decode_Retained_Body( root );

    if ( outcome.action == HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN )
    {
        return outcome.status;
    }
    if ( outcome.action == HIL_TRANSPORT_MVP_RECEIVE_BODY_ALREADY_RELEASED )
    {
        return outcome.status;
    }
    if ( outcome.action != HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    if ( HIL_TRANSPORT_Parser_Consume_Body( &root->parser ) != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
    }
    return outcome.status;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Receive_Bytes( HIL_Transport_Mvp_Root_T* root,
                                                        const uint8_t* data, size_t data_size,
                                                        size_t* bytes_consumed )
{
    HIL_Transport_Status_T        status;
    HIL_Transport_Parser_Result_T parser_result;
    size_t                        accepted_size;

    if ( bytes_consumed == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *bytes_consumed = 0u;
    if ( ( data == NULL ) && ( data_size != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Receive_Validate_Root( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->base.link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    status = HIL_TRANSPORT_MVP_Receive_Process_Pending_Error( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->parser.body_ready != 0u )
    {
        status = HIL_TRANSPORT_MVP_Receive_Process_Retained_Body( root );
        if ( status != HIL_TRANSPORT_STATUS_OK )
        {
            return status;
        }
    }

    while ( *bytes_consumed < data_size )
    {
        accepted_size = 0u;
        parser_result = HIL_TRANSPORT_Parser_Push_Bytes(
            &root->parser, data + *bytes_consumed, data_size - *bytes_consumed, &accepted_size );
        *bytes_consumed += accepted_size;

        switch ( parser_result )
        {
            case HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA:
            case HIL_TRANSPORT_PARSER_RESULT_DISCARDING:
                if ( *bytes_consumed != data_size )
                {
                    return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
                }
                return HIL_TRANSPORT_STATUS_OK;
            case HIL_TRANSPORT_PARSER_RESULT_BODY_READY:
                status = HIL_TRANSPORT_MVP_Receive_Process_Retained_Body( root );
                if ( status != HIL_TRANSPORT_STATUS_OK )
                {
                    return status;
                }
                break;
            case HIL_TRANSPORT_PARSER_RESULT_DISCARDED_BODY:
                root->receive_protocol_error_pending = 1u;
                status = HIL_TRANSPORT_MVP_Receive_Process_Pending_Error( root );
                if ( status != HIL_TRANSPORT_STATUS_OK )
                {
                    return status;
                }
                break;
            case HIL_TRANSPORT_PARSER_RESULT_ERROR:
            default:
                return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
        }
    }
    return HIL_TRANSPORT_STATUS_OK;
}
