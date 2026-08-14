#include "transport_session_mvp.h"

#include "transport_events_mvp.h"
#include "transport_output_mvp.h"

#include <string.h>

static void HIL_TRANSPORT_MVP_Session_Set_Link_State( HIL_Transport_Mvp_Root_T* root,
                                                      HIL_Transport_Link_State_T link_state )
{
    root->base.link_state    = link_state;
    root->session.link_state = link_state;
}

static void HIL_TRANSPORT_MVP_Session_Set_State( HIL_Transport_Mvp_Root_T* root,
                                                 HIL_Transport_Session_State_T state )
{
    root->base.session_state = state;
    root->session.state      = state;
}

static void HIL_TRANSPORT_MVP_Session_Set_Failure( HIL_Transport_Mvp_Root_T* root,
                                                   HIL_Transport_Failure_T failure )
{
    root->base.last_failure = failure;
    root->session.last_failure = failure;
}

static void HIL_TRANSPORT_MVP_Session_Set_Fault( HIL_Transport_Mvp_Root_T* root )
{
    HIL_TRANSPORT_MVP_Session_Set_State( root, HIL_TRANSPORT_SESSION_STATE_FAULT );
    HIL_TRANSPORT_MVP_Session_Set_Failure( root, HIL_TRANSPORT_FAILURE_INTERNAL );
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Clear_Scoped_Work( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T output_status;

    output_status = HIL_TRANSPORT_MVP_Output_Reset( root );

    HIL_TRANSPORT_Parser_Reset( &root->parser );
    root->submitted_message_size                  = 0u;
    root->submitted_message_pending               = 0u;
    root->received_message_size                   = 0u;
    root->received_message_pending                = 0u;
    root->session.session_identifier              = HIL_TRANSPORT_SESSION_SEED_INVALID;
    root->session.session_identifier_valid        = 0u;
    root->session.handshake_phase                 = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE;
    root->session.next_transmit_sequence          = root->session.initial_reliable_sequence;
    root->session.expected_receive_sequence       = root->session.initial_reliable_sequence;
    root->session.last_accepted_receive_sequence  = 0u;
    root->session.accepted_receive_sequence_valid = 0u;
    root->session.reliable_state                  = HIL_TRANSPORT_MVP_RELIABLE_IDLE;
    root->session.retained_reliable_frame_type    = HIL_TRANSPORT_MVP_FRAME_INVALID;
    root->session.retained_transmit_sequence      = 0u;
    root->session.retransmissions_committed       = 0u;
    root->session.reliable_last_committed_ms      = 0u;
    root->session.last_valid_receive_ms           = 0u;
    root->encoded_output_size                     = 0u;
    root->control_output_size                     = 0u;
    root->control_output_state                    = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE;
    root->output_selection                        = HIL_TRANSPORT_MVP_OUTPUT_NONE;

    if ( output_status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    ( void )HIL_TRANSPORT_MVP_Session_Clear_Scoped_Work( root );
    HIL_TRANSPORT_MVP_Session_Set_Fault( root );
    return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Reset_Status_For_Failure( HIL_Transport_Failure_T failure,
                                                    HIL_Transport_Status_T* status )
{
    if ( status == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    switch ( failure )
    {
        case HIL_TRANSPORT_FAILURE_LINK_LOST:
        case HIL_TRANSPORT_FAILURE_PROTOCOL:
            *status = HIL_TRANSPORT_STATUS_NOT_READY;
            return HIL_TRANSPORT_STATUS_OK;
        case HIL_TRANSPORT_FAILURE_CONNECTION_TIMEOUT:
            *status = HIL_TRANSPORT_STATUS_TIMEOUT;
            return HIL_TRANSPORT_STATUS_OK;
        case HIL_TRANSPORT_FAILURE_DELIVERY:
            *status = HIL_TRANSPORT_STATUS_DELIVERY_FAILED;
            return HIL_TRANSPORT_STATUS_OK;
        case HIL_TRANSPORT_FAILURE_CAPACITY:
            *status = HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
            return HIL_TRANSPORT_STATUS_OK;
        default:
            return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Publish_Link_Event( HIL_Transport_Mvp_Root_T* root,
                                              HIL_Transport_Link_State_T link_state )
{
    HIL_Transport_Event_T event;

    event.type              = HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED;
    event.status            = HIL_TRANSPORT_STATUS_OK;
    event.failure           = link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED
                                  ? HIL_TRANSPORT_FAILURE_LINK_LOST
                                  : HIL_TRANSPORT_FAILURE_NONE;
    event.required_capacity = 0u;
    return HIL_TRANSPORT_MVP_Events_Publish( root, &event );
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Init( HIL_Transport_Mvp_Session_T* session,
                                                       HIL_Transport_Role_T         role,
                                                       uint64_t                     session_seed,
                                                       uint16_t initial_reliable_sequence )
{
    HIL_Transport_Mvp_Session_T initialized_session;

    if ( session == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( role != HIL_TRANSPORT_ROLE_HOST ) && ( role != HIL_TRANSPORT_ROLE_RIG ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( ( role == HIL_TRANSPORT_ROLE_HOST )
           && ( ( session_seed == HIL_TRANSPORT_SESSION_SEED_INVALID )
                || ( session_seed == HIL_TRANSPORT_SESSION_SEED_RESERVED ) ) )
         || ( ( role == HIL_TRANSPORT_ROLE_RIG )
              && ( session_seed != HIL_TRANSPORT_SESSION_SEED_INVALID ) ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    memset( &initialized_session, 0, sizeof( initialized_session ) );
    initialized_session.role                         = role;
    initialized_session.link_state                   = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    initialized_session.link_state_observed          = 0u;
    initialized_session.state                        = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
    initialized_session.handshake_phase              = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE;
    initialized_session.next_host_session_identifier = session_seed;
    initialized_session.initial_reliable_sequence    = initial_reliable_sequence;
    initialized_session.next_transmit_sequence       = initial_reliable_sequence;
    initialized_session.expected_receive_sequence    = initial_reliable_sequence;
    initialized_session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_INVALID;
    initialized_session.reliable_state               = HIL_TRANSPORT_MVP_RELIABLE_IDLE;
    initialized_session.last_failure                 = HIL_TRANSPORT_FAILURE_NONE;

    *session = initialized_session;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Begin_Establishment( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Mvp_Session_T* session;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    session = &root->session;

    if ( ( session->role < HIL_TRANSPORT_ROLE_HOST ) || ( session->role > HIL_TRANSPORT_ROLE_RIG )
         || ( root->base.role != session->role )
         || ( session->link_state < HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         || ( session->link_state > HIL_TRANSPORT_LINK_STATE_CONNECTED )
         || ( root->base.link_state != session->link_state )
         || ( session->link_state_observed > 1u )
         || ( session->state < HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         || ( session->state > HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->base.session_state != session->state ) )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }
    if ( ( session->state != HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         && ( session->state != HIL_TRANSPORT_SESSION_STATE_RECOVERING ) )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( ( session->link_state_observed == 0u )
         || ( session->link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED ) )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    if ( ( session->handshake_phase != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE )
         || ( session->session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
         || ( session->session_identifier_valid != 0u )
         || ( session->reliable_state != HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( session->retained_reliable_frame_type != HIL_TRANSPORT_MVP_FRAME_INVALID )
         || ( session->retained_transmit_sequence != 0u )
         || ( session->retransmissions_committed != 0u )
         || ( session->reliable_last_committed_ms != 0u )
         || ( root->encoded_output_size != 0u )
         || ( root->control_output_state != HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE )
         || ( root->control_output_size != 0u )
         || ( root->output_selection != HIL_TRANSPORT_MVP_OUTPUT_NONE )
         || ( root->submitted_message_size != 0u )
         || ( root->submitted_message_pending != 0u )
         || ( root->received_message_size != 0u )
         || ( root->received_message_pending != 0u )
         || ( root->parser.accumulated_size != 0u ) || ( root->parser.body_ready != 0u )
         || ( root->parser.discarding != 0u )
         || ( session->last_accepted_receive_sequence != 0u )
         || ( session->accepted_receive_sequence_valid != 0u )
         || ( session->last_valid_receive_ms != 0u ) )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }

    session->next_transmit_sequence              = session->initial_reliable_sequence;
    session->expected_receive_sequence           = session->initial_reliable_sequence;
    session->last_accepted_receive_sequence      = 0u;
    session->accepted_receive_sequence_valid     = 0u;

    if ( session->role == HIL_TRANSPORT_ROLE_HOST )
    {
        if ( ( session->next_host_session_identifier == HIL_TRANSPORT_SESSION_SEED_INVALID )
             || ( session->next_host_session_identifier == HIL_TRANSPORT_SESSION_SEED_RESERVED ) )
        {
            return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
        }
        session->session_identifier       = session->next_host_session_identifier;
        session->session_identifier_valid = 1u;
        if ( session->next_host_session_identifier == ( HIL_TRANSPORT_SESSION_SEED_RESERVED - 1u ) )
        {
            session->next_host_session_identifier = 1u;
        }
        else
        {
            session->next_host_session_identifier++;
        }
        session->handshake_phase = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING;
    }
    else
    {
        if ( session->next_host_session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
        {
            return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
        }
        session->handshake_phase = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE;
    }

    HIL_TRANSPORT_MVP_Session_Set_State( root, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Abandon( HIL_Transport_Mvp_Root_T* root,
                                                          HIL_Transport_Failure_T failure )
{
    HIL_Transport_Status_T event_status;
    HIL_Transport_Status_T associated_status;
    HIL_Transport_Event_T  event;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( HIL_TRANSPORT_MVP_Session_Reset_Status_For_Failure( failure, &associated_status )
         != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( root->base.session_state != root->session.state )
         || ( root->base.link_state != root->session.link_state )
         || ( root->session.state < HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         || ( root->session.state > HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.link_state < HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         || ( root->session.link_state > HIL_TRANSPORT_LINK_STATE_CONNECTED ) )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }

    if ( HIL_TRANSPORT_MVP_Session_Clear_Scoped_Work( root ) != HIL_TRANSPORT_STATUS_OK )
    {
        HIL_TRANSPORT_MVP_Session_Set_Fault( root );
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT )
    {
        HIL_TRANSPORT_MVP_Session_Set_Failure( root, HIL_TRANSPORT_FAILURE_INTERNAL );
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    HIL_TRANSPORT_MVP_Session_Set_Failure( root, failure );
    HIL_TRANSPORT_MVP_Session_Set_State(
        root, root->session.link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED
                  ? HIL_TRANSPORT_SESSION_STATE_DISCONNECTED
                  : HIL_TRANSPORT_SESSION_STATE_RECOVERING );

    event.type              = HIL_TRANSPORT_EVENT_SESSION_RESET;
    event.status            = associated_status;
    event.failure           = failure;
    event.required_capacity = 0u;
    event_status            = HIL_TRANSPORT_MVP_Events_Publish( root, &event );
    if ( event_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( event_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( event_status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Explicit_Reset( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T cleanup_status;
    int                    retained_link_is_valid;
    int                    retained_role_is_valid;
    int                    retained_setup_is_valid;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    cleanup_status = HIL_TRANSPORT_MVP_Session_Clear_Scoped_Work( root );
    ( void )HIL_TRANSPORT_MVP_Events_Reset( root );
    root->session.link_state_observed = ( uint8_t )( root->session.link_state_observed != 0u );
    retained_role_is_valid            = ( root->base.role == HIL_TRANSPORT_ROLE_HOST )
                             || ( root->base.role == HIL_TRANSPORT_ROLE_RIG );
    retained_link_is_valid = ( root->base.link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
                             || ( root->base.link_state == HIL_TRANSPORT_LINK_STATE_CONNECTED );
    if ( retained_role_is_valid )
    {
        root->session.role = root->base.role;
        if ( root->base.role == HIL_TRANSPORT_ROLE_RIG )
        {
            root->session.next_host_session_identifier = HIL_TRANSPORT_SESSION_SEED_INVALID;
        }
    }
    if ( retained_link_is_valid )
    {
        HIL_TRANSPORT_MVP_Session_Set_Link_State( root, root->base.link_state );
    }

    retained_setup_is_valid = retained_role_is_valid && retained_link_is_valid
                              && ( ( root->base.role == HIL_TRANSPORT_ROLE_RIG )
                                   || ( ( root->session.next_host_session_identifier
                                          != HIL_TRANSPORT_SESSION_SEED_INVALID )
                                        && ( root->session.next_host_session_identifier
                                             != HIL_TRANSPORT_SESSION_SEED_RESERVED ) ) );
    if ( ( cleanup_status != HIL_TRANSPORT_STATUS_OK ) || !retained_setup_is_valid )
    {
        HIL_TRANSPORT_MVP_Session_Set_Fault( root );
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    HIL_TRANSPORT_MVP_Session_Set_Failure( root, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    HIL_TRANSPORT_MVP_Session_Set_State(
        root, root->base.link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED
                  ? HIL_TRANSPORT_SESSION_STATE_DISCONNECTED
                  : HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Notify_Link_State(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Link_State_T link_state )
{
    HIL_Transport_Status_T link_event_status;
    HIL_Transport_Status_T transition_status;
    int                    was_observed;
    HIL_Transport_Link_State_T previous_link_state;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( link_state != HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         && ( link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        HIL_Transport_Status_T cleanup_status = HIL_TRANSPORT_STATUS_OK;

        root->session.link_state_observed = 1u;
        HIL_TRANSPORT_MVP_Session_Set_Link_State( root, link_state );
        if ( link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
        {
            cleanup_status = HIL_TRANSPORT_MVP_Session_Clear_Scoped_Work( root );
        }
        HIL_TRANSPORT_MVP_Session_Set_Fault( root );
        return cleanup_status;
    }

    if ( ( root->session.role < HIL_TRANSPORT_ROLE_HOST )
         || ( root->session.role > HIL_TRANSPORT_ROLE_RIG )
         || ( root->base.role != root->session.role )
         || ( root->base.link_state < HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         || ( root->base.link_state > HIL_TRANSPORT_LINK_STATE_CONNECTED )
         || ( root->base.link_state != root->session.link_state )
         || ( root->session.link_state_observed > 1u )
         || ( root->base.session_state < HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         || ( root->base.session_state > HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->base.session_state != root->session.state )
         || ( root->base.last_failure != root->session.last_failure ) )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }

    was_observed        = root->session.link_state_observed != 0u;
    previous_link_state = root->session.link_state;
    if ( was_observed && ( previous_link_state == link_state ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    root->session.link_state_observed = 1u;
    HIL_TRANSPORT_MVP_Session_Set_Link_State( root, link_state );
    if ( !was_observed && ( link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    link_event_status = HIL_TRANSPORT_MVP_Session_Publish_Link_Event( root, link_state );
    if ( link_event_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }
    if ( ( link_event_status != HIL_TRANSPORT_STATUS_OK )
         && ( link_event_status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }

    if ( link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
    {
        transition_status =
            HIL_TRANSPORT_MVP_Session_Abandon( root, HIL_TRANSPORT_FAILURE_LINK_LOST );
    }
    else
    {
        transition_status = HIL_TRANSPORT_MVP_Session_Begin_Establishment( root );
    }

    if ( transition_status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( ( link_event_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
         || ( transition_status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( transition_status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Session_Record_Invariant_Failure( root );
    }
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Reserve_Sequence( HIL_Transport_Mvp_Session_T* session,
                                            uint16_t*                    sequence )
{
    if ( sequence == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *sequence = 0u;
    if ( session == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( session->reliable_state < HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( session->reliable_state > HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( session->reliable_state != HIL_TRANSPORT_MVP_RELIABLE_IDLE )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( ( session->retained_reliable_frame_type != HIL_TRANSPORT_MVP_FRAME_INVALID )
         || ( session->retained_transmit_sequence != 0u )
         || ( session->retransmissions_committed != 0u )
         || ( session->reliable_last_committed_ms != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    /* The candidate is neither published nor consumed until encoding and ACK succeed. */
    *sequence = session->next_transmit_sequence;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Sequence( HIL_Transport_Mvp_Session_T*            session,
                                             uint16_t                                sequence,
                                             HIL_Transport_Mvp_Rx_Sequence_Result_T* result )
{
    /*
     * TODO: Validate and classify the expected sequence, the exact last accepted
     * duplicate, or incompatible traffic using approved wrap rules. Deliver and
     * advance expected data once; request another ACK without redelivery for the
     * duplicate; and require complete session restart for incompatibility. The
     * private result is mapped to public status/events by the MVP profile.
     */
    ( void )session;
    ( void )sequence;
    ( void )result;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement( HIL_Transport_Mvp_Session_T*    session,
                                                    uint16_t                        sequence,
                                                    HIL_Transport_Mvp_Ack_Result_T* result )
{
    if ( result == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *result = HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED;
    if ( session == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( session->reliable_state < HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( session->reliable_state > HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    /*
     * A timeout authorizes an unpinned retry; it does not invalidate the ACK
     * for the previously committed transmission. Once retry bytes are peeked,
     * pin-until-commit/reset ownership takes precedence for this MVP.
     */
    if ( ( ( session->reliable_state == HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
           || ( session->reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY ) )
         && ( sequence == session->retained_transmit_sequence ) )
    {
        *result = HIL_TRANSPORT_MVP_ACK_MATCHED;
    }
    return HIL_TRANSPORT_STATUS_OK;
}
