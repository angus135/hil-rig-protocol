/**
 ________________________________________________
 |                                              |
 |                Test ID {16}                  |
 |______________________________________________|
 |                     |                        |
 |   Message Type {1}  |  Message Sub-Type {1}  |
 |_____________________|________________________|
 |                                              |
 |         Payload Size (Bytes) {16}            |
 |______________________________________________|
 |                                              |
 |                  Payload                     |
 |______________________________________________|
*/

#include "hil_rig_protocol/application/application_message.h"
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

#define HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE


HIL_Application_Status_T HIL_APPLICATION_Byte_Span_encode( const HIL_Application_Byte_Span_T* data,
                                                           uint8_t* payload )
{
    /**
    Payload = 4 + X Bytes:
    ________________________________
    |               |               |
    |    size {4}   |    span {X}   |
    |_______________|_______________|
    */
    memcpy( payload, &( data->size ), sizeof( data->size ) );
    memcpy( &(payload[sizeof( data->size )]), &( data->data ), data->size );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    /**
    Payload = 5 Bytes:
    ________________________________
    |               |               |
    |  git hash {1} |   query {4}   |
    |_______________|_______________|
    */
    uint32_t payload_size = 2;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->request_firmware_git_hash ),
            sizeof( data->request_firmware_git_hash ) );
    memcpy( &( payload[sizeof( data->request_firmware_git_hash )] ), &( data->query ), sizeof( data->query ) );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    /**
    Payload = 10 Bytes (fixed) + X + Y
    _______________________________________________________
    |                         |                            |
    |   protocol major {2}    |    protocol minor {2}      |
    |_________________________|____________________________|
    |                         |                            |
    |    version major {2}    |     version minor {2}      |
    |_________________________|____________________________|
    |                         |                            |
    |    version patch {2}    |        git hash {X}        |
    |_________________________|____________________________|
    |                         |
    |   diagnostic data {Y}   |
    |_________________________|
    */
    uint32_t payload_size = 10 + data->firmware_git_hash.size + data->diagnostic_data.size;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->application_protocol_major ),
            sizeof( data->application_protocol_major ) );
    memcpy( &( payload[2] ), &( data->application_protocol_minor ),
            sizeof( data->application_protocol_minor ) );
    memcpy( &( payload[4] ), &( data->firmware_version_major ),
            sizeof( data->firmware_version_major ) );
    memcpy( &( payload[6] ), &( data->firmware_version_minor ),
            sizeof( data->firmware_version_minor ) );
    memcpy( &( payload[8] ), &( data->firmware_version_patch ),
            sizeof( data->firmware_version_patch ) );
    HIL_APPLICATION_Byte_Span_encode( &( data->firmware_git_hash ), &( payload[10] ) );
    HIL_APPLICATION_Byte_Span_encode( &( data->firmware_git_hash ),
                                      &( payload[10 + data->diagnostic_data.size] ) );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Channel_Id_encode( const HIL_Application_Channel_Id_T* data,
                                                           uint8_t* payload )
{
    /**
    Payload = 6 Bytes
    _______________________________________________________
    |                         |                            |
    |     peripheral {4}      |        channel {2}         |
    |_________________________|____________________________|
    */
    memcpy( payload, &( data->peripheral ), sizeof( data->peripheral ) );
    memcpy( &(payload[sizeof( data->peripheral )]), &( data->channel ), data->channel );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Peripheral_Config_encode( const HIL_Application_Peripheral_Config_T* data,
                                                           uint8_t* payload )
{
    /**
    Payload:
    _______________________________________________________
    |                         |                            |
    |         type {4}        |         value {Z}          |
    |_________________________|____________________________|
    Value is a Union of 4 different types:
    Digital :
    _______________________________________________________
    |                         |                            |
    |       channel {3}       |       output mV {4}        |
    |_________________________|____________________________|
    |                         |                            |
    |      input mV {4}       |   initial output high {1}  |
    |_________________________|____________________________|
    |                         |
    |     capture en {1}      |
    |_________________________|
    Analog :
    _______________________________________________________
    |                         |                            |
    |      channel {3}        |       minimum mV {4}       |
    |_________________________|____________________________|
    |                         |
    |      maximum mV {4}     |
    |_________________________|
    PWM :
    _______________________________________________________
    |                         |                            |
    |       channel {3}       |        period nS {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |initial duty cycle pm {2}|     capture enabled {1}    |
    |_________________________|____________________________|
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {3}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    memcpy( payload, &( data->type ), sizeof( data->type ) );
    switch (data->type) {
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            HIL_APPLICATION_Channel_Id_encode( &( data->value.digital.channel ), &( payload[sizeof( data->type )] ) );
            memcpy( payload, &( data->type ), sizeof( data->type ) );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:

        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:

        case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:

        case HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        default:
            return HIL_APPLICATION_STATUS_INTERNAL_ERROR
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    /**
    Payload = 16 Bytes + X + Y
    _______________________________________________________
    |                         |                            |
    |    tick duration {4}    |  expected tick count {4}   |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |      *peripherals {Y}      |
    |_________________________|____________________________|
    |                         |                            |
    |   peripheral count {4}  |     extension data {X}     |
    |_________________________|____________________________|

    *Peripgerals expanded:
    _______________________________________________________
    |                         |                            |
    |         type {1}        |         *value {Z}         |
    |_________________________|____________________________|
    

    */
    uint32_t payload_size = 10 + data->firmware_git_hash.size + data->diagnostic_data.size;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T                    test_id,
    const HIL_Application_Variable_Instruction_Data_T* data, uint32_t max_payload_size,
    uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Execution_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Global_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Result_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Result_Data_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Response_T* data,
    uint32_t max_payload_size, uint8_t* payload )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}