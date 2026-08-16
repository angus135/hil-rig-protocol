/**
 * @file transport_output_mvp.c
 * @brief Private public-facing output arbitration for the MVP profile.
 */
#include "transport_output_mvp.h"

#include "transport_control_output_mvp.h"
#include "transport_reliability_mvp.h"

#include <stddef.h>

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Output_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
}

static HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Validate_State(
    HIL_Transport_Mvp_Root_T* root, uint8_t* reliable_output_pending,
    uint8_t* control_output_pending, uint8_t* reliable_delivery_pending )
{
    HIL_Transport_Status_T status;
    int                    reliable_peeked;
    int                    control_peeked;

    *reliable_output_pending   = 0u;
    *control_output_pending    = 0u;
    *reliable_delivery_pending = 0u;

    status = HIL_TRANSPORT_MVP_Reliability_Get_Pending_Status( root, reliable_output_pending,
                                                               reliable_delivery_pending );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    status = HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( root, control_output_pending );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    reliable_peeked =
        ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED )
        || ( root->session.reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
    control_peeked = root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED;

    if ( ( root->recovery_reset_pending > 1u )
         || ( ( root->recovery_reset_pending == 0u )
              && ( root->recovery_reset_session_identifier != HIL_TRANSPORT_SESSION_SEED_INVALID ) )
         || ( ( root->recovery_reset_pending != 0u )
              && ( ( root->base.session_state != HIL_TRANSPORT_SESSION_STATE_RECOVERING )
                   || ( root->session.state != HIL_TRANSPORT_SESSION_STATE_RECOVERING )
                   || ( root->base.link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED )
                   || ( root->session.link_state != HIL_TRANSPORT_LINK_STATE_CONNECTED )
                   || ( root->recovery_reset_session_identifier
                        == HIL_TRANSPORT_SESSION_SEED_INVALID )
                   || ( root->recovery_reset_session_identifier
                        == HIL_TRANSPORT_SESSION_SEED_RESERVED )
                   || ( root->control_output_state == HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE ) ) )
         || ( root->output_selection < HIL_TRANSPORT_MVP_OUTPUT_NONE )
         || ( root->output_selection > HIL_TRANSPORT_MVP_OUTPUT_CONTROL )
         || ( reliable_peeked && control_peeked ) )
    {
        return HIL_TRANSPORT_MVP_Output_Record_Invariant_Failure( root );
    }

    switch ( root->output_selection )
    {
        case HIL_TRANSPORT_MVP_OUTPUT_NONE:
            if ( !reliable_peeked && !control_peeked )
            {
                return HIL_TRANSPORT_STATUS_OK;
            }
            break;

        case HIL_TRANSPORT_MVP_OUTPUT_RELIABLE:
            if ( reliable_peeked && !control_peeked )
            {
                return HIL_TRANSPORT_STATUS_OK;
            }
            break;

        case HIL_TRANSPORT_MVP_OUTPUT_CONTROL:
            if ( control_peeked && !reliable_peeked )
            {
                return HIL_TRANSPORT_STATUS_OK;
            }
            break;

        default:
            break;
    }

    return HIL_TRANSPORT_MVP_Output_Record_Invariant_Failure( root );
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Peek_Output( HIL_Transport_Mvp_Root_T* root,
                                                             uint8_t*                  out_buffer,
                                                             size_t  out_buffer_size,
                                                             size_t* output_size )
{
    HIL_Transport_Status_T               status;
    HIL_Transport_Mvp_Output_Selection_T selection;
    uint8_t                              reliable_output_pending;
    uint8_t                              control_output_pending;
    uint8_t                              reliable_delivery_pending;

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
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    status = HIL_TRANSPORT_MVP_Output_Validate_State(
        root, &reliable_output_pending, &control_output_pending, &reliable_delivery_pending );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    ( void )reliable_delivery_pending;

    selection = root->output_selection;
    if ( selection == HIL_TRANSPORT_MVP_OUTPUT_NONE )
    {
        if ( control_output_pending != 0u )
        {
            selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;
        }
        else if ( reliable_output_pending != 0u )
        {
            selection = HIL_TRANSPORT_MVP_OUTPUT_RELIABLE;
        }
        else
        {
            return HIL_TRANSPORT_STATUS_NOT_READY;
        }
    }

    if ( selection == HIL_TRANSPORT_MVP_OUTPUT_CONTROL )
    {
        status = HIL_TRANSPORT_MVP_Control_Output_Peek_Output( root, out_buffer, out_buffer_size,
                                                               output_size );
    }
    else
    {
        status = HIL_TRANSPORT_MVP_Reliability_Peek_Output( root, out_buffer, out_buffer_size,
                                                            output_size );
    }

    if ( status == HIL_TRANSPORT_STATUS_OK )
    {
        root->output_selection = selection;
    }
    return status;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Commit_Output( HIL_Transport_Mvp_Root_T* root,
                                                               uint32_t                  now_ms )
{
    HIL_Transport_Status_T status;
    uint8_t                reliable_output_pending;
    uint8_t                control_output_pending;
    uint8_t                reliable_delivery_pending;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( root->base.session_state == HIL_TRANSPORT_SESSION_STATE_FAULT )
         || ( root->session.state == HIL_TRANSPORT_SESSION_STATE_FAULT ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    status = HIL_TRANSPORT_MVP_Output_Validate_State(
        root, &reliable_output_pending, &control_output_pending, &reliable_delivery_pending );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    ( void )reliable_output_pending;
    ( void )control_output_pending;
    ( void )reliable_delivery_pending;

    if ( root->output_selection == HIL_TRANSPORT_MVP_OUTPUT_NONE )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( root->output_selection == HIL_TRANSPORT_MVP_OUTPUT_RELIABLE )
    {
        status = HIL_TRANSPORT_MVP_Reliability_Commit_Output( root, now_ms );
    }
    else
    {
        ( void )now_ms;
        status = HIL_TRANSPORT_MVP_Control_Output_Commit_Output( root );
    }

    if ( status == HIL_TRANSPORT_STATUS_OK )
    {
        if ( ( root->output_selection == HIL_TRANSPORT_MVP_OUTPUT_CONTROL )
             && ( root->recovery_reset_pending != 0u ) )
        {
            root->recovery_reset_pending            = 0u;
            root->recovery_reset_session_identifier = HIL_TRANSPORT_SESSION_SEED_INVALID;
        }
        root->output_selection = HIL_TRANSPORT_MVP_OUTPUT_NONE;
    }
    return status;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Reset( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T reliable_status;
    HIL_Transport_Status_T control_status;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    reliable_status        = HIL_TRANSPORT_MVP_Reliability_Reset( root );
    control_status         = HIL_TRANSPORT_MVP_Control_Output_Reset( root );
    root->output_selection = HIL_TRANSPORT_MVP_OUTPUT_NONE;

    if ( reliable_status != HIL_TRANSPORT_STATUS_OK )
    {
        return reliable_status;
    }
    return control_status;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Get_Pending_Status(
    HIL_Transport_Mvp_Root_T* root, uint8_t* output_pending, uint8_t* reliable_delivery_pending )
{
    HIL_Transport_Status_T status;
    uint8_t                reliable_output_pending;
    uint8_t                control_output_pending;

    if ( ( output_pending == NULL ) || ( reliable_delivery_pending == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *output_pending            = 0u;
    *reliable_delivery_pending = 0u;
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Output_Validate_State(
        root, &reliable_output_pending, &control_output_pending, reliable_delivery_pending );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        *reliable_delivery_pending = 0u;
        return status;
    }

    *output_pending =
        ( uint8_t )( ( reliable_output_pending != 0u ) || ( control_output_pending != 0u ) );
    return HIL_TRANSPORT_STATUS_OK;
}
