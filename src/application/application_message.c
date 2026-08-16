
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"

#include <string.h>

/**
 ________________________________________________
 |                                              |
 |                Test ID {16}                  |
 |______________________________________________|
 |                     |                        |
 |   Message Type {4}  |  Message Sub-Type {4}  |
 |_____________________|________________________|
 |                                              |
 |         Payload Size (Bytes) {16}            |
 |______________________________________________|
 |                                              |
 |                  Payload                     |
 |______________________________________________|
*/

HIL_Application_Status_T HIL_APPLICATION_Header_Encoding( const HIL_Application_Message_T* message,
                                                          const HIL_Application_Context_T* context,
                                                          uint8_t*                         dest )
{
    /**
     *
     *
     */
    // Test ID
    if ( message->has_test_id != 0 )
    {
        memcpy( dest, message->test_id, sizeof( message->test_id ) );
    }
    else
    {
        for ( uint8_t i = 0; i < sizeof( message->test_id ); i++ )
        {
            dest[i] = 0U;
        }
    }
    // Message Type
    memcpy( &( dest[sizeof( message->test_id )] ), context->type, sizeof( context->type ) );
    // Message sub-Type
    memcpy( &( dest[sizeof( message->test_id ) + sizeof( context->type )] ), context->subtype,
            sizeof( context->subtype ) );
    // We don't know the size of the payload yet so leave it blank but store the pointer
    return HIL_APPLICATION_STATUS_OK;
}