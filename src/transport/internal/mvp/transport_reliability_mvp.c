/**
 * @file transport_reliability_mvp.c
 * @brief Private one-item reliable encoded-output lifecycle for the MVP profile.
 */
#include "transport_reliability_mvp.h"

#include "transport_session_mvp.h"

#include <string.h>

static int
HIL_TRANSPORT_MVP_Reliability_Is_Reliable_Type( HIL_Transport_Mvp_Frame_Type_T frame_type )
{
    return ( frame_type == HIL_TRANSPORT_MVP_FRAME_INITIATE )
           || ( frame_type == HIL_TRANSPORT_MVP_FRAME_RESPONSE )
           || ( frame_type == HIL_TRANSPORT_MVP_FRAME_CONFIRM )
           || ( frame_type == HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
}

static int HIL_TRANSPORT_MVP_Reliability_Has_Storage( const HIL_Transport_Mvp_Root_T* root )
{
    return ( root != NULL ) && ( root->encoded_output != NULL )
           && ( root->encoded_output_capacity != 0u )
           && ( root->encoded_output_capacity == root->base.config.max_encoded_frame_size );
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
}

static void HIL_TRANSPORT_MVP_Reliability_Clear_Metadata( HIL_Transport_Mvp_Root_T* root )
{
    root->session.reliable_state               = HIL_TRANSPORT_MVP_RELIABLE_IDLE;
    root->session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_INVALID;
    root->session.retained_transmit_sequence   = 0u;
    root->session.retransmissions_committed    = 0u;
    root->session.reliable_last_committed_ms   = 0u;
    root->encoded_output_size                  = 0u;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Reliability_Validate_State( HIL_Transport_Mvp_Root_T* root )
{
    const HIL_Transport_Mvp_Reliable_State_T state     = root->session.reliable_state;
    const int                                is_active = state != HIL_TRANSPORT_MVP_RELIABLE_IDLE;

    if ( ( state < HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( state > HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED )
         || ( root->session.retransmissions_committed > root->base.config.max_retries ) )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }

    if ( !is_active )
    {
        if ( ( root->encoded_output_size != 0u )
             || ( root->session.retained_reliable_frame_type != HIL_TRANSPORT_MVP_FRAME_INVALID )
             || ( root->session.retained_transmit_sequence != 0u )
             || ( root->session.retransmissions_committed != 0u )
             || ( root->session.reliable_last_committed_ms != 0u ) )
        {
            return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
        }
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( ( root->encoded_output_size == 0u )
         || ( root->encoded_output_size > root->encoded_output_capacity )
         || !HIL_TRANSPORT_MVP_Reliability_Is_Reliable_Type(
             root->session.retained_reliable_frame_type )
         || ( root->session.retained_transmit_sequence != root->session.next_transmit_sequence ) )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }

    if ( ( ( state == HIL_TRANSPORT_MVP_RELIABLE_READY )
           || ( state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED ) )
         && ( ( root->session.retransmissions_committed != 0u )
              || ( root->session.reliable_last_committed_ms != 0u ) ) )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }

    if ( ( ( state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY )
           || ( state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED ) )
         && ( root->session.retransmissions_committed >= root->base.config.max_retries ) )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }

    if ( ( state == HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED )
         && ( root->session.retransmissions_committed != root->base.config.max_retries ) )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }

    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Reliability_Publish_Encoded( HIL_Transport_Mvp_Root_T*      root,
                                               HIL_Transport_Mvp_Frame_Type_T frame_type,
                                               uint16_t sequence, size_t encoded_size )
{
    HIL_Transport_Status_T status;

    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_IDLE )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( ( encoded_size == 0u ) || ( encoded_size > root->encoded_output_capacity )
         || !HIL_TRANSPORT_MVP_Reliability_Is_Reliable_Type( frame_type )
         || ( sequence != root->session.next_transmit_sequence ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    root->session.retained_reliable_frame_type = frame_type;
    root->session.retained_transmit_sequence   = sequence;
    root->session.retransmissions_committed    = 0u;
    root->session.reliable_last_committed_ms   = 0u;
    root->encoded_output_size                  = encoded_size;
    root->session.reliable_state               = HIL_TRANSPORT_MVP_RELIABLE_READY;

    /* Publication owns the candidate but ACK completion alone consumes it. */
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Peek_Output( HIL_Transport_Mvp_Root_T* root,
                                                                  uint8_t* out_buffer,
                                                                  size_t   out_buffer_size,
                                                                  size_t*  output_size )
{
    HIL_Transport_Status_T status;

    if ( output_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0u;
    if ( ( out_buffer == NULL ) && ( out_buffer_size != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
         || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    *output_size = root->encoded_output_size;
    if ( ( out_buffer == NULL ) || ( out_buffer_size < root->encoded_output_size ) )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    memcpy( out_buffer, root->encoded_output, root->encoded_output_size );
    if ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_READY )
    {
        root->session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_PEEKED;
    }
    else if ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY )
    {
        root->session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED;
    }

    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Commit_Output( HIL_Transport_Mvp_Root_T* root,
                                                                    uint32_t now_ms )
{
    HIL_Transport_Status_T status;

    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_PEEKED )
         && ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED ) )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    if ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED )
    {
        /* A retry is spent only when external I/O accepts it, never at timeout/peek. */
        ++root->session.retransmissions_committed;
    }
    root->session.reliable_last_committed_ms = now_ms;
    root->session.reliable_state             = HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK;

    /* The original encoded bytes remain the retry copy; no reconstruction occurs. */
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
    HIL_Transport_Mvp_Root_T* root, uint16_t acknowledgement_sequence,
    HIL_Transport_Mvp_Frame_Type_T*          completed_frame_type,
    HIL_Transport_Mvp_Reliability_Outcome_T* outcome )
{
    HIL_Transport_Status_T         status;
    HIL_Transport_Mvp_Ack_Result_T classification;

    if ( ( completed_frame_type == NULL ) || ( outcome == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *completed_frame_type = HIL_TRANSPORT_MVP_FRAME_INVALID;
    *outcome              = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;
    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement(
        &root->session, acknowledgement_sequence, &classification );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }
    if ( classification != HIL_TRANSPORT_MVP_ACK_MATCHED )
    {
        /* A peeked retry stays pinned until commit/reset; stale ACKs change nothing. */
        return HIL_TRANSPORT_STATUS_OK;
    }

    /* RETRANSMIT_READY has no pinned output, so the exact ACK cancels that retry. */
    *completed_frame_type = root->session.retained_reliable_frame_type;
    root->session.next_transmit_sequence =
        ( uint16_t )( root->session.retained_transmit_sequence + UINT16_C( 1 ) );
    /* Invalidating ownership is sufficient; clearing the large buffer is wasted work. */
    HIL_TRANSPORT_MVP_Reliability_Clear_Metadata( root );
    *outcome = HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Reliability_Process_Pending( HIL_Transport_Mvp_Root_T* root, uint32_t now_ms,
                                               HIL_Transport_Mvp_Reliability_Outcome_T* outcome )
{
    HIL_Transport_Status_T status;
    uint32_t               elapsed_ms;

    if ( outcome == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *outcome = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;
    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
         || ( root->base.config.retransmit_timeout_ms == 0u ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    /* Unsigned subtraction deliberately preserves elapsed time across timer wrap. */
    elapsed_ms = now_ms - root->session.reliable_last_committed_ms;
    if ( elapsed_ms < root->base.config.retransmit_timeout_ms )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( root->session.retransmissions_committed < root->base.config.max_retries )
    {
        root->session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY;
        *outcome                     = HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY;
    }
    else
    {
        root->session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED;
        /* The session owner, not this byte-retention primitive, chooses recovery. */
        *outcome = HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED;
    }

    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Mvp_Frame_Type_T expected_frame_type,
    HIL_Transport_Mvp_Reliability_Outcome_T* outcome )
{
    HIL_Transport_Status_T status;

    if ( outcome == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *outcome = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;
    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root )
         || !HIL_TRANSPORT_MVP_Reliability_Is_Reliable_Type( expected_frame_type ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_IDLE )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( root->session.retained_reliable_frame_type != expected_frame_type )
    {
        return HIL_TRANSPORT_MVP_Reliability_Record_Invariant_Failure( root );
    }
    if ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( root->session.retransmissions_committed < root->base.config.max_retries )
    {
        root->session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY;
        *outcome                     = HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY;
    }
    else
    {
        root->session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED;
        *outcome                     = HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED;
    }
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Reset( HIL_Transport_Mvp_Root_T* root )
{
    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    /* Ownership metadata, not buffer erasure, makes every old byte inaccessible. */
    HIL_TRANSPORT_MVP_Reliability_Clear_Metadata( root );
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Get_Pending_Status(
    HIL_Transport_Mvp_Root_T* root, uint8_t* output_pending, uint8_t* delivery_pending )
{
    HIL_Transport_Status_T status;

    if ( ( output_pending == NULL ) || ( delivery_pending == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *output_pending   = 0u;
    *delivery_pending = 0u;
    if ( !HIL_TRANSPORT_MVP_Reliability_Has_Storage( root ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Reliability_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    *output_pending =
        ( uint8_t )( ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_READY )
                     || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED )
                     || ( root->session.reliable_state
                          == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY )
                     || ( root->session.reliable_state
                          == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED ) );
    *delivery_pending =
        ( uint8_t )( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    return HIL_TRANSPORT_STATUS_OK;
}
