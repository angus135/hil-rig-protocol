/**
 * @file transport_events_mvp.c
 * @brief Private fixed-capacity high-level event lifecycle for the MVP profile.
 */
#include "transport_events_mvp.h"

#include <stddef.h>

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Events_Record_Invariant_Failure( HIL_Transport_Mvp_Root_T* root )
{
    root->base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root->session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root->session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
}

static int HIL_TRANSPORT_MVP_Events_Value_Is_Valid( const HIL_Transport_Event_T* event )
{
    return ( event->type > HIL_TRANSPORT_EVENT_NONE )
           && ( event->type <= HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED )
           && ( event->status >= HIL_TRANSPORT_STATUS_OK )
           && ( event->status <= HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
           && ( event->failure >= HIL_TRANSPORT_FAILURE_NONE )
           && ( event->failure <= HIL_TRANSPORT_FAILURE_INTERNAL );
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Events_Validate_State( HIL_Transport_Mvp_Root_T* root )
{
    size_t offset;

    if ( ( root->event_read_index >= HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY )
         || ( root->event_count > HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY ) )
    {
        return HIL_TRANSPORT_MVP_Events_Record_Invariant_Failure( root );
    }

    for ( offset = 0u; offset < root->event_count; ++offset )
    {
        const size_t index =
            ( root->event_read_index + offset ) % HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY;
        if ( !HIL_TRANSPORT_MVP_Events_Value_Is_Valid( &root->event_queue[index] ) )
        {
            return HIL_TRANSPORT_MVP_Events_Record_Invariant_Failure( root );
        }
    }
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Publish( HIL_Transport_Mvp_Root_T*    root,
                                                         const HIL_Transport_Event_T* event )
{
    HIL_Transport_Status_T status;
    size_t                 write_index;

    if ( ( root == NULL ) || ( event == NULL )
         || !HIL_TRANSPORT_MVP_Events_Value_Is_Valid( event ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_MVP_Events_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->event_count == HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY )
    {
        return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED;
    }

    write_index =
        ( root->event_read_index + root->event_count ) % HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY;
    root->event_queue[write_index] = *event;
    root->event_count++;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Events_Check_Capacity( HIL_Transport_Mvp_Root_T* root )
{
    HIL_Transport_Status_T status;

    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_MVP_Events_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    return root->event_count == HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY
               ? HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED
               : HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Read( HIL_Transport_Mvp_Root_T* root,
                                                      HIL_Transport_Event_T*    event )
{
    HIL_Transport_Status_T status;

    if ( ( root == NULL ) || ( event == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_MVP_Events_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( root->event_count == 0u )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    *event = root->event_queue[root->event_read_index];
    root->event_count--;
    if ( root->event_count == 0u )
    {
        root->event_read_index = 0u;
    }
    else
    {
        root->event_read_index =
            ( root->event_read_index + 1u ) % HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY;
    }
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Reset( HIL_Transport_Mvp_Root_T* root )
{
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    root->event_read_index = 0u;
    root->event_count      = 0u;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Get_Pending_Status( HIL_Transport_Mvp_Root_T* root,
                                                                    uint8_t* event_pending )
{
    HIL_Transport_Status_T status;

    if ( event_pending == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *event_pending = 0u;
    if ( root == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_MVP_Events_Validate_State( root );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    *event_pending = ( uint8_t )( root->event_count != 0u );
    return HIL_TRANSPORT_STATUS_OK;
}
