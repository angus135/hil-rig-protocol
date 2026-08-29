/**
 * @file hil_rig_protocol_ffi.c
 * @brief Host-only ownership adapter for a future Python CFFI extension.
 */
#include "hil_rig_protocol_ffi.h"

#include <stdlib.h>
#include <string.h>

struct HIL_Python_Transport
{
    HIL_Transport_Context_T context;
    uint8_t*                workspace;
    size_t                  workspace_size;
};

void HIL_PY_TRANSPORT_Default_Config( HIL_Transport_Config_T* config )
{
    HIL_TRANSPORT_Default_Config( config );
}

HIL_Python_Adapter_Status_T HIL_PY_TRANSPORT_Create( HIL_Transport_Role_T          role,
                                                     const HIL_Transport_Config_T* config,
                                                     HIL_Python_Transport_T**      out_transport,
                                                     HIL_Transport_Status_T* out_transport_status )
{
    HIL_Python_Transport_T* transport     = NULL;
    HIL_Transport_Status_T  core_status   = HIL_TRANSPORT_STATUS_OK;
    HIL_Transport_Storage_T storage       = { 0 };
    size_t                  required_size = 0u;

    if ( out_transport != NULL )
    {
        *out_transport = NULL;
    }
    if ( out_transport_status != NULL )
    {
        *out_transport_status = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( out_transport == NULL ) || ( out_transport_status == NULL ) )
    {
        return HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT;
    }

    *out_transport_status = HIL_TRANSPORT_STATUS_OK;
    core_status           = HIL_TRANSPORT_Required_Storage_Size( config, &required_size );
    if ( core_status != HIL_TRANSPORT_STATUS_OK )
    {
        *out_transport_status = core_status;
        return HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR;
    }

    transport = ( HIL_Python_Transport_T* )calloc( 1u, sizeof( *transport ) );
    if ( transport == NULL )
    {
        return HIL_PY_ADAPTER_STATUS_ALLOCATION_FAILED;
    }

    transport->workspace = ( uint8_t* )calloc( required_size, sizeof( *transport->workspace ) );
    if ( transport->workspace == NULL )
    {
        memset( transport, 0, sizeof( *transport ) );
        free( transport );
        return HIL_PY_ADAPTER_STATUS_ALLOCATION_FAILED;
    }
    transport->workspace_size = required_size;

    storage.workspace      = transport->workspace;
    storage.workspace_size = transport->workspace_size;
    core_status            = HIL_TRANSPORT_Init( &transport->context, role, config, &storage );
    if ( core_status != HIL_TRANSPORT_STATUS_OK )
    {
        *out_transport_status = core_status;
        memset( transport->workspace, 0, transport->workspace_size );
        free( transport->workspace );
        memset( transport, 0, sizeof( *transport ) );
        free( transport );
        return HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR;
    }

    *out_transport = transport;
    return HIL_PY_ADAPTER_STATUS_OK;
}

void HIL_PY_TRANSPORT_Destroy( HIL_Python_Transport_T* transport )
{
    if ( transport == NULL )
    {
        return;
    }

    if ( transport->workspace != NULL )
    {
        memset( transport->workspace, 0, transport->workspace_size );
        free( transport->workspace );
    }
    memset( transport, 0, sizeof( *transport ) );
    free( transport );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Reset( HIL_Python_Transport_T* transport )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Reset( &transport->context );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Notify_Link_State( HIL_Python_Transport_T*    transport,
                                                           HIL_Transport_Link_State_T link_state,
                                                           uint32_t                   now_ms )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Notify_Link_State( &transport->context, link_state, now_ms );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Submit_Application_Data( HIL_Python_Transport_T* transport,
                                                                 const uint8_t*          payload,
                                                                 size_t payload_size )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Submit_Application_Data( &transport->context, payload, payload_size );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Receive_Bytes( HIL_Python_Transport_T* transport,
                                                       const uint8_t* data, size_t data_size,
                                                       size_t* bytes_consumed )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Receive_Bytes( &transport->context, data, data_size, bytes_consumed );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Process( HIL_Python_Transport_T* transport, uint32_t now_ms,
                                                 HIL_Transport_Operating_Mode_T operating_mode )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Process( &transport->context, now_ms, operating_mode );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Peek_Output( HIL_Python_Transport_T* transport,
                                                     uint8_t* output, size_t output_capacity,
                                                     size_t* output_size )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Peek_Output( &transport->context, output, output_capacity, output_size );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Commit_Output( HIL_Python_Transport_T* transport,
                                                       uint32_t                now_ms )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Commit_Output( &transport->context, now_ms );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Read_Application_Data( HIL_Python_Transport_T* transport,
                                                               uint8_t*                output,
                                                               size_t  output_capacity,
                                                               size_t* output_size )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Read_Application_Data( &transport->context, output, output_capacity,
                                                output_size );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Read_Event( HIL_Python_Transport_T* transport,
                                                    HIL_Transport_Event_T*  event )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Read_Event( &transport->context, event );
}

HIL_Transport_Status_T HIL_PY_TRANSPORT_Get_Status( const HIL_Python_Transport_T*    transport,
                                                    HIL_Transport_Status_Snapshot_T* status )
{
    if ( transport == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    return HIL_TRANSPORT_Get_Status( &transport->context, status );
}
