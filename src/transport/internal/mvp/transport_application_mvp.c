/**
 * @file transport_application_mvp.c
 * @brief Private Application-message delivery orchestration for the MVP profile.
 */
#include "transport_application_mvp.h"

#include "transport_control_output_mvp.h"
#include "transport_events_mvp.h"
#include "transport_frame_codec_mvp.h"
#include "transport_handshake_mvp.h"
#include "transport_reliability_mvp.h"
#include "transport_session_mvp.h"

#include <string.h>

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    return HIL_TRANSPORT_MVP_Session_Enter_Fault( root );
}

static int
HIL_TRANSPORT_MVP_Application_Has_Required_Storage( const HIL_Transport_Mvp_Root_T* root )
{
    return ( root != NULL ) && ( root->submitted_message != NULL )
           && ( root->encoded_output != NULL ) && ( root->received_message != NULL )
           && ( root->codec_scratch != NULL ) && ( root->encoded_output_capacity != 0u )
           && ( root->base.config.max_application_message_size != 0u )
           && ( root->base.config.max_encoded_frame_size == root->encoded_output_capacity )
           && ( root->base.config.max_application_message_size
                <= ( SIZE_MAX - HIL_TRANSPORT_MVP_RAW_OVERHEAD ) )
           && ( root->codec_scratch_size >= ( root->base.config.max_application_message_size
                                              + HIL_TRANSPORT_MVP_RAW_OVERHEAD ) );
}

static int
HIL_TRANSPORT_MVP_Application_Session_Is_Established( const HIL_Transport_Mvp_Root_T* root )
{
    return ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
           && ( root->session.state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
           && ( root->session.handshake_phase == HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED )
           && ( root->session.session_identifier_valid != 0u )
           && ( root->session.session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID )
           && ( root->session.session_identifier != HIL_TRANSPORT_SESSION_SEED_RESERVED );
}

static int HIL_TRANSPORT_MVP_Application_Received_Message_Metadata_Is_Valid(
    const HIL_Transport_Mvp_Root_T* root )
{
    if ( ( root == NULL ) || ( root->received_message_pending > 1u )
         || ( root->received_message_size > root->base.config.max_application_message_size ) )
    {
        return 0;
    }
    if ( root->received_message_pending == 0u )
    {
        return root->received_message_size == 0u;
    }
    return root->received_message_size != 0u;
}

static int HIL_TRANSPORT_MVP_Application_Session_Identifier_Is_Previous(
    uint64_t current_session_identifier, uint64_t candidate_session_identifier )
{
    const uint64_t previous_session_identifier = current_session_identifier == 1u
                                                     ? HIL_TRANSPORT_SESSION_SEED_RESERVED - 1u
                                                     : current_session_identifier - 1u;

    return candidate_session_identifier == previous_session_identifier;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Application_Publish_Acknowledgement( HIL_Transport_Mvp_Root_T* root,
                                                       uint16_t                  sequence )
{
    uint8_t                   encoded_control[HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY];
    HIL_Transport_Mvp_Frame_T frame;
    HIL_Transport_Status_T    status;
    size_t                    encoded_size = 0u;

    frame.type                     = HIL_TRANSPORT_MVP_FRAME_ACK;
    frame.session_identifier       = root->session.session_identifier;
    frame.sequence                 = 0u;
    frame.acknowledgement_sequence = sequence;
    frame.payload                  = NULL;
    frame.payload_size             = 0u;
    status                         = HIL_TRANSPORT_MVP_Encode_Frame(
        &frame, root->base.config.max_application_message_size, root->codec_scratch,
        root->codec_scratch_size, encoded_control, sizeof( encoded_control ), &encoded_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status =
        HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( root, encoded_control, encoded_size );
    if ( status == HIL_TRANSPORT_STATUS_NOT_READY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    return status;
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Publish_Delivery_Event(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Event_Type_T event_type,
    HIL_Transport_Status_T status, HIL_Transport_Failure_T failure )
{
    const HIL_Transport_Event_T event = { event_type, status, failure, 0u };

    return HIL_TRANSPORT_MVP_Events_Publish( root, &event );
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Submit( HIL_Transport_Mvp_Root_T* root,
                                                             const uint8_t*            payload,
                                                             size_t payload_size )
{
    HIL_Transport_Mvp_Frame_T frame;
    HIL_Transport_Status_T    status;
    uint16_t                  sequence;
    size_t                    encoded_size = 0u;

    if ( ( root == NULL ) || ( payload == NULL ) || ( payload_size == 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_MVP_Application_Session_Is_Established( root ) )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( !HIL_TRANSPORT_MVP_Application_Has_Required_Storage( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( payload_size > root->base.config.max_application_message_size )
    {
        return HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE;
    }
    if ( ( root->submitted_message_pending != 0u ) || ( root->submitted_message_size != 0u )
         || ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_IDLE ) )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }

    status = HIL_TRANSPORT_MVP_Session_Reserve_Sequence( &root->session, &sequence );
    if ( status == HIL_TRANSPORT_STATUS_NOT_READY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    memcpy( root->submitted_message, payload, payload_size );
    frame.type                     = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
    frame.session_identifier       = root->session.session_identifier;
    frame.sequence                 = sequence;
    frame.acknowledgement_sequence = 0u;
    frame.payload                  = root->submitted_message;
    frame.payload_size             = payload_size;

    status = HIL_TRANSPORT_MVP_Encode_Frame( &frame, root->base.config.max_application_message_size,
                                             root->codec_scratch, root->codec_scratch_size,
                                             root->encoded_output, root->encoded_output_capacity,
                                             &encoded_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    status = HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
        root, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, sequence, encoded_size );
    if ( status == HIL_TRANSPORT_STATUS_NOT_READY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    root->submitted_message_size    = payload_size;
    root->submitted_message_pending = 1u;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Handle_Acknowledgement(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Application_Ack_Result_T* result )
{
    HIL_Transport_Mvp_Ack_Result_T          acknowledgement_result;
    HIL_Transport_Mvp_Frame_Type_T          completed_type;
    HIL_Transport_Mvp_Reliability_Outcome_T reliability_outcome;
    HIL_Transport_Status_T                  status;

    if ( ( root == NULL ) || ( frame == NULL ) || ( result == NULL )
         || ( frame->type != HIL_TRANSPORT_MVP_FRAME_ACK ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *result = HIL_TRANSPORT_MVP_APPLICATION_ACK_STALE;
    if ( !HIL_TRANSPORT_MVP_Application_Has_Required_Storage( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( ( root->session.retained_reliable_frame_type
           != HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
         || ( root->submitted_message_pending == 0u ) || ( root->submitted_message_size == 0u ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( !HIL_TRANSPORT_MVP_Application_Session_Is_Established( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( frame->session_identifier != root->session.session_identifier )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement(
        &root->session, frame->acknowledgement_sequence, &acknowledgement_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( acknowledgement_result != HIL_TRANSPORT_MVP_ACK_MATCHED )
    {
        if ( ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED )
             && ( frame->acknowledgement_sequence == root->session.retained_transmit_sequence ) )
        {
            return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
        }
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Events_Check_Capacity( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
        root, frame->acknowledgement_sequence, &completed_type, &reliability_outcome );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( ( reliability_outcome != HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED )
         || ( completed_type != HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    root->submitted_message_size    = 0u;
    root->submitted_message_pending = 0u;
    status                          = HIL_TRANSPORT_MVP_Application_Publish_Delivery_Event(
        root, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED, HIL_TRANSPORT_STATUS_OK,
        HIL_TRANSPORT_FAILURE_NONE );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    *result = HIL_TRANSPORT_MVP_APPLICATION_ACK_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Handle_Received_Frame(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Application_Frame_Result_T* result )
{
    HIL_Transport_Mvp_Rx_Sequence_Result_T sequence_result;
    HIL_Transport_Status_T                 status;

    if ( ( root == NULL ) || ( frame == NULL ) || ( result == NULL )
         || ( frame->type != HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *result = HIL_TRANSPORT_MVP_APPLICATION_FRAME_INCOMPATIBLE;
    if ( !HIL_TRANSPORT_MVP_Application_Has_Required_Storage( root )
         || !HIL_TRANSPORT_MVP_Application_Received_Message_Metadata_Is_Valid( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( !HIL_TRANSPORT_MVP_Application_Session_Is_Established( root ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( ( frame->payload == NULL ) || ( frame->payload_size == 0u )
         || ( frame->payload_size > root->base.config.max_application_message_size )
         || ( frame->acknowledgement_sequence != 0u ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( frame->session_identifier != root->session.session_identifier )
    {
        if ( HIL_TRANSPORT_MVP_Application_Session_Identifier_Is_Previous(
                 root->session.session_identifier, frame->session_identifier ) )
        {
            *result = HIL_TRANSPORT_MVP_APPLICATION_FRAME_STALE;
        }
        return HIL_TRANSPORT_STATUS_OK;
    }

    status = HIL_TRANSPORT_MVP_Session_Classify_Sequence( &root->session, frame->sequence,
                                                          &sequence_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    if ( sequence_result == HIL_TRANSPORT_MVP_RX_SEQUENCE_DUPLICATE )
    {
        if ( ( root->session.last_accepted_receive_frame_type
               != HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
             || ( root->session.last_accepted_receive_acknowledgement_sequence != 0u ) )
        {
            return HIL_TRANSPORT_STATUS_OK;
        }
        status = HIL_TRANSPORT_MVP_Application_Publish_Acknowledgement( root, frame->sequence );
        if ( status != HIL_TRANSPORT_STATUS_OK )
        {
            return status;
        }
        *result = HIL_TRANSPORT_MVP_APPLICATION_FRAME_DUPLICATE;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( sequence_result != HIL_TRANSPORT_MVP_RX_SEQUENCE_EXPECTED )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( root->received_message_pending != 0u )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }

    /*
     * Decoder payload aliases codec_scratch. Copy it into currently unowned
     * received storage before ACK encoding reuses codec_scratch. Ownership is
     * still invisible until every fallible ACK publication step succeeds.
     */
    memcpy( root->received_message, frame->payload, frame->payload_size );
    status = HIL_TRANSPORT_MVP_Application_Publish_Acknowledgement( root, frame->sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    status = HIL_TRANSPORT_MVP_Session_Accept_Sequence( &root->session, frame->sequence );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    root->session.last_accepted_receive_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
    root->session.last_accepted_receive_acknowledgement_sequence = 0u;
    root->received_message_size                                  = frame->payload_size;
    root->received_message_pending                               = 1u;
    *result = HIL_TRANSPORT_MVP_APPLICATION_FRAME_ACCEPTED;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Read( HIL_Transport_Mvp_Root_T* root,
                                                           uint8_t*                  out_buffer,
                                                           size_t  out_buffer_size,
                                                           size_t* message_size )
{
    if ( message_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *message_size = 0u;
    if ( ( out_buffer == NULL ) && ( out_buffer_size != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_MVP_Application_Has_Required_Storage( root )
         || !HIL_TRANSPORT_MVP_Application_Received_Message_Metadata_Is_Valid( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( root->received_message_pending == 0u )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( !HIL_TRANSPORT_MVP_Application_Session_Is_Established( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    *message_size = root->received_message_size;
    if ( ( out_buffer == NULL ) || ( out_buffer_size < root->received_message_size ) )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    memcpy( out_buffer, root->received_message, root->received_message_size );
    root->received_message_size    = 0u;
    root->received_message_pending = 0u;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Application_Get_Pending_Status( HIL_Transport_Mvp_Root_T* root,
                                                  uint8_t* application_message_pending )
{
    if ( application_message_pending == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *application_message_pending = 0u;
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_MVP_Application_Received_Message_Metadata_Is_Valid( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    *application_message_pending = root->received_message_pending;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Application_Handle_Retry_Exhaustion( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T status;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_MVP_Application_Has_Required_Storage( root ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    if ( ( root->session.reliable_state != HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED )
         || ( root->session.retained_reliable_frame_type
              != HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
         || ( root->submitted_message_pending == 0u ) || ( root->submitted_message_size == 0u ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    status = HIL_TRANSPORT_MVP_Application_Publish_Delivery_Event(
        root, HIL_TRANSPORT_EVENT_DELIVERY_FAILED, HIL_TRANSPORT_STATUS_DELIVERY_FAILED,
        HIL_TRANSPORT_FAILURE_DELIVERY );
    if ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED )
    {
        return status;
    }
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }

    status =
        HIL_TRANSPORT_MVP_Handshake_Begin_Local_Recovery( root, HIL_TRANSPORT_FAILURE_DELIVERY );
    if ( status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
    {
        return status;
    }
    if ( ( status != HIL_TRANSPORT_STATUS_OK )
         && ( status != HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_MVP_Application_Record_Invariant_Failure( root );
    }
    return HIL_TRANSPORT_STATUS_DELIVERY_FAILED;
}
