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
#include "hil_rig_protocol/application/application_size.h"
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
    memcpy( &( payload[sizeof( data->size )] ), &( data->data ), data->size );
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
    uint32_t payload_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->request_firmware_git_hash ),
            sizeof( data->request_firmware_git_hash ) );
    memcpy( &( payload[sizeof( data->request_firmware_git_hash )] ), &( data->query ),
            sizeof( data->query ) );
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
    uint32_t payload_size = HIL_APPLICATION_SYSTEM_INFO_RESPONSE_FIXED_ENCODE_SIZE
                            + data->firmware_git_hash.size + data->diagnostic_data.size;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->application_protocol_major ),
            sizeof( data->application_protocol_major ) );
    uint8_t running_total = sizeof( data->application_protocol_major );
    memcpy( &( payload[running_total] ), &( data->application_protocol_minor ),
            sizeof( data->application_protocol_minor ) );
    running_total += sizeof( data->application_protocol_minor );
    memcpy( &( payload[running_total] ), &( data->firmware_version_major ),
            sizeof( data->firmware_version_major ) );
    running_total += sizeof( data->firmware_version_major );
    memcpy( &( payload[running_total] ), &( data->firmware_version_minor ),
            sizeof( data->firmware_version_minor ) );
    running_total += sizeof( data->firmware_version_minor );
    memcpy( &( payload[running_total] ), &( data->firmware_version_patch ),
            sizeof( data->firmware_version_patch ) );
    running_total += sizeof( data->firmware_version_patch );
    HIL_APPLICATION_Byte_Span_encode( &( data->firmware_git_hash ), &( payload[running_total] ) );
    running_total += data->diagnostic_data.size;
    HIL_APPLICATION_Byte_Span_encode( &( data->firmware_git_hash ), &( payload[running_total] ) );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Channel_Id_encode( const HIL_Application_Channel_Id_T* data, uint8_t* payload )
{
    /**
    Payload = 6 Bytes
    _______________________________________________________
    |                         |                            |
    |     peripheral {4}      |        channel {2}         |
    |_________________________|____________________________|
    */
    memcpy( payload, &( data->peripheral ), sizeof( data->peripheral ) );
    memcpy( &( payload[sizeof( data->peripheral )] ), &( data->channel ), data->channel );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_encode( const HIL_Application_Peripheral_Config_T* data,
                                          uint8_t* payload, uint32_t* size )
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
    |       channel {6}       |       output mV {4}        |
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
    |      channel {6}        |       minimum mV {4}       |
    |_________________________|____________________________|
    |                         |
    |      maximum mV {4}     |
    |_________________________|
    PWM :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |        period nS {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |initial duty cycle pm {2}|     capture enabled {1}    |
    |_________________________|____________________________|
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    memcpy( payload, &( data->type ), sizeof( data->type ) );
    uint8_t running_total = sizeof( data->type );
    switch ( data->type )
    {
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            HIL_APPLICATION_Channel_Id_encode( &( data->value.digital.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.digital.output_millivolts ),
                    sizeof( data->value.digital.output_millivolts ) );
            running_total += sizeof( data->value.digital.output_millivolts );
            memcpy( &( payload[running_total] ),
                    &( data->value.digital.input_threshold_millivolts ),
                    sizeof( data->value.digital.input_threshold_millivolts ) );
            running_total += sizeof( data->value.digital.input_threshold_millivolts );
            memcpy( &( payload[running_total] ), &( data->value.digital.initial_output_high ),
                    sizeof( data->value.digital.initial_output_high ) );
            running_total += sizeof( data->value.digital.initial_output_high );
            memcpy( &( payload[running_total] ), &( data->value.digital.capture_enabled ),
                    sizeof( data->value.digital.capture_enabled ) );
            running_total += sizeof( data->value.digital.capture_enabled );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            HIL_APPLICATION_Channel_Id_encode( &( data->value.analog.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.analog.minimum_microvolts ),
                    sizeof( data->value.analog.minimum_microvolts ) );
            running_total += sizeof( data->value.analog.minimum_microvolts );
            memcpy( &( payload[running_total] ), &( data->value.analog.maximum_microvolts ),
                    sizeof( data->value.analog.maximum_microvolts ) );
            running_total += sizeof( data->value.analog.maximum_microvolts );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            HIL_APPLICATION_Channel_Id_encode( &( data->value.pwm.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.pwm.period_nanoseconds ),
                    sizeof( data->value.pwm.period_nanoseconds ) );
            running_total += sizeof( data->value.pwm.period_nanoseconds );
            memcpy( &( payload[running_total] ), &( data->value.pwm.initial_duty_cycle_permyriad ),
                    sizeof( data->value.pwm.initial_duty_cycle_permyriad ) );
            running_total += sizeof( data->value.pwm.initial_duty_cycle_permyriad );
            memcpy( &( payload[running_total] ), &( data->value.pwm.capture_enabled ),
                    sizeof( data->value.pwm.capture_enabled ) );
            running_total += sizeof( data->value.pwm.capture_enabled );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:
            HIL_APPLICATION_Channel_Id_encode( &( data->value.communication.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.communication.bit_rate ),
                    sizeof( data->value.communication.bit_rate ) );
            running_total += sizeof( data->value.communication.bit_rate );
            memcpy( &( payload[running_total] ), &( data->value.communication.flags ),
                    sizeof( data->value.communication.flags ) );
            running_total += sizeof( data->value.communication.flags );
            memcpy( &( payload[running_total] ), &( data->value.communication.capture_limit_bytes ),
                    sizeof( data->value.communication.capture_limit_bytes ) );
            running_total += sizeof( data->value.communication.capture_limit_bytes );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        default:
            return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = running_total;
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
    // TODO payload_size
    // uint32_t payload_size = 10 + data->firmware_git_hash.size + data->diagnostic_data.size;
    // if ( max_payload_size < payload_size )
    // {
    //     return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    // }
    memcpy( payload, &( data->tick_duration ), sizeof( data->tick_duration ) );
    uint8_t running_total = sizeof( data->tick_duration );
    memcpy( &( payload[running_total] ), &( data->expected_tick_count ),
            sizeof( data->expected_tick_count ) );
    running_total += sizeof( data->expected_tick_count );
    memcpy( &( payload[running_total] ), &( data->flags ), sizeof( data->flags ) );
    running_total += sizeof( data->flags );
    uint32_t var_size = 0;
    for ( uint32_t i = 0; i < data->peripheral_count; i++ )
    {
        HIL_APPLICATION_Peripheral_Config_encode( &( data->peripherals[i] ),
                                                  &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    memcpy( &( payload[running_total] ), &( data->peripheral_count ),
            sizeof( data->peripheral_count ) );
    running_total += sizeof( data->peripheral_count );
    memcpy( &( payload[running_total] ), &( data->flags ), sizeof( data->flags ) );
    running_total += sizeof( data->flags );
    HIL_APPLICATION_Byte_Span_encode( &( data->extension_data ), &( payload[running_total] ) );
    running_total += data->extension_data.size;
    return HIL_APPLICATION_STATUS_OK;
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