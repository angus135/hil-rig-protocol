
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
        memcpy( dest, &(message->test_id), sizeof( message->test_id ) );
    }
    else
    {
        for ( uint8_t i = 0; i < sizeof( message->test_id ); i++ )
        {
            dest[i] = 0U;
        }
    }
    // Message Type
    memcpy( &( dest[sizeof( message->test_id )] ), &(context->type), sizeof( context->type ) );
    // Message sub-Type
    memcpy( &( dest[sizeof( message->test_id ) + sizeof( context->type )] ), &(context->subtype),
            sizeof( context->subtype ) );
    // We don't know the size of the payload yet so leave it blank but store the pointer
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Header_decoding( const HIL_Application_Context_T* old_context,
                                                           HIL_Application_Context_T* new_contex, uint8_t* encoded_message )
{
    // Test ID

    memcpy( &(new_contex->test_id), encoded_message, sizeof( new_context->test_id ) );
    if ( new_context->has_test_id != 0 )
    {
        new_context->has_test_id = 1;
    }
    else
    {
        new_context->has_test_id = 0;
    }
    // Message Type
    memcpy( &( new_context->type ), encoded_message[sizeof( message->test_id )], sizeof( new_context->type ) );
    // Message sub-Type
    memcpy( &( new_context->sub_type ), encoded_message[sizeof( message->test_id ) + sizeof( context->type )],
            sizeof( new_context->subtype ) );
    // We don't know the size of the payload yet so leave it blank but store the pointer
    return HIL_APPLICATION_STATUS_OK;
}