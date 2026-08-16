#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport.h"

#if defined( _MSC_VER )
_Static_assert( HIL_TRANSPORT_WORKSPACE_ALIGNMENT >= __alignof( long double ),
                "Transport workspace alignment must satisfy MSVC C scalar alignment" );
#else
_Static_assert( HIL_TRANSPORT_WORKSPACE_ALIGNMENT >= _Alignof( max_align_t ),
                "Transport workspace alignment must satisfy max_align_t" );
#endif
_Static_assert( ( HIL_TRANSPORT_WORKSPACE_ALIGNMENT & ( HIL_TRANSPORT_WORKSPACE_ALIGNMENT - 1u ) )
                    == 0u,
                "Transport workspace alignment must be a power of two" );

int main( void )
{
    HIL_Transport_Context_T                               context                 = { 0 };
    HIL_Transport_Config_T                                config                  = { 0 };
    _Alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) uint8_t workspace[1024]         = { 0 };
    uint8_t                                               application_message[32] = { 0 };
    uint8_t                                               encoded_output[128]     = { 0 };

    HIL_Transport_Storage_T storage = {
        .workspace      = workspace,
        .workspace_size = sizeof( workspace ),
    };
    HIL_Transport_Event_T           event              = { 0 };
    HIL_Transport_Status_Snapshot_T snapshot           = { 0 };
    size_t                          required_workspace = 1u;
    size_t                          bytes_consumed     = 1u;
    size_t                          output_size        = 1u;
    size_t                          message_size       = 1u;

    HIL_TRANSPORT_Default_Config( &config );
    config.max_application_message_size = sizeof( application_message );
    config.max_encoded_frame_size       = sizeof( encoded_output );
    config.session_seed                 = UINT64_C( 0x1234 );
    config.initial_reliable_sequence    = 17u;
    config.connection_timeout_ms        = 0u;
    config.retransmit_timeout_ms        = 0u;
    config.max_retries                  = 0u;

    if ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_workspace )
             != HIL_TRANSPORT_STATUS_OK
         || required_workspace == 0u || required_workspace > sizeof( workspace ) )
    {
        return 1;
    }

    if ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage )
         != HIL_TRANSPORT_STATUS_OK )
    {
        return 2;
    }

    if ( HIL_TRANSPORT_Notify_Link_State( &context, HIL_TRANSPORT_LINK_STATE_CONNECTED, 10u )
         != HIL_TRANSPORT_STATUS_OK )
    {
        return 3;
    }
    ( void )HIL_TRANSPORT_Process( &context, 11u, HIL_TRANSPORT_OPERATING_MODE_NORMAL );
    ( void )HIL_TRANSPORT_Process( &context, 12u, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
    ( void )HIL_TRANSPORT_Process( &context, 13u, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );
    ( void )HIL_TRANSPORT_Get_Status( &context, &snapshot );
    if ( snapshot.session_state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
    {
        ( void )HIL_TRANSPORT_Submit_Application_Data( &context, application_message,
                                                       sizeof( application_message ) );
    }

    if ( HIL_TRANSPORT_Receive_Bytes( &context, encoded_output, sizeof( encoded_output ),
                                      &bytes_consumed )
             != HIL_TRANSPORT_STATUS_OK
         || bytes_consumed != sizeof( encoded_output ) )
    {
        return 3;
    }

    if ( HIL_TRANSPORT_Peek_Output( &context, encoded_output, sizeof( encoded_output ),
                                    &output_size )
             != HIL_TRANSPORT_STATUS_OK
         || output_size == 0u )
    {
        return 4;
    }
    if ( HIL_TRANSPORT_Commit_Output( &context, 14u ) != HIL_TRANSPORT_STATUS_OK )
    {
        return 4;
    }

    if ( HIL_TRANSPORT_Read_Application_Data( &context, application_message,
                                              sizeof( application_message ), &message_size )
             != HIL_TRANSPORT_STATUS_NOT_READY
         || message_size != 0u )
    {
        return 5;
    }

    ( void )HIL_TRANSPORT_Read_Event( &context, &event );
    ( void )HIL_TRANSPORT_Reset( &context );
    return 0;
}
