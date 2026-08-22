/**
 * @file transport_receive_mvp.c
 * @brief Transactional parser-to-session receive orchestration for the MVP.
 */
#include "transport_receive_mvp.h"

#include "transport_application_mvp.h"
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

static int HIL_TRANSPORT_MVP_Receive_Session_Identifier_Is_Valid( uint64_t session_identifier )
{
    return ( session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
           && ( session_identifier != HIL_TRANSPORT_SESSION_SEED_RESERVED );
}

static int HIL_TRANSPORT_MVP_Receive_Recently_Abandoned_Metadata_Is_Valid(
    const HIL_Transport_Mvp_Root_T* root )
{
    if ( root->recently_abandoned_session_identifier_valid > 1u )
    {
        return 0;
    }
    if ( root->recently_abandoned_session_identifier_valid == 0u )
    {
        return root->recently_abandoned_session_identifier == HIL_TRANSPORT_SESSION_SEED_INVALID;
    }
    return HIL_TRANSPORT_MVP_Receive_Session_Identifier_Is_Valid(
        root->recently_abandoned_session_identifier );
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
    if ( ( root->receive_protocol_error_pending > 1u ) || ( root->recovery_reset_pending > 1u )
         || !HIL_TRANSPORT_MVP_Receive_Recently_Abandoned_Metadata_Is_Valid( root )
         || !HIL_TRANSPORT_MVP_Session_Completed_Confirm_Metadata_Is_Valid( root )
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

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Receive_Try_Advance_Recovery( HIL_Transport_Mvp_Root_T* root )
{
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ( ( root->session.state != HIL_TRANSPORT_SESSION_STATE_RECOVERING )
         || ( root->base.session_state != HIL_TRANSPORT_SESSION_STATE_RECOVERING ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    /*
     * Recovery may cross into replacement establishment only after the local
     * RESET barrier and all receive/output ownership from the abandoned session
     * have been resolved. A partial body, active discard, or deferred diagnostic
     * is ordinary pending work and leaves the endpoint in RECOVERING.
     */
    if ( ( root->recovery_reset_pending != 0u )
         || ( root->control_output_state != HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE )
         || ( root->control_output_size != 0u )
         || ( root->output_selection != HIL_TRANSPORT_MVP_OUTPUT_NONE )
         || ( root->receive_protocol_error_pending != 0u ) || ( root->parser.body_ready != 0u )
         || ( root->parser.accumulated_size != 0u ) || ( root->parser.discarding != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    return HIL_TRANSPORT_MVP_Session_Begin_Establishment( root );
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
HIL_TRANSPORT_MVP_Receive_Reject_Obsolete_Body( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T status = HIL_TRANSPORT_MVP_Receive_Publish_Protocol_Error( root );

    if ( status == HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_OK,
                                                  HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
    }
    if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
    {
        /*
         * This body is already known to be irrelevant to the replacement
         * session. Consume it even when the diagnostic FIFO is full so it can
         * never be reinterpreted after recovery; retain only the event
         * publication obligation.
         */
        root->receive_protocol_error_pending = 1u;
        return HIL_TRANSPORT_MVP_Receive_Outcome( status, HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
    }
    return HIL_TRANSPORT_MVP_Receive_Fault( root );
}

static int
HIL_TRANSPORT_MVP_Receive_Frame_Is_Recently_Abandoned( const HIL_Transport_Mvp_Root_T*  root,
                                                       const HIL_Transport_Mvp_Frame_T* frame )
{
    /*
     * This pre-dispatch check is the sole cross-session stale classifier. Only
     * the explicitly recorded abandoned identity is stale; frame-specific
     * handlers treat every other different valid identity as incompatible.
     */
    return ( root->recently_abandoned_session_identifier_valid != 0u )
           && ( frame->session_identifier == root->recently_abandoned_session_identifier )
           && ( ( root->session.session_identifier_valid == 0u )
                || ( frame->session_identifier != root->session.session_identifier ) );
}

static int HIL_TRANSPORT_MVP_Receive_Rig_Is_Waiting_Unbound_For_Initiate(
    const HIL_Transport_Mvp_Root_T* root )
{
    return ( root->session.role == HIL_TRANSPORT_ROLE_RIG )
           && ( root->session.state == HIL_TRANSPORT_SESSION_STATE_CONNECTING )
           && ( root->session.handshake_phase
                == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE )
           && ( root->session.session_identifier_valid == 0u )
           && ( root->session.session_identifier == HIL_TRANSPORT_SESSION_SEED_INVALID );
}

static int HIL_TRANSPORT_MVP_Receive_Frame_Has_Incompatible_Session_Identifier(
    const HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame )
{
    return ( root->session.session_identifier_valid != 0u )
           && ( frame->session_identifier != root->session.session_identifier );
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T event_status;
    HIL_Transport_Status_T recovery_status;

    event_status = HIL_TRANSPORT_MVP_Receive_Publish_Protocol_Error( root );

    /*
     * A locally generated recovery RESET is the synchronization barrier for the
     * abandoned session. While it remains uncommitted, further incompatible
     * traffic belongs to a session that is already being discarded. Reject the
     * frame without abandoning again, because Session_Abandon() would clear
     * the pending RESET and recreate the original split-brain failure.
     */
    if ( root->recovery_reset_pending != 0u )
    {
        if ( ( root->recovery_reset_pending != 1u )
             || ( root->base.session_state != HIL_TRANSPORT_SESSION_STATE_RECOVERING )
             || ( root->session.state != HIL_TRANSPORT_SESSION_STATE_RECOVERING )
             || ( root->session.link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED )
             || ( root->base.link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED )
             || ( root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE )
             || ( root->recovery_reset_session_identifier == HIL_TRANSPORT_SESSION_SEED_INVALID )
             || ( root->recovery_reset_session_identifier == HIL_TRANSPORT_SESSION_SEED_RESERVED ) )
        {
            return HIL_TRANSPORT_MVP_Receive_Fault( root );
        }
        if ( event_status == HIL_TRANSPORT_STATUS_OK )
        {
            return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_OK,
                                                      HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
        }
        if ( event_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
        {
            /*
             * The frame is already known to belong to an abandoned session, so
             * it must not survive past RESET commit and be reinterpreted under
             * the replacement session. Consume it now and retain only the
             * diagnostic obligation, matching oversized-body recovery.
             */
            root->receive_protocol_error_pending = 1u;
            return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED,
                                                      HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
        }
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }

    /*
     * Locally detected incompatibility uses the common recovery policy: old
     * session work is abandoned and the peer is notified with RESET. Event
     * backpressure must not suppress that mandatory wire-recovery attempt.
     */
    recovery_status =
        HIL_TRANSPORT_MVP_Handshake_Begin_Local_Recovery( root, HIL_TRANSPORT_FAILURE_PROTOCOL );

    if ( ( event_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
         || ( recovery_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
    if ( ( event_status != HIL_TRANSPORT_STATUS_OK )
         && ( event_status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
    if ( ( recovery_status != HIL_TRANSPORT_STATUS_OK )
         && ( recovery_status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }
    if ( event_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
    {
        root->receive_protocol_error_pending = 1u;
    }
    if ( ( event_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
         || ( recovery_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
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
    HIL_Transport_Mvp_Application_Ack_Result_T application_result;
    HIL_Transport_Status_T                     status;

    if ( root->session.retained_reliable_frame_type == HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
    {
        status = HIL_TRANSPORT_MVP_Application_Handle_Acknowledgement( root, frame,
                                                                       &application_result );
        if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
        {
            return HIL_TRANSPORT_MVP_Receive_Outcome( status,
                                                      HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN );
        }
        if ( status != HIL_TRANSPORT_STATUS_OK )
        {
            return HIL_TRANSPORT_MVP_Receive_Fault( root );
        }
        if ( application_result == HIL_TRANSPORT_MVP_APPLICATION_ACK_ACCEPTED )
        {
            return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_OK,
                                                      HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
        }
        if ( application_result == HIL_TRANSPORT_MVP_APPLICATION_ACK_STALE )
        {
            return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
        }
        if ( application_result == HIL_TRANSPORT_MVP_APPLICATION_ACK_INCOMPATIBLE )
        {
            return HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( root );
        }
        return HIL_TRANSPORT_MVP_Receive_Fault( root );
    }

    /*
     * Once a session is established, the sole handshake exception is an exact
     * current-session repeat of the ACK that completed the host's CONFIRM. The
     * handshake layer recognizes that immutable completion marker without event
     * capacity or lifecycle mutation. Every other ACK remains stale/unexpected.
     */
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
         && ( root->session.state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED ) )
    {
        if ( HIL_TRANSPORT_MVP_Handshake_Is_Duplicate_Final_Host_Acknowledgement( root, frame ) )
        {
            return HIL_TRANSPORT_MVP_Receive_Outcome( HIL_TRANSPORT_STATUS_OK,
                                                      HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
        }
        return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
    }

    return HIL_TRANSPORT_MVP_Receive_Dispatch_Handshake_Frame( root, frame );
}

static HIL_Transport_Mvp_Receive_Body_Outcome_T
HIL_TRANSPORT_MVP_Receive_Dispatch_Frame( HIL_Transport_Mvp_Root_T*        root,
                                          const HIL_Transport_Mvp_Frame_T* frame )
{
    if ( HIL_TRANSPORT_MVP_Receive_Frame_Is_Recently_Abandoned( root, frame ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Reject_Obsolete_Body( root );
    }

    /*
     * An unbound rig has no current session to abandon. Delayed ACK, RESET,
     * Application, RESPONSE, or CONFIRM traffic is therefore diagnostic noise,
     * not a reason to enter another recovery cycle. Stay ready for the next
     * valid INITIATE.
     */
    if ( HIL_TRANSPORT_MVP_Receive_Rig_Is_Waiting_Unbound_For_Initiate( root )
         && ( frame->type != HIL_TRANSPORT_MVP_FRAME_INITIATE ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Reject_Obsolete_Body( root );
    }

    /*
     * Decoding has already rejected invalid/reserved identities. With the sole
     * stale marker and unbound-rig exception handled above, every other identity
     * mismatch against a valid current session requires recovery before semantic
     * dispatch. Current-session frames retain their frame-specific rules.
     */
    if ( HIL_TRANSPORT_MVP_Receive_Frame_Has_Incompatible_Session_Identifier( root, frame ) )
    {
        return HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( root );
    }

    switch ( frame->type )
    {
        case HIL_TRANSPORT_MVP_FRAME_INITIATE:
        case HIL_TRANSPORT_MVP_FRAME_RESPONSE:
        case HIL_TRANSPORT_MVP_FRAME_CONFIRM:
        case HIL_TRANSPORT_MVP_FRAME_RESET:
            return HIL_TRANSPORT_MVP_Receive_Dispatch_Handshake_Frame( root, frame );
        case HIL_TRANSPORT_MVP_FRAME_ACK:
            return HIL_TRANSPORT_MVP_Receive_Dispatch_Acknowledgement( root, frame );
        case HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE: {
            HIL_Transport_Mvp_Application_Frame_Result_T           application_result;
            HIL_Transport_Mvp_Handshake_Application_Proof_Result_T proof_result;
            HIL_Transport_Status_T                                 status;

            /*
             * A rig can legitimately send Application traffic after accepting
             * CONFIRM even when the host's final ACK was lost. In that one
             * host-only phase, a valid next-sequence Application is equivalent
             * evidence that CONFIRM completed.
             */
            status = HIL_TRANSPORT_MVP_Handshake_Try_Complete_Host_From_Application(
                root, frame, &proof_result );
            if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
            {
                return HIL_TRANSPORT_MVP_Receive_Outcome( status,
                                                          HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN );
            }
            if ( status != HIL_TRANSPORT_STATUS_OK )
            {
                return HIL_TRANSPORT_MVP_Receive_Fault( root );
            }
            if ( proof_result == HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_STALE )
            {
                return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
            }
            if ( proof_result == HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_INCOMPATIBLE )
            {
                return HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( root );
            }

            status = HIL_TRANSPORT_MVP_Application_Handle_Received_Frame( root, frame,
                                                                          &application_result );
            if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
            {
                return HIL_TRANSPORT_MVP_Receive_Outcome( status,
                                                          HIL_TRANSPORT_MVP_RECEIVE_BODY_RETAIN );
            }
            if ( status != HIL_TRANSPORT_STATUS_OK )
            {
                return HIL_TRANSPORT_MVP_Receive_Fault( root );
            }
            switch ( application_result )
            {
                case HIL_TRANSPORT_MVP_APPLICATION_FRAME_ACCEPTED:
                case HIL_TRANSPORT_MVP_APPLICATION_FRAME_DUPLICATE:
                    return HIL_TRANSPORT_MVP_Receive_Outcome(
                        HIL_TRANSPORT_STATUS_OK, HIL_TRANSPORT_MVP_RECEIVE_BODY_CONSUME );
                case HIL_TRANSPORT_MVP_APPLICATION_FRAME_STALE:
                    return HIL_TRANSPORT_MVP_Receive_Reject_Retained_Body( root );
                case HIL_TRANSPORT_MVP_APPLICATION_FRAME_INCOMPATIBLE:
                    return HIL_TRANSPORT_MVP_Receive_Abandon_Incompatible_Session( root );
                default:
                    return HIL_TRANSPORT_MVP_Receive_Fault( root );
            }
        }
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

    /*
     * Once a locally generated recovery RESET has been committed, newly
     * supplied bytes must be interpreted in the replacement establishment
     * state even if the caller has not made an intervening Process() call.
     * Reconsider the same boundary between complete bodies below so arbitrary
     * byte-stream chunking cannot make a following replacement frame stale.
     */
    status = HIL_TRANSPORT_MVP_Receive_Try_Advance_Recovery( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
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
                status = HIL_TRANSPORT_MVP_Receive_Try_Advance_Recovery( root );
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
                status = HIL_TRANSPORT_MVP_Receive_Try_Advance_Recovery( root );
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
