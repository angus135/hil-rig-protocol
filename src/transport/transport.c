#include "hil_rig_protocol/transport/transport.h"

#include "internal/transport_profile.h"

void HIL_TRANSPORT_Default_Config( HIL_Transport_Config_T* config )
{
    HIL_TRANSPORT_PROFILE_Default_Config( config );
}

HIL_Transport_Status_T HIL_TRANSPORT_Required_Storage_Size( const HIL_Transport_Config_T* config,
                                                            size_t* required_size )
{
    return HIL_TRANSPORT_PROFILE_Required_Storage_Size( config, required_size );
}

HIL_Transport_Status_T HIL_TRANSPORT_Init( HIL_Transport_Context_T*       context,
                                           HIL_Transport_Role_T           role,
                                           const HIL_Transport_Config_T*  config,
                                           const HIL_Transport_Storage_T* storage )
{
    return HIL_TRANSPORT_PROFILE_Init( context, role, config, storage );
}

HIL_Transport_Status_T HIL_TRANSPORT_Reset( HIL_Transport_Context_T* context )
{
    return HIL_TRANSPORT_PROFILE_Reset( context );
}

HIL_Transport_Status_T HIL_TRANSPORT_Notify_Link_State( HIL_Transport_Context_T*   context,
                                                        HIL_Transport_Link_State_T link_state,
                                                        uint32_t                   now_ms )
{
    return HIL_TRANSPORT_PROFILE_Notify_Link_State( context, link_state, now_ms );
}

HIL_Transport_Status_T HIL_TRANSPORT_Submit_Application_Data( HIL_Transport_Context_T* context,
                                                              const uint8_t*           payload,
                                                              size_t                   payload_len )
{
    return HIL_TRANSPORT_PROFILE_Submit_Application_Data( context, payload, payload_len );
}

HIL_Transport_Status_T HIL_TRANSPORT_Receive_Bytes( HIL_Transport_Context_T* context,
                                                    const uint8_t* data, size_t data_len,
                                                    size_t* bytes_consumed )
{
    return HIL_TRANSPORT_PROFILE_Receive_Bytes( context, data, data_len, bytes_consumed );
}

HIL_Transport_Status_T HIL_TRANSPORT_Process( HIL_Transport_Context_T* context, uint32_t now_ms,
                                              HIL_Transport_Operating_Mode_T operating_mode )
{
    return HIL_TRANSPORT_PROFILE_Process( context, now_ms, operating_mode );
}

HIL_Transport_Status_T HIL_TRANSPORT_Peek_Output( HIL_Transport_Context_T* context,
                                                  uint8_t* out_buffer, size_t out_buffer_size,
                                                  size_t* output_size )
{
    return HIL_TRANSPORT_PROFILE_Peek_Output( context, out_buffer, out_buffer_size, output_size );
}

HIL_Transport_Status_T HIL_TRANSPORT_Commit_Output( HIL_Transport_Context_T* context,
                                                    uint32_t                 now_ms )
{
    return HIL_TRANSPORT_PROFILE_Commit_Output( context, now_ms );
}

HIL_Transport_Status_T HIL_TRANSPORT_Read_Application_Data( HIL_Transport_Context_T* context,
                                                            uint8_t*                 out_buffer,
                                                            size_t  out_buffer_size,
                                                            size_t* message_size )
{
    return HIL_TRANSPORT_PROFILE_Read_Application_Data( context, out_buffer, out_buffer_size,
                                                        message_size );
}

HIL_Transport_Status_T HIL_TRANSPORT_Read_Event( HIL_Transport_Context_T* context,
                                                 HIL_Transport_Event_T*   event )
{
    return HIL_TRANSPORT_PROFILE_Read_Event( context, event );
}

HIL_Transport_Status_T HIL_TRANSPORT_Get_Status( const HIL_Transport_Context_T*   context,
                                                 HIL_Transport_Status_Snapshot_T* status )
{
    return HIL_TRANSPORT_PROFILE_Get_Status( context, status );
}
