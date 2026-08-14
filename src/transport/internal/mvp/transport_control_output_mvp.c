/**
 * @file transport_control_output_mvp.c
 * @brief Private one-item encoded control-output lifecycle for the MVP profile.
 */
#include "transport_control_output_mvp.h"

#include <string.h>

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Control_Output_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    /* Both views record the fault so public status and private policy agree. */
    root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Control_Output_Validate_State( HIL_Transport_Mvp_Root_T* root )
{
    switch ( root->control_output_state )
    {
        case HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE:
            if ( root->control_output_size == 0u )
            {
                return HIL_TRANSPORT_STATUS_OK;
            }
            break;

        case HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY:
        case HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED:
            if ( ( root->control_output_size != 0u )
                 && ( root->control_output_size <= HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY ) )
            {
                return HIL_TRANSPORT_STATUS_OK;
            }
            break;

        default:
            break;
    }

    return HIL_TRANSPORT_MVP_Control_Output_Record_Invariant_Failure( root );
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded(
    HIL_Transport_Mvp_Root_T* root, const uint8_t* encoded_item, size_t encoded_item_size )
{
    HIL_Transport_Status_T status;

    if ( ( root == NULL ) || ( encoded_item == NULL ) || ( encoded_item_size == 0u )
         || ( encoded_item_size > HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Control_Output_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    if ( root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE )
    {
        /* Copy first so READY can never identify incomplete owned bytes. */
        memmove( root->control_output, encoded_item, encoded_item_size );
        root->control_output_size  = encoded_item_size;
        root->control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY;
        return HIL_TRANSPORT_STATUS_OK;
    }

    /* Repeated requests for the same ACK are harmless even while it is pinned. */
    if ( ( root->control_output_size == encoded_item_size )
         && ( memcmp( root->control_output, encoded_item, encoded_item_size ) == 0 ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    /* A different item cannot replace output already waiting or pinned. */
    return HIL_TRANSPORT_STATUS_NOT_READY;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Control_Output_Peek_Output( HIL_Transport_Mvp_Root_T* root,
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
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Control_Output_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    *output_size = root->control_output_size;
    if ( ( out_buffer == NULL ) || ( out_buffer_size < root->control_output_size ) )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    memcpy( out_buffer, root->control_output, root->control_output_size );
    root->control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED;
    /* Global output selection belongs exclusively to the output arbiter. */
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Control_Output_Commit_Output( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T status;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_MVP_Control_Output_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->control_output_state != HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    /* Control output needs no ACK wait: commit releases ownership immediately. */
    root->control_output_size  = 0u;
    root->control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE;
    /* Stale array bytes are inaccessible, so clearing them would be wasted work. */
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Control_Output_Reset( HIL_Transport_Mvp_Root_T* root )
{
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    /* Reset bypasses validation so it can repair corrupted lifecycle metadata. */
    root->control_output_size  = 0u;
    root->control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( HIL_Transport_Mvp_Root_T* root,
                                                     uint8_t*                  pending )
{
    HIL_Transport_Status_T status;

    if ( pending == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *pending = 0u;
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Control_Output_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    *pending = ( uint8_t )( root->control_output_state != HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    return HIL_TRANSPORT_STATUS_OK;
}
