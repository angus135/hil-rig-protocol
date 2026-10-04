/** @file application_codec.c
 * @brief Caller-owned C11 storage for the v0.4.0 message families.
 */
#include <stdio.h>
#include <string.h>

#include "hil_rig_protocol/application/application.h"

/** Verify one codec round trip; no endpoint operation is executed. */
static int round_trip( const HIL_Application_Context_T* context,
                       const HIL_Application_Message_T* message )
{
    uint8_t wire[HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE];
    uint8_t copy[HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE];
    _Alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ) uint8_t storage[8192];
    HIL_Application_Message_T                                    decoded      = { 0 };
    size_t                                                       encoded_size = 0;
    size_t                                                       storage_size = 0;
    size_t                                                       copy_size    = 0;
    if ( HIL_APPLICATION_Encode_Message( context, message, wire, sizeof( wire ), &encoded_size )
             != HIL_APPLICATION_STATUS_OK
         || HIL_APPLICATION_Decode_Message( context, wire, encoded_size, &decoded, storage,
                                            sizeof( storage ), &storage_size )
                != HIL_APPLICATION_STATUS_OK
         || HIL_APPLICATION_Encode_Message( context, &decoded, copy, sizeof( copy ), &copy_size )
                != HIL_APPLICATION_STATUS_OK
         || copy_size != encoded_size || memcmp( wire, copy, encoded_size ) != 0 )
    {
        return 1;
    }
    printf( "Application type %u: %zu bytes, round trip OK\n", message->type, encoded_size );
    return 0;
}

/** Construct illustrative endpoint values without retaining buffers in the codec. */
int main( void )
{
    HIL_Application_Config_T  config;
    HIL_Application_Context_T context;
    HIL_Application_Message_T message = { 0 };
    const uint8_t             reply[] = { 42, 0, 0, 0 };
    if ( HIL_APPLICATION_Default_Config( &config ) != HIL_APPLICATION_STATUS_OK
         || HIL_APPLICATION_Init( &context, &config ) != HIL_APPLICATION_STATUS_OK )
    {
        return 1;
    }

    /* IDs 1/2 are local ping/echo conventions, not protocol assignments. */
    message.type                              = HIL_APPLICATION_MESSAGE_TYPE_ARBITRARY_CONTROL;
    message.body.arbitrary_control.control_id = 1;
    message.body.arbitrary_control.value      = 42;
    if ( round_trip( &context, &message ) != 0 )
        return 1;

    memset( &message, 0, sizeof( message ) );
    message.type                             = HIL_APPLICATION_MESSAGE_TYPE_ARBITRARY_DATA;
    message.body.arbitrary_data.data_id      = 1;
    message.body.arbitrary_data.payload.data = reply;
    message.body.arbitrary_data.payload.size = sizeof( reply );
    if ( round_trip( &context, &message ) != 0 )
        return 1;

    memset( &message, 0, sizeof( message ) );
    message.type                           = HIL_APPLICATION_MESSAGE_TYPE_RIG_STATUS;
    message.body.rig_status.schema_version = 1;
    message.body.rig_status.origin         = HIL_APPLICATION_STATUS_ORIGIN_QUERY_RESPONSE;
    message.body.rig_status.state          = HIL_APPLICATION_RIG_STATE_IDLE;
    message.body.rig_status.flags =
        HIL_APPLICATION_RIG_STATUS_READY_FOR_NEW_TEST | HIL_APPLICATION_RIG_STATUS_RESET_PERMITTED;
    if ( round_trip( &context, &message ) != 0 )
        return 1;

    memset( &message, 0, sizeof( message ) );
    message.type                                 = HIL_APPLICATION_MESSAGE_TYPE_RUN_REPORT;
    message.has_test_id                          = 1;
    message.body.run_report.schema_version       = 1;
    message.body.run_report.run_outcome          = HIL_APPLICATION_RUN_OUTCOME_SUCCESS;
    message.body.run_report.execution_outcome    = HIL_APPLICATION_EXECUTION_OUTCOME_COMPLETE;
    message.body.run_report.result_status        = HIL_APPLICATION_RUN_RESULT_STATUS_COMPLETE;
    message.body.run_report.valid_sections       = HIL_APPLICATION_RUN_REPORT_VALID_TERMINAL;
    message.body.run_report.expected_tick_count  = 1;
    message.body.run_report.tick_period_us       = 1000;
    message.body.run_report.result_ticks_emitted = 1;
    return round_trip( &context, &message );
}
