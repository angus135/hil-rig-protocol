/**
 * @file transport_handshake_mvp.c
 * @brief Private semantic coordinator for the MVP session handshake.
 */
#include "transport_handshake_mvp.h"

#include "transport_control_output_mvp.h"
#include "transport_events_mvp.h"
#include "transport_frame_codec_mvp.h"
#include "transport_reliability_mvp.h"
#include "transport_session_mvp.h"

#include <stddef.h>

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Validate_Root( HIL_Transport_Mvp_Root_T* root )
{
    if ( ( root->session.role < HIL_TRANSPORT_ROLE_HOST )
         || ( root->session.role > HIL_TRANSPORT_ROLE_RIG )
         || ( root->base.role != root->session.role )
         || ( root->session.link_state < HIL_TRANSPORT_LINK_STATE_DISCONNECTED )
         || ( root->session.link_state > HIL_TRANSPORT_LINK_STATE_CONNECTED )
         || ( root->base.link_state != root->session.link_state )
         || ( root->session.state < HIL_TRANSPORT_SESSION_STATE_DISCONNECTED )
         || ( root->session.state > HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->base.session_state != root->session.state )
         || ( root->base.last_failure != root->session.last_failure )
         || ( root->session.handshake_phase < HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE )
         || ( root->session.handshake_phase > HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED )
         || ( root->session.session_identifier_valid > 1u ) )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    return HIL_TRANSPORT_STATUS_OK;
}

static int HIL_TRANSPORT_MVP_Handshake_Session_Identifier_Is_Valid( uint64_t session_identifier )
{
    return ( session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
           && ( session_identifier != HIL_TRANSPORT_SESSION_SEED_RESERVED );
}

static int
HIL_TRANSPORT_MVP_Handshake_Frame_Has_Empty_Payload( const HIL_Transport_Mvp_Frame_T* frame )
{
    return frame->payload_size == 0u;
}

static int
HIL_TRANSPORT_MVP_Handshake_Frame_Is_Exact_Duplicate( const HIL_Transport_Mvp_Session_T* session,
                                                      const HIL_Transport_Mvp_Frame_T*   frame )
{
    return ( session->accepted_receive_sequence_valid != 0u )
           && ( frame->sequence == session->last_accepted_receive_sequence )
           && ( frame->type == session->last_accepted_receive_frame_type )
           && ( frame->acknowledgement_sequence
                == session->last_accepted_receive_acknowledgement_sequence );
}

static void
HIL_TRANSPORT_MVP_Handshake_Record_Accepted_Frame( HIL_Transport_Mvp_Session_T*     session,
                                                   const HIL_Transport_Mvp_Frame_T* frame )
{
    session->last_accepted_receive_frame_type               = frame->type;
    session->last_accepted_receive_acknowledgement_sequence = frame->acknowledgement_sequence;
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Publish_Reliable(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Mvp_Frame_Type_T frame_type,
    uint16_t acknowledgement_sequence, HIL_Transport_Mvp_Handshake_Phase_T next_phase )
{
    HIL_Transport_Mvp_Frame_T frame;
    HIL_Transport_Status_T    status;
    uint16_t                  sequence;
    size_t                    encoded_size = 0u;

    status = HIL_TRANSPORT_MVP_Session_Reserve_Sequence( &root->session, &sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    frame.type                     = frame_type;
    frame.session_identifier       = root->session.session_identifier;
    frame.sequence                 = sequence;
    frame.acknowledgement_sequence = acknowledgement_sequence;
    frame.payload                  = NULL;
    frame.payload_size             = 0u;
    status = HIL_TRANSPORT_MVP_Encode_Frame( &frame, root->base.config.max_application_message_size,
                                             root->codec_scratch, root->codec_scratch_size,
                                             root->encoded_output, root->encoded_output_capacity,
                                             &encoded_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status =
        HIL_TRANSPORT_MVP_Reliability_Publish_Encoded( root, frame_type, sequence, encoded_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    root->session.handshake_phase = next_phase;
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Publish_Control(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Mvp_Frame_Type_T frame_type,
    uint64_t session_identifier, uint16_t acknowledgement_sequence )
{
    uint8_t                   encoded_control[HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY];
    HIL_Transport_Mvp_Frame_T frame;
    HIL_Transport_Status_T    status;
    size_t                    encoded_size = 0u;

    frame.type                     = frame_type;
    frame.session_identifier       = session_identifier;
    frame.sequence                 = 0u;
    frame.acknowledgement_sequence = acknowledgement_sequence;
    frame.payload                  = NULL;
    frame.payload_size             = 0u;
    status                         = HIL_TRANSPORT_MVP_Encode_Frame(
        &frame, root->base.config.max_application_message_size, root->codec_scratch,
        root->codec_scratch_size, encoded_control, sizeof( encoded_control ), &encoded_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    return HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( root, encoded_control, encoded_size );
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Publish_Acknowledgement( HIL_Transport_Mvp_Root_T* root,
                                                     uint16_t                  sequence )
{
    HIL_Transport_Status_T status = HIL_TRANSPORT_MVP_Handshake_Publish_Control(
        root, HIL_TRANSPORT_MVP_FRAME_ACK, root->session.session_identifier, sequence );

    /* Semantic receive can retry once the different ready or pinned control item is committed. */
    if ( status == HIL_TRANSPORT_STATUS_NOT_READY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    return status;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Publish_Established_Event( HIL_Transport_Mvp_Root_T* root )
{
    const HIL_Transport_Event_T event = { HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED,
                                          HIL_TRANSPORT_STATUS_OK, HIL_TRANSPORT_FAILURE_NONE, 0u };
    return HIL_TRANSPORT_MVP_Events_Publish( root, &event );
}

static void HIL_TRANSPORT_MVP_Handshake_Set_Established( HIL_Transport_Mvp_Root_T* root )
{
    root->base.session_state      = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    root->session.state           = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    root->session.handshake_phase = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
    root->base.last_failure       = HIL_TRANSPORT_FAILURE_NONE;
    root->session.last_failure    = HIL_TRANSPORT_FAILURE_NONE;
}

typedef enum
{
    HIL_TRANSPORT_MVP_HANDSHAKE_ACK_UNEXPECTED = 0,
    HIL_TRANSPORT_MVP_HANDSHAKE_ACK_MATCHED,
    HIL_TRANSPORT_MVP_HANDSHAKE_ACK_BLOCKED_BY_PINNED_RETRY
} HIL_Transport_Mvp_Handshake_Ack_Result_T;

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Classify_Acknowledgement(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Frame_Type_T expected_type, HIL_Transport_Mvp_Handshake_Ack_Result_T* result )
{
    HIL_Transport_Mvp_Ack_Result_T acknowledgement_result;
    HIL_Transport_Status_T         status;

    *result = HIL_TRANSPORT_MVP_HANDSHAKE_ACK_UNEXPECTED;
    if ( root->session.retained_reliable_frame_type != expected_type )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement(
        &root->session, frame->acknowledgement_sequence, &acknowledgement_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( acknowledgement_result == HIL_TRANSPORT_MVP_ACK_MATCHED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_ACK_MATCHED;
    }
    else if ( ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED )
              && ( frame->acknowledgement_sequence == root->session.retained_transmit_sequence ) )
    {
        /* The exact ACK is valid, but the selected retry remains pinned until commit. */
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_ACK_BLOCKED_BY_PINNED_RETRY;
    }
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Complete_Acknowledgement( HIL_Transport_Mvp_Root_T* root,
                                                      uint16_t acknowledgement_sequence,
                                                      HIL_Transport_Mvp_Frame_Type_T expected_type )
{
    HIL_Transport_Mvp_Frame_Type_T          completed_type;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    HIL_Transport_Status_T                  status;

    status = HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( root, acknowledgement_sequence,
                                                                   &completed_type, &outcome );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( outcome != HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED )
         || ( completed_type != expected_type ) )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Handle_Duplicate_Response_Work(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Mvp_Frame_Type_T expected_response_type )
{
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;

    if ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_IDLE )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    return HIL_TRANSPORT_MVP_Reliability_Request_Retransmission( root, expected_response_type,
                                                                 &outcome );
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Handle_Host_Response(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Handshake_Frame_Result_T* result )
{
    HIL_Transport_Mvp_Rx_Sequence_Result_T   sequence_result;
    HIL_Transport_Status_T                   status;
    HIL_Transport_Mvp_Handshake_Ack_Result_T acknowledgement_result;

    if ( frame->session_identifier != root->session.session_identifier )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Session_Classify_Sequence( &root->session, frame->sequence,
                                                          &sequence_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_DUPLICATE )
    {
        if ( !HIL_TRANSPORT_MVP_Handshake_Frame_Is_Exact_Duplicate( &root->session, frame ) )
        {
            *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
            return HIL_TRANSPORT_STATUS_OK;
        }
        if ( ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING )
             || ( root->session.handshake_phase
                  == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK ) )
        {
            status = HIL_TRANSPORT_MVP_Handshake_Handle_Duplicate_Response_Work(
                root, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
            if ( status != HIL_TRANSPORT_STATUS_OK )
            {
                return status;
            }
            *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE;
            return HIL_TRANSPORT_STATUS_OK;
        }
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_STALE )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result != HIL_TRANSPORT_MVP_RX_SEQUENCE_EXPECTED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( root->session.handshake_phase != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Classify_Acknowledgement(
        root, frame, HIL_TRANSPORT_MVP_FRAME_INITIATE, &acknowledgement_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( acknowledgement_result == HIL_TRANSPORT_MVP_HANDSHAKE_ACK_BLOCKED_BY_PINNED_RETRY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( acknowledgement_result != HIL_TRANSPORT_MVP_HANDSHAKE_ACK_MATCHED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Handshake_Complete_Acknowledgement(
        root, frame->acknowledgement_sequence, HIL_TRANSPORT_MVP_FRAME_INITIATE );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Session_Accept_Sequence( &root->session, frame->sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    HIL_TRANSPORT_MVP_Handshake_Record_Accepted_Frame( &root->session, frame );
    root->session.handshake_phase = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING;
    *result                       = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Handle_Host_Ack( HIL_Transport_Mvp_Root_T*                   root,
                                             const HIL_Transport_Mvp_Frame_T*            frame,
                                             HIL_Transport_Mvp_Handshake_Frame_Result_T* result )
{
    HIL_Transport_Status_T                   status;
    HIL_Transport_Mvp_Handshake_Ack_Result_T acknowledgement_result;

    if ( ( root->session.handshake_phase
           != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK )
         || ( frame->session_identifier != root->session.session_identifier ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Classify_Acknowledgement(
        root, frame, HIL_TRANSPORT_MVP_FRAME_CONFIRM, &acknowledgement_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( acknowledgement_result == HIL_TRANSPORT_MVP_HANDSHAKE_ACK_BLOCKED_BY_PINNED_RETRY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( acknowledgement_result != HIL_TRANSPORT_MVP_HANDSHAKE_ACK_MATCHED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Events_Check_Capacity( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Complete_Acknowledgement(
        root, frame->acknowledgement_sequence, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    HIL_TRANSPORT_MVP_Handshake_Set_Established( root );
    status = HIL_TRANSPORT_MVP_Handshake_Publish_Established_Event( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Try_Complete_Host_From_Application(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Handshake_Application_Proof_Result_T* result )
{
    HIL_Transport_Mvp_Rx_Sequence_Result_T sequence_result;
    HIL_Transport_Status_T                 status;

    if ( result == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *result = HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_NOT_APPLICABLE;
    if ( ( root == NULL ) || ( frame == NULL )
         || ( frame->type != HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( HIL_TRANSPORT_MVP_Handshake_Validate_Root( root ) != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( ( root->session.role != HIL_TRANSPORT_ROLE_HOST )
         || ( root->session.state != HIL_TRANSPORT_SESSION_STATE_CONNECTING )
         || ( root->session.handshake_phase
              != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( frame->session_identifier != root->session.session_identifier )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( ( frame->payload == NULL ) || ( frame->payload_size == 0u )
         || ( frame->payload_size > root->base.config.max_application_message_size )
         || ( frame->acknowledgement_sequence != 0u ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Session_Classify_Sequence( &root->session, frame->sequence,
                                                          &sequence_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    /*
     * The previously accepted peer sequence belongs to RESPONSE, not
     * APPLICATION. Reusing it (or any older/current-session sequence) for a
     * different reliable frame type is incompatible rather than a duplicate
     * Application. Only the exact next sequence can prove CONFIRM acceptance.
     */
    if ( sequence_result != HIL_TRANSPORT_MVP_RX_SEQUENCE_EXPECTED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( root->session.retained_reliable_frame_type != HIL_TRANSPORT_MVP_FRAME_CONFIRM )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    if ( ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED )
         || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED ) )
    {
        /* Caller-owned peeked bytes cannot be invalidated until their routed commit. */
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
         && ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Events_Check_Capacity( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Complete_Acknowledgement(
        root, root->session.retained_transmit_sequence, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    HIL_TRANSPORT_MVP_Handshake_Set_Established( root );
    status = HIL_TRANSPORT_MVP_Handshake_Publish_Established_Event( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    *result = HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Handle_Rig_Initiate(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Handshake_Frame_Result_T* result )
{
    HIL_Transport_Mvp_Rx_Sequence_Result_T sequence_result;
    HIL_Transport_Status_T                 status;

    if ( !HIL_TRANSPORT_MVP_Handshake_Session_Identifier_Is_Valid( frame->session_identifier )
         || ( frame->acknowledgement_sequence != 0u ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( ( root->session.session_identifier_valid != 0u )
         && ( frame->session_identifier != root->session.session_identifier ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Session_Classify_Sequence( &root->session, frame->sequence,
                                                          &sequence_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_DUPLICATE )
    {
        if ( !HIL_TRANSPORT_MVP_Handshake_Frame_Is_Exact_Duplicate( &root->session, frame ) )
        {
            *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
            return HIL_TRANSPORT_STATUS_OK;
        }
        if ( ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING )
             || ( root->session.handshake_phase
                  == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM ) )
        {
            status = HIL_TRANSPORT_MVP_Handshake_Handle_Duplicate_Response_Work(
                root, HIL_TRANSPORT_MVP_FRAME_RESPONSE );
            if ( status != HIL_TRANSPORT_STATUS_OK )
            {
                return status;
            }
            *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE;
            return HIL_TRANSPORT_STATUS_OK;
        }
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_STALE )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result != HIL_TRANSPORT_MVP_RX_SEQUENCE_EXPECTED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( ( root->session.link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED )
         || ( root->session.state != HIL_TRANSPORT_SESSION_STATE_CONNECTING )
         || ( root->session.handshake_phase
              != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    root->session.session_identifier       = frame->session_identifier;
    root->session.session_identifier_valid = 1u;
    status = HIL_TRANSPORT_MVP_Session_Accept_Sequence( &root->session, frame->sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    HIL_TRANSPORT_MVP_Handshake_Record_Accepted_Frame( &root->session, frame );
    root->session.handshake_phase = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING;
    *result                       = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Handle_Rig_Confirm( HIL_Transport_Mvp_Root_T*                   root,
                                                const HIL_Transport_Mvp_Frame_T*            frame,
                                                HIL_Transport_Mvp_Handshake_Frame_Result_T* result )
{
    HIL_Transport_Mvp_Rx_Sequence_Result_T   sequence_result;
    HIL_Transport_Status_T                   status;
    HIL_Transport_Mvp_Handshake_Ack_Result_T acknowledgement_result;

    if ( frame->session_identifier != root->session.session_identifier )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Session_Classify_Sequence( &root->session, frame->sequence,
                                                          &sequence_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_DUPLICATE )
    {
        if ( !HIL_TRANSPORT_MVP_Handshake_Frame_Is_Exact_Duplicate( &root->session, frame ) )
        {
            *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
            return HIL_TRANSPORT_STATUS_OK;
        }
        if ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED )
        {
            status = HIL_TRANSPORT_MVP_Handshake_Publish_Acknowledgement( root, frame->sequence );
            if ( status != HIL_TRANSPORT_STATUS_OK )
            {
                return status;
            }
            *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE;
            return HIL_TRANSPORT_STATUS_OK;
        }
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_STALE )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result != HIL_TRANSPORT_MVP_RX_SEQUENCE_EXPECTED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( root->session.handshake_phase != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Classify_Acknowledgement(
        root, frame, HIL_TRANSPORT_MVP_FRAME_RESPONSE, &acknowledgement_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( acknowledgement_result == HIL_TRANSPORT_MVP_HANDSHAKE_ACK_BLOCKED_BY_PINNED_RETRY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( acknowledgement_result != HIL_TRANSPORT_MVP_HANDSHAKE_ACK_MATCHED )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    status = HIL_TRANSPORT_MVP_Events_Check_Capacity( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Publish_Acknowledgement( root, frame->sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Complete_Acknowledgement(
        root, frame->acknowledgement_sequence, HIL_TRANSPORT_MVP_FRAME_RESPONSE );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Session_Accept_Sequence( &root->session, frame->sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    HIL_TRANSPORT_MVP_Handshake_Record_Accepted_Frame( &root->session, frame );
    HIL_TRANSPORT_MVP_Handshake_Set_Established( root );
    status = HIL_TRANSPORT_MVP_Handshake_Publish_Established_Event( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
    }
    *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Handle_Reset( HIL_Transport_Mvp_Root_T*                   root,
                                          const HIL_Transport_Mvp_Frame_T*            frame,
                                          HIL_Transport_Mvp_Handshake_Frame_Result_T* result )
{
    HIL_Transport_Status_T status;

    if ( ( frame->sequence != 0u ) || ( frame->acknowledgement_sequence != 0u )
         || !HIL_TRANSPORT_MVP_Handshake_Session_Identifier_Is_Valid( frame->session_identifier ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( ( root->session.session_identifier_valid == 0u )
         || ( frame->session_identifier != root->session.session_identifier ) )
    {
        *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Session_Abandon( root, HIL_TRANSPORT_FAILURE_PROTOCOL );
    if ( ( status != HIL_TRANSPORT_STATUS_OK )
         && ( status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return status;
    }

    /*
     * Peer RESET already provides the required synchronization signal. Prepare
     * the connected endpoint for replacement establishment immediately so a
     * following INITIATE in the same byte chunk cannot be consumed as stale.
     */
    if ( root->session.link_state == HIL_TRANSPORT_LINK_STATE_CONNECTED )
    {
        HIL_Transport_Status_T establishment_status =
            HIL_TRANSPORT_MVP_Session_Begin_Establishment( root );
        if ( establishment_status != HIL_TRANSPORT_STATUS_OK )
        {
            return establishment_status;
        }
    }

    *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED;
    return status;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Process( HIL_Transport_Mvp_Root_T* root,
                                                            uint32_t                  now_ms )
{
    HIL_Transport_Status_T status;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    ( void )now_ms;
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    status = HIL_TRANSPORT_MVP_Handshake_Validate_Root( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING )
         || ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING )
         || ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING ) )
    {
        if ( ( root->session.link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED )
             || ( root->session.state != HIL_TRANSPORT_SESSION_STATE_CONNECTING )
             || ( root->session.session_identifier_valid == 0u )
             || !HIL_TRANSPORT_MVP_Handshake_Session_Identifier_Is_Valid(
                 root->session.session_identifier ) )
        {
            return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
        }
    }

    switch ( root->session.handshake_phase )
    {
        case HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING:
            if ( root->session.role != HIL_TRANSPORT_ROLE_HOST )
            {
                return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
            }
            return HIL_TRANSPORT_MVP_Handshake_Publish_Reliable(
                root, HIL_TRANSPORT_MVP_FRAME_INITIATE, 0u,
                HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
        case HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING:
            if ( root->session.role != HIL_TRANSPORT_ROLE_RIG )
            {
                return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
            }
            return HIL_TRANSPORT_MVP_Handshake_Publish_Reliable(
                root, HIL_TRANSPORT_MVP_FRAME_RESPONSE,
                root->session.last_accepted_receive_sequence,
                HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM );
        case HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING:
            if ( root->session.role != HIL_TRANSPORT_ROLE_HOST )
            {
                return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
            }
            return HIL_TRANSPORT_MVP_Handshake_Publish_Reliable(
                root, HIL_TRANSPORT_MVP_FRAME_CONFIRM, root->session.last_accepted_receive_sequence,
                HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );
        default:
            return HIL_TRANSPORT_STATUS_OK;
    }
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Handle_Frame( HIL_Transport_Mvp_Root_T*                   root,
                                          const HIL_Transport_Mvp_Frame_T*            frame,
                                          HIL_Transport_Mvp_Handshake_Frame_Result_T* result )
{
    if ( result == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *result = HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE;
    if ( ( root == NULL ) || ( frame == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( HIL_TRANSPORT_MVP_Handshake_Validate_Root( root ) != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( !HIL_TRANSPORT_MVP_Handshake_Frame_Has_Empty_Payload( frame ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( frame->type == HIL_TRANSPORT_MVP_FRAME_RESET )
    {
        return HIL_TRANSPORT_MVP_Handshake_Handle_Reset( root, frame, result );
    }

    if ( root->session.role == HIL_TRANSPORT_ROLE_HOST )
    {
        if ( frame->type == HIL_TRANSPORT_MVP_FRAME_RESPONSE )
        {
            return HIL_TRANSPORT_MVP_Handshake_Handle_Host_Response( root, frame, result );
        }
        if ( ( frame->type == HIL_TRANSPORT_MVP_FRAME_ACK ) && ( frame->sequence == 0u ) )
        {
            return HIL_TRANSPORT_MVP_Handshake_Handle_Host_Ack( root, frame, result );
        }
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( root->session.role == HIL_TRANSPORT_ROLE_RIG )
    {
        if ( frame->type == HIL_TRANSPORT_MVP_FRAME_INITIATE )
        {
            return HIL_TRANSPORT_MVP_Handshake_Handle_Rig_Initiate( root, frame, result );
        }
        if ( frame->type == HIL_TRANSPORT_MVP_FRAME_CONFIRM )
        {
            return HIL_TRANSPORT_MVP_Handshake_Handle_Rig_Confirm( root, frame, result );
        }
        return HIL_TRANSPORT_STATUS_OK;
    }
    return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Publish_Reset( HIL_Transport_Mvp_Root_T* root,
                                                                  uint64_t session_identifier )
{
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_MVP_Handshake_Session_Identifier_Is_Valid( session_identifier ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_MVP_Handshake_Publish_Control( root, HIL_TRANSPORT_MVP_FRAME_RESET,
                                                        session_identifier, 0u );
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Begin_Local_Recovery( HIL_Transport_Mvp_Root_T* root,
                                                  HIL_Transport_Failure_T   failure )
{
    HIL_Transport_Status_T abandon_status;
    HIL_Transport_Status_T reset_status = HIL_TRANSPORT_STATUS_OK;
    uint64_t               failed_session_identifier;
    uint8_t                failed_session_identifier_valid;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    failed_session_identifier = root->session.session_identifier;
    failed_session_identifier_valid =
        ( uint8_t )( ( root->session.link_state == HIL_TRANSPORT_LINK_STATE_CONNECTED )
                     && ( root->session.session_identifier_valid != 0u )
                     && HIL_TRANSPORT_MVP_Handshake_Session_Identifier_Is_Valid(
                         failed_session_identifier ) );

    abandon_status = HIL_TRANSPORT_MVP_Session_Abandon( root, failure );
    if ( ( abandon_status != HIL_TRANSPORT_STATUS_OK )
         && ( abandon_status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return abandon_status;
    }

    /* Event backpressure is observational; it must not suppress wire recovery. */
    if ( failed_session_identifier_valid != 0u )
    {
        reset_status = HIL_TRANSPORT_MVP_Handshake_Publish_Reset( root, failed_session_identifier );
        if ( reset_status != HIL_TRANSPORT_STATUS_OK )
        {
            return HIL_TRANSPORT_MVP_Handshake_Record_Invariant_Failure( root );
        }
        root->recovery_reset_pending            = 1u;
        root->recovery_reset_session_identifier = failed_session_identifier;
    }

    return abandon_status;
}
