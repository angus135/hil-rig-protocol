
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"
#include "hil_rig_protocol/application/application_message.h"

#include <string.h>

/**
 ________________________________________________
 |                     |                        |
 |   Has Test ID {1}   |      Test ID {16}      |
 |_____________________|________________________|
 |                     |                        |
 |   Message Type {4}  |  Message Sub-Type {4}  |
 |_____________________|________________________|
 |                                              |
 |          Payload Size (Bytes) {4}            |
 |______________________________________________|
 |                                              |
 |                  Payload                     |
 |______________________________________________|
 |                                              |
 |            End Payload Flag {1}              |
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
    size_t running_total = 0;
    memcpy( dest, &( message->has_test_id ), sizeof( message->has_test_id ) );
    running_total += sizeof( message->has_test_id );
    if ( message->has_test_id != 0 )
    {
        memcpy( &( dest[running_total] ), &( message->test_id ), sizeof( message->test_id ) );
    }
    else
    {
        for ( size_t i = running_total; i < running_total + sizeof( message->test_id ); i++ )
        {
            dest[i] = 0U;
        }
    }
    running_total += sizeof( message->test_id );
    // Message Type
    memcpy( &( dest[running_total] ), &( message->type ), sizeof( message->type ) );
    running_total += sizeof( message->type );
    // Message sub-Type
    memcpy( &( dest[running_total] ), &( message->subtype ), sizeof( message->subtype ) );
    running_total += sizeof( message->subtype );
    // We don't know the size of the payload yet so leave it blank but store the pointer
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Header_decoding( const HIL_Application_Context_T* old_context,
                                 HIL_Application_Message_T* new_message, const uint8_t* encoded_message,
                                 size_t* payload_size )
{
    // Test ID

    size_t running_total = 0;
    memcpy( &( new_message->has_test_id ), encoded_message, sizeof( new_message->has_test_id ) );
    running_total += sizeof( new_message->has_test_id );
    memcpy( &( new_message->test_id ), &( encoded_message[running_total] ),
            sizeof( new_message->test_id ) );
    running_total += sizeof( new_message->test_id );
    // Message Type
    memcpy( &( new_message->type ), &( encoded_message[running_total] ),
            sizeof( new_message->type ) );
    running_total += sizeof( new_message->type );
    // Message sub-Type
    memcpy( &( new_message->subtype ), &( encoded_message[running_total] ),
            sizeof( new_message->subtype ) );
    running_total += sizeof( new_message->subtype );
    // the expected payload size
    memcpy( payload_size, &( encoded_message[running_total] ),
            HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
    running_total += HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES;
    return HIL_APPLICATION_STATUS_OK;
}