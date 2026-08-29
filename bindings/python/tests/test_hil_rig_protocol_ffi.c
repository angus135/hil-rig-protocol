#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hil_rig_protocol_ffi.h"

#define TEST_CHECK( condition )                                                                    \
    do                                                                                             \
    {                                                                                              \
        if ( !( condition ) )                                                                      \
        {                                                                                          \
            ( void )fprintf( stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,              \
                             #condition );                                                         \
            return 1;                                                                              \
        }                                                                                          \
    } while ( 0 )

typedef int ( *Test_Function_T )( void );

typedef struct
{
    const char*     name;
    Test_Function_T function;
} Test_Case_T;

static HIL_Transport_Config_T Make_Host_Config( void )
{
    HIL_Transport_Config_T config = { 0 };

    HIL_PY_TRANSPORT_Default_Config( &config );
    config.session_seed = UINT64_C( 0x12345678 );
    return config;
}

static HIL_Transport_Config_T Make_Rig_Config( void )
{
    HIL_Transport_Config_T config = { 0 };

    HIL_PY_TRANSPORT_Default_Config( &config );
    return config;
}

static HIL_Python_Transport_T* Fake_Transport_Pointer( void* storage )
{
    return ( HIL_Python_Transport_T* )storage;
}

static int Test_Default_Config_Forwards_Core_Defaults( void )
{
    HIL_Transport_Config_T expected = { 0 };
    HIL_Transport_Config_T actual   = { 0 };

    HIL_TRANSPORT_Default_Config( &expected );
    HIL_PY_TRANSPORT_Default_Config( &actual );

    TEST_CHECK( actual.max_application_message_size == expected.max_application_message_size );
    TEST_CHECK( actual.max_encoded_frame_size == expected.max_encoded_frame_size );
    TEST_CHECK( actual.session_seed == expected.session_seed );
    TEST_CHECK( actual.initial_reliable_sequence == expected.initial_reliable_sequence );
    TEST_CHECK( actual.connection_timeout_ms == expected.connection_timeout_ms );
    TEST_CHECK( actual.retransmit_timeout_ms == expected.retransmit_timeout_ms );
    TEST_CHECK( actual.max_retries == expected.max_retries );

    HIL_PY_TRANSPORT_Default_Config( NULL );
    return 0;
}

static int Test_Create_Valid_Host_And_Rig( void )
{
    HIL_Transport_Config_T          host_config = Make_Host_Config();
    HIL_Transport_Config_T          rig_config  = Make_Rig_Config();
    HIL_Python_Transport_T*         host        = NULL;
    HIL_Python_Transport_T*         rig         = NULL;
    HIL_Transport_Status_T          core_status = HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    HIL_Transport_Status_Snapshot_T snapshot    = { 0 };

    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &host_config, &host, &core_status )
        == HIL_PY_ADAPTER_STATUS_OK );
    TEST_CHECK( host != NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( host, &snapshot ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( snapshot.role == HIL_TRANSPORT_ROLE_HOST );

    core_status = HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    TEST_CHECK( HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_RIG, &rig_config, &rig, &core_status )
                == HIL_PY_ADAPTER_STATUS_OK );
    TEST_CHECK( rig != NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( rig, &snapshot ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( snapshot.role == HIL_TRANSPORT_ROLE_RIG );

    HIL_PY_TRANSPORT_Destroy( host );
    HIL_PY_TRANSPORT_Destroy( rig );
    HIL_PY_TRANSPORT_Destroy( NULL );
    return 0;
}

static int Test_Create_Validates_Adapter_Outputs( void )
{
    HIL_Transport_Config_T  config      = Make_Host_Config();
    HIL_Transport_Status_T  core_status = HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    HIL_Python_Transport_T* transport   = Fake_Transport_Pointer( &config );

    TEST_CHECK( HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, NULL, &core_status )
                == HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    TEST_CHECK( HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, NULL )
                == HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( transport == NULL );
    return 0;
}

static int Test_Create_Preserves_Core_Configuration_Failures( void )
{
    HIL_Transport_Config_T  config      = Make_Host_Config();
    HIL_Transport_Status_T  core_status = HIL_TRANSPORT_STATUS_OK;
    HIL_Python_Transport_T* transport   = Fake_Transport_Pointer( &config );

    TEST_CHECK( HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, NULL, &transport, &core_status )
                == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    config              = Make_Host_Config();
    config.session_seed = HIL_TRANSPORT_SESSION_SEED_INVALID;
    transport           = Fake_Transport_Pointer( &config );
    core_status         = HIL_TRANSPORT_STATUS_OK;
    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, &core_status )
        == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    config              = Make_Host_Config();
    config.session_seed = HIL_TRANSPORT_SESSION_SEED_RESERVED;
    transport           = Fake_Transport_Pointer( &config );
    core_status         = HIL_TRANSPORT_STATUS_OK;
    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, &core_status )
        == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    config              = Make_Rig_Config();
    config.session_seed = UINT64_C( 1 );
    transport           = Fake_Transport_Pointer( &config );
    core_status         = HIL_TRANSPORT_STATUS_OK;
    TEST_CHECK( HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_RIG, &config, &transport, &core_status )
                == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    config                       = Make_Host_Config();
    config.connection_timeout_ms = 1u;
    transport                    = Fake_Transport_Pointer( &config );
    core_status                  = HIL_TRANSPORT_STATUS_OK;
    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, &core_status )
        == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );

    config                              = Make_Host_Config();
    config.max_application_message_size = 0u;
    transport                           = Fake_Transport_Pointer( &config );
    core_status                         = HIL_TRANSPORT_STATUS_OK;
    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, &core_status )
        == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    config                        = Make_Host_Config();
    config.max_encoded_frame_size = 1u;
    transport                     = Fake_Transport_Pointer( &config );
    core_status                   = HIL_TRANSPORT_STATUS_OK;
    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, &core_status )
        == HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR );
    TEST_CHECK( transport == NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );

    return 0;
}

static int Test_Null_Handle_Forwarding( void )
{
    uint8_t                         byte     = 0u;
    size_t                          size     = 0u;
    HIL_Transport_Event_T           event    = { 0 };
    HIL_Transport_Status_Snapshot_T snapshot = { 0 };
    const uint8_t* const            byte_ptr = &byte;

    TEST_CHECK( HIL_PY_TRANSPORT_Reset( NULL ) == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Notify_Link_State( NULL, HIL_TRANSPORT_LINK_STATE_CONNECTED, 0u )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Submit_Application_Data( NULL, byte_ptr, 1u )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Receive_Bytes( NULL, byte_ptr, 1u, &size )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Process( NULL, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Peek_Output( NULL, &byte, 1u, &size )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Commit_Output( NULL, 0u )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Read_Application_Data( NULL, &byte, 1u, &size )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Read_Event( NULL, &event )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( NULL, &snapshot )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    return 0;
}

static int Test_Representative_Forwarding_Preserves_Core_Behaviour( void )
{
    HIL_Transport_Config_T          config      = Make_Host_Config();
    HIL_Python_Transport_T*         transport   = NULL;
    HIL_Transport_Status_T          core_status = HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    HIL_Transport_Status_Snapshot_T snapshot    = { 0 };
    HIL_Transport_Event_T           event       = { 0 };
    uint8_t                         output[HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE] = { 0 };
    const uint8_t                   receive_bytes[] = { 0x01u, 0x02u, 0x03u };
    size_t                          output_size     = 0u;
    size_t                          bytes_consumed  = 99u;
    size_t                          message_size    = 99u;

    TEST_CHECK(
        HIL_PY_TRANSPORT_Create( HIL_TRANSPORT_ROLE_HOST, &config, &transport, &core_status )
        == HIL_PY_ADAPTER_STATUS_OK );
    TEST_CHECK( transport != NULL );
    TEST_CHECK( core_status == HIL_TRANSPORT_STATUS_OK );

    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( transport, &snapshot ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( snapshot.role == HIL_TRANSPORT_ROLE_HOST );
    TEST_CHECK( snapshot.link_state == HIL_TRANSPORT_LINK_STATE_DISCONNECTED );

    TEST_CHECK(
        HIL_PY_TRANSPORT_Notify_Link_State( transport, HIL_TRANSPORT_LINK_STATE_CONNECTED, 10u )
        == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( transport, &snapshot ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( snapshot.link_state == HIL_TRANSPORT_LINK_STATE_CONNECTED );
    TEST_CHECK( snapshot.session_state == HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    TEST_CHECK( HIL_PY_TRANSPORT_Read_Event( transport, &event ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( event.type == HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED );
    TEST_CHECK( HIL_PY_TRANSPORT_Read_Event( transport, &event )
                == HIL_TRANSPORT_STATUS_NOT_READY );

    TEST_CHECK(
        HIL_PY_TRANSPORT_Process( transport, 11u, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER )
        == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( transport, &snapshot ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( snapshot.operating_mode == HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
    TEST_CHECK( snapshot.operating_mode_valid == 1u );
    TEST_CHECK( snapshot.output_pending == 1u );

    TEST_CHECK( HIL_PY_TRANSPORT_Peek_Output( transport, NULL, 0u, &output_size )
                == HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    TEST_CHECK( output_size > 0u );
    TEST_CHECK( output_size <= sizeof( output ) );
    {
        const size_t queried_size = output_size;
        TEST_CHECK(
            HIL_PY_TRANSPORT_Peek_Output( transport, output, sizeof( output ), &output_size )
            == HIL_TRANSPORT_STATUS_OK );
        TEST_CHECK( output_size == queried_size );
    }
    TEST_CHECK( HIL_PY_TRANSPORT_Commit_Output( transport, 12u ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( HIL_PY_TRANSPORT_Commit_Output( transport, 13u )
                == HIL_TRANSPORT_STATUS_NOT_READY );

    TEST_CHECK( HIL_PY_TRANSPORT_Receive_Bytes( transport, receive_bytes, sizeof( receive_bytes ),
                                                &bytes_consumed )
                == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( bytes_consumed == sizeof( receive_bytes ) );

    TEST_CHECK( HIL_PY_TRANSPORT_Read_Application_Data( transport, NULL, 0u, &message_size )
                == HIL_TRANSPORT_STATUS_NOT_READY );
    TEST_CHECK( message_size == 0u );

    TEST_CHECK( HIL_PY_TRANSPORT_Submit_Application_Data( transport, NULL, 1u )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    bytes_consumed = 99u;
    TEST_CHECK( HIL_PY_TRANSPORT_Receive_Bytes( transport, NULL, 1u, &bytes_consumed )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( bytes_consumed == 0u );
    TEST_CHECK( HIL_PY_TRANSPORT_Process( transport, 14u, ( HIL_Transport_Operating_Mode_T )99 )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Peek_Output( transport, output, sizeof( output ), NULL )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Read_Application_Data( transport, output, sizeof( output ), NULL )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Read_Event( transport, NULL )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( transport, NULL )
                == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    TEST_CHECK(
        HIL_PY_TRANSPORT_Notify_Link_State( transport, ( HIL_Transport_Link_State_T )99, 15u )
        == HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    TEST_CHECK( HIL_PY_TRANSPORT_Reset( transport ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( HIL_PY_TRANSPORT_Get_Status( transport, &snapshot ) == HIL_TRANSPORT_STATUS_OK );
    TEST_CHECK( snapshot.last_failure == HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    TEST_CHECK( snapshot.session_state == HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    TEST_CHECK( snapshot.application_message_pending == 0u );
    TEST_CHECK( snapshot.event_pending == 0u );

    HIL_PY_TRANSPORT_Destroy( transport );
    return 0;
}

int main( void )
{
    static const Test_Case_T tests[] = {
        { "default config forwarding", Test_Default_Config_Forwards_Core_Defaults },
        { "valid host and rig creation", Test_Create_Valid_Host_And_Rig },
        { "adapter output validation", Test_Create_Validates_Adapter_Outputs },
        { "core creation failures", Test_Create_Preserves_Core_Configuration_Failures },
        { "null handle forwarding", Test_Null_Handle_Forwarding },
        { "representative forwarding", Test_Representative_Forwarding_Preserves_Core_Behaviour },
    };
    size_t index;

    for ( index = 0u; index < ( sizeof( tests ) / sizeof( tests[0] ) ); ++index )
    {
        if ( tests[index].function() != 0 )
        {
            ( void )fprintf( stderr, "FAILED: %s\n", tests[index].name );
            return 1;
        }
        ( void )printf( "PASSED: %s\n", tests[index].name );
    }
    return 0;
}
