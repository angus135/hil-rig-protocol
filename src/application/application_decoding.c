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
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"

#include <string.h>

void HIL_APPLICATION_Decode_U16_le( uint16_t* dest, const uint8_t* src, size_t* running_total )
{
    *dest          = ( ( uint16_t )src[0] ) | ( ( uint16_t )src[1] << 8 );
    *running_total = *running_total + 2;
}

void HIL_APPLICATION_Decode_U32_le( uint32_t* dest, const uint8_t* src, size_t* running_total )
{
    *dest = ( ( uint32_t )src[0] ) | ( ( uint32_t )src[1] << 8 ) | ( ( uint32_t )src[2] << 16 )
            | ( ( uint32_t )src[3] << 24 );
    *running_total = *running_total + 4;
}

void HIL_APPLICATION_Decode_U64_Le( uint64_t* dest, const uint8_t* src, size_t* running_total )
{
    *dest = ( ( uint64_t )src[0] ) | ( ( uint64_t )src[1] << 8 ) | ( ( uint64_t )src[2] << 16 )
            | ( ( uint64_t )src[3] << 24 ) | ( ( uint64_t )src[4] << 32 )
            | ( ( uint64_t )src[5] << 40 ) | ( ( uint64_t )src[6] << 48 )
            | ( ( uint64_t )src[7] << 56 );
    *running_total = *running_total + 8;
}

HIL_Application_Status_T HIL_APPLICATION_Byte_Span_decode( HIL_Application_Byte_Span_T* byte_span,
                                                           const uint8_t*               payload,
                                                           uint8_t* decode_data_dest )
{
    /**
    Payload = 1 + X Bytes:
    ________________________________
    |               |               |
    |    size {1}   |    span {X}   |
    |_______________|_______________|
    */
    size_t running_total = 0;
    memcpy( &( byte_span->size ), &( payload[running_total] ), sizeof( byte_span->size ) );
    running_total += sizeof( byte_span->size );
    // check size and copy the address of the data storage to the message struct
    if ( byte_span->size == 0 )
    {
        byte_span->data = NULL;
        return HIL_APPLICATION_STATUS_OK;
    }
    // copy the data over to the data storage destination
    memcpy( decode_data_dest, &( payload[running_total] ), byte_span->size );
    running_total += sizeof( byte_span->size );
    byte_span->data = decode_data_dest;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Request_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    const size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    /**
    Payload = 5 Bytes:
    ________________________________
    |               |               |
    |  git hash {1} |   query {4}   |
    |_______________|_______________|
    */
    uint32_t predicted_payload_size =
        sizeof( data->request_firmware_git_hash ) + sizeof( data->query );
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t running_total = 0;
    memcpy( &( data->request_firmware_git_hash ), &( payload[running_total] ),
            sizeof( data->request_firmware_git_hash ) );
    running_total += sizeof( data->request_firmware_git_hash );
    memcpy( &( data->query ), &( payload[running_total] ), sizeof( data->query ) );
    running_total += sizeof( data->query );
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    // no additional data needed
    *used_decoded_size = 0;
    *payload_size      = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    Payload = 10 Bytes (fixed) + X + Y
    _______________________________________________________
    |                         |                            |
    |   protocol major {2}    |    protocol minor {2}      |
    |_________________________|____________________________|
    |                         |                            |
    |    protcol patch {2}    |    version major {2}       |
    |_________________________|____________________________|
    |                         |                            |
    |    version minor {2}    |     version patch {2}      |
    |_________________________|____________________________|
    |                         |                            |
    |   diagnostic data {X}   |        git hash {Y}        |
    |_________________________|____________________________|
    */
    size_t   running_total = 0;
    uint8_t  decoded_total = 0;
    uint32_t predicted_payload_size =
        sizeof( data->application_protocol_major ) + sizeof( data->application_protocol_minor )
        + sizeof( data->application_protocol_patch ) + sizeof( data->firmware_version_major )
        + sizeof( data->firmware_version_minor ) + sizeof( data->firmware_version_patch ) + sizeof( data->diagnostic_data.size ) + sizeof( data->firmware_git_hash.size ) ;
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Decode_U16_le( &( data->application_protocol_major ),
                                   &( payload[running_total] ), &running_total );
    HIL_APPLICATION_Decode_U16_le( &( data->application_protocol_minor ),
                                   &( payload[running_total] ), &running_total );
    HIL_APPLICATION_Decode_U16_le( &( data->application_protocol_patch ),
                                   &( payload[running_total] ), &running_total );
    HIL_APPLICATION_Decode_U16_le( &( data->firmware_version_major ), &( payload[running_total] ),
                                   &running_total );
    HIL_APPLICATION_Decode_U16_le( &( data->firmware_version_minor ), &( payload[running_total] ),
                                   &running_total );
    HIL_APPLICATION_Decode_U16_le( &( data->firmware_version_patch ), &( payload[running_total] ),
                                   &running_total );

    // Variable data
    memcpy( &( data->diagnostic_data.size ), &( payload[running_total] ),
            sizeof( data->diagnostic_data.size ) );
    if ( data->diagnostic_data.size > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Byte_Span_decode( &( data->diagnostic_data ), &( payload[running_total] ),
                                      &( decoded_data[decoded_total] ) );
    running_total += sizeof( data->diagnostic_data.size );
    running_total += data->diagnostic_data.size;
    decoded_total += data->diagnostic_data.size;
    memcpy( &( data->firmware_git_hash.size ), &( payload[running_total] ),
            sizeof( data->firmware_git_hash.size ) );
    if ( data->diagnostic_data.size + data->firmware_git_hash.size > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Byte_Span_decode( &( data->firmware_git_hash ), &( payload[running_total] ),
                                      &( decoded_data[decoded_total] ) );
    running_total += sizeof( data->firmware_git_hash.size );
    running_total += data->firmware_git_hash.size;
    decoded_total += data->firmware_git_hash.size;
    if ( running_total-decoded_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_decoded_size = decoded_total;
    *payload_size      = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Channel_Id_decode( HIL_Application_Channel_Id_T* data,
                                                            const uint8_t*                payload )
{
    /**
    Payload = 6 Bytes
    _______________________________________________________
    |                         |                            |
    |     peripheral {4}      |        channel {2}         |
    |_________________________|____________________________|
    */
    size_t running_total = 0;
    memcpy( &( data->peripheral ), &( payload[running_total] ), sizeof( data->peripheral ) );
    running_total += sizeof( data->peripheral );
    HIL_APPLICATION_Decode_U16_le( &( data->channel ), &( payload[running_total] ),
                                   &running_total );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_decode( HIL_Application_Peripheral_Config_T* data,
                                          const uint8_t* payload, size_t* size )
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
    |       channel {6}       |     voltage_level {4}      |
    |_________________________|____________________________|

    Analog :
    _______________________________________________________
    |                         |                            |
    |      channel {6}        |     voltage level {4}      |
    |_________________________|____________________________|

    PWM :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |        period nS {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |initial duty cycle pm {2}|      voltage_level {4}     |
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
    size_t running_total = 0;
    memcpy( &( data->type ), &( payload[running_total] ), sizeof( data->type ) );
    running_total += sizeof( data->type );
    switch ( data->type )
    {
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            HIL_APPLICATION_Channel_Id_decode( &( data->value.digital.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( data->value.digital.voltage_level ), &( payload[running_total] ),
                    sizeof( data->value.digital.voltage_level ) );
            running_total += sizeof( data->value.digital.voltage_level );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            HIL_APPLICATION_Channel_Id_decode( &( data->value.analog.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( data->value.analog.voltage_level ), &( payload[running_total] ),
                    sizeof( data->value.analog.voltage_level ) );
            running_total += sizeof( data->value.analog.voltage_level );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            HIL_APPLICATION_Channel_Id_decode( &( data->value.pwm.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            HIL_APPLICATION_Decode_U32_le( &( data->value.pwm.period_nanoseconds ),
                                           &( payload[running_total] ), &running_total );
            HIL_APPLICATION_Decode_U16_le( &( data->value.pwm.initial_duty_cycle_permyriad ),
                                           payload, &running_total );
            memcpy( &( data->value.pwm.voltage_level ), &( payload[running_total] ),
                    sizeof( data->value.pwm.voltage_level ) );
            running_total += sizeof( data->value.pwm.voltage_level );
            break;
        // case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:
        //     HIL_APPLICATION_Channel_Id_decode( &( data->value.communication.channel ),
        //                                        &( payload[running_total] ) );
        //     running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
        //     memcpy( &( data->value.communication.bit_rate ), &( payload[running_total] ),
        //             sizeof( data->value.communication.bit_rate ) );
        //     running_total += sizeof( data->value.communication.bit_rate );
        //     memcpy( &( data->value.communication.flags ), &( payload[running_total] ),
        //             sizeof( data->value.communication.flags ) );
        //     running_total += sizeof( data->value.communication.flags );
        //     memcpy( &( data->value.communication.capture_limit_bytes ), &( payload[running_total]
        //     ),
        //             sizeof( data->value.communication.capture_limit_bytes ) );
        //     running_total += sizeof( data->value.communication.capture_limit_bytes );
        //     break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        default:
            return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Digital_Config_decode( HIL_Application_Digital_Config_T* data,
                                       const uint8_t* payload, size_t* size )
{
    /**
    Digital :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |     voltage_level {4}      |
    |_________________________|____________________________|

    */
    size_t running_total = 0;
    HIL_APPLICATION_Channel_Id_decode( &( data->channel ), &( payload[running_total] ) );
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    memcpy( &( data->voltage_level ), &( payload[running_total] ), sizeof( data->voltage_level ) );
    running_total += sizeof( data->voltage_level );
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Analog_Config_decode( HIL_Application_Analog_Config_T* data, const uint8_t* payload,
                                      size_t* size )
{
    /**
    Analog :
    _______________________________________________________
    |                         |                            |
    |      channel {6}        |     voltage level {4}      |
    |_________________________|____________________________|

    */
    size_t running_total = 0;
    HIL_APPLICATION_Channel_Id_decode( &( data->channel ), &( payload[running_total] ) );
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    memcpy( &( data->voltage_level ), &( payload[running_total] ), sizeof( data->voltage_level ) );
    running_total += sizeof( data->voltage_level );
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Pwm_Config_decode( HIL_Application_Pwm_Config_T* data,
                                                            const uint8_t* payload, size_t* size )
{
    /**

    PWM :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |        period nS {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |initial duty cycle pm {2}|      voltage_level {4}     |
    |_________________________|____________________________|

    */
    size_t running_total = 0;
    HIL_APPLICATION_Channel_Id_decode( &( data->channel ), &( payload[running_total] ) );
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    HIL_APPLICATION_Decode_U32_le( &( data->period_nanoseconds ), &( payload[running_total] ),
                                   &running_total );
    HIL_APPLICATION_Decode_U16_le( &( data->initial_duty_cycle_permyriad ),
                                   &( payload[running_total] ), &running_total );
    memcpy( &( data->voltage_level ), &( payload[running_total] ), sizeof( data->voltage_level ) );
    running_total += sizeof( data->voltage_level );
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Configuration_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size,
    HIL_Application_Peripheral_Config_T* decoded_peripherals, size_t max_decoded_peripherals_num,
    uint8_t* decoded_data, size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    Payload = 16 Bytes + X + Y
    _______________________________________________________
    |                         |                            |
    |    tick duration {4}    |  expected tick count {4}   |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   digital out [10] {10}    |
    |_________________________|____________________________|
    |                         |                            |
    |  digital in [10] {10}   |    analog out [6] {10}     |
    |_________________________|____________________________|
    |                         |                            |
    |    analog in [2] {10}   |       pwm out [2] {16}     |
    |_________________________|____________________________|
    |                         |                            |
    |    pwm in [2] {16}      |     extension data {X}     |
    |_________________________|____________________________|


    */
    size_t running_total = 0;
    size_t predicted_payload_size =
        sizeof( data->tick_duration_us.useconds ) + sizeof( data->expected_tick_count )
        + sizeof( data->flags )
        + ( sizeof( data->digital_in[0].channel.channel )
            + sizeof( data->digital_in[0].channel.peripheral )
            + sizeof( data->digital_in[0].voltage_level ) )
              * HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT
        + ( sizeof( data->digital_out[0].channel.channel )
            + sizeof( data->digital_out[0].channel.peripheral )
            + sizeof( data->digital_out[0].voltage_level ) )
              * HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT
        + ( sizeof( data->analog_in[0].channel.channel )
            + sizeof( data->analog_in[0].channel.peripheral )
            + sizeof( data->analog_in[0].voltage_level ) )
              * HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT
        + ( sizeof( data->analog_out[0].channel.channel )
            + sizeof( data->analog_out[0].channel.peripheral )
            + sizeof( data->analog_out[0].voltage_level ) )
              * HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT
        + ( sizeof( data->pwm_in[0].channel.channel ) + sizeof( data->pwm_in[0].channel.peripheral )
            + sizeof( data->pwm_in[0].voltage_level )
            + sizeof( data->pwm_in[0].initial_duty_cycle_permyriad )
            + sizeof( data->pwm_in[0].period_nanoseconds ) )
              * HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT
        + ( sizeof( data->pwm_out[0].channel.channel )
            + sizeof( data->pwm_out[0].channel.peripheral )
            + sizeof( data->pwm_out[0].voltage_level )
            + sizeof( data->pwm_out[0].initial_duty_cycle_permyriad )
            + sizeof( data->pwm_out[0].period_nanoseconds ) )
              * HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT
        + sizeof(data->extension_data.size);
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Decode_U32_le( &( data->tick_duration_us.useconds ),
                                   &( payload[running_total] ), &running_total );
    HIL_APPLICATION_Decode_U32_le( &( data->expected_tick_count ), &( payload[running_total] ),
                                   &running_total );
    HIL_APPLICATION_Decode_U32_le( &( data->flags ), &( payload[running_total] ), &running_total );
    size_t var_total = 0;
    for ( uint32_t i = 0; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Digital_Config_decode( &( data->digital_in[i] ),
                                               &( payload[running_total] ), &var_total );
        running_total += var_total;
        var_total = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Digital_Config_decode( &( data->digital_out[i] ),
                                               &( payload[running_total] ), &var_total );
        running_total += var_total;
        var_total = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Analog_Config_decode( &( data->analog_in[i] ), &( payload[running_total] ),
                                              &var_total );
        running_total += var_total;
        var_total = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Analog_Config_decode( &( data->analog_out[i] ), &( payload[running_total] ),
                                              &var_total );
        running_total += var_total;
        var_total = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Pwm_Config_decode( &( data->pwm_in[i] ), &( payload[running_total] ),
                                           &var_total );
        running_total += var_total;
        var_total = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Pwm_Config_decode( &( data->pwm_out[i] ), &( payload[running_total] ),
                                           &var_total );
        running_total += var_total;
        var_total = 0;
    }
    memcpy( &( data->extension_data.size ), &( payload[running_total] ),
            sizeof( data->extension_data.size ) );
    if ( data->extension_data.size > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Byte_Span_decode( &( data->extension_data ), &( payload[running_total] ),
                                      decoded_data );
    running_total += sizeof( data->extension_data.size );
    running_total += data->extension_data.size;
    if ( running_total- data->extension_data.size != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_decoded_size = data->extension_data.size;
    *payload_size      = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Instruction_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size,
    HIL_Application_Data_Declaration_T* decoded_variable_data, size_t max_decoded_variable_data_num,
    size_t* used_devoded_variable_num )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    _______________________________________________________
    |                         |                            |
    |     tick number {4}     |      Digital Out {X}       |
    |_________________________|____________________________|
    |                         |                            |
    |     analog out {Y}      |        PWM out {Z}         |
    |_________________________|____________________________|
    |                         |                            |
    | variable data count {4} |    *varaible data {Q}      |
    |_________________________|____________________________|

    *variable data expanded:
    _______________________________________________________
    |                         |                            |
    |     peripheral {4}      |        channel {2}         |
    |_________________________|____________________________|
    |                         |                            |
    |         size {4}        |          span {U}          |
    |_________________________|____________________________|
    */
    // tick number
    size_t   running_total          = 0;
    uint32_t predicted_payload_size = sizeof( data->tick_number );
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        predicted_payload_size += sizeof( data->digital_outputs[i].high );
    }
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        predicted_payload_size += sizeof( data->analog_outputs[i].microvolts );
    }
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        predicted_payload_size += sizeof( data->pwm_outputs[i].period_nanoseconds );
        predicted_payload_size += sizeof( data->pwm_outputs[i].duty_cycle_permyriad );
    }
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Decode_U32_le( &( data->tick_number ), &( payload[running_total] ),
                                   &running_total );
    //  digital out
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        memcpy( &( data->digital_outputs[i].high ), &( payload[running_total] ),
                sizeof( data->digital_outputs[i].high ) );
        running_total += sizeof( data->digital_outputs[i].high );
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Decode_U32_le( &( data->analog_outputs[i].microvolts ),
                                       &( payload[running_total] ), &running_total );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Decode_U32_le( &( data->pwm_outputs[i].period_nanoseconds ),
                                       &( payload[running_total] ), &running_total );
        HIL_APPLICATION_Decode_U16_le( &( data->pwm_outputs[i].duty_cycle_permyriad ),
                                       &( payload[running_total] ), &running_total );
    }
    // variable data
    // memcpy( &( data->variable_data_count ), &( payload[running_total] ),
    //         sizeof( data->variable_data_count ) );
    // running_total += sizeof( data->variable_data_count );
    // data->variable_data = decoded_variable_data;
    // if ( data->variable_data_count > max_decoded_variable_data_num )
    // {
    //     return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    // }
    // size_t decoded_running = 0;
    // for ( uint8_t i = 0; i < data->variable_data_count; i++ )
    // {
    //     memcpy( &( data->variable_data[i].channel.peripheral ), &( payload[running_total] ),
    //             sizeof( data->variable_data[i].channel.peripheral ) );
    //     running_total += sizeof( data->variable_data[i].channel.peripheral );
    //     memcpy( &( data->variable_data[i].channel.channel ), &( payload[running_total] ),
    //             sizeof( data->variable_data[i].channel.channel ) );
    //     running_total += sizeof( data->variable_data[i].channel.channel );
    //     memcpy( &( data->variable_data[i].data.size ), &( payload[running_total] ),
    //             sizeof( data->variable_data[i].data.size ) );
    //     if ( data->variable_data[i].data.size + decoded_running > max_decoded_data_size )
    //     {
    //         return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    //     }
    //     HIL_APPLICATION_Byte_Span_decode( &( data->variable_data[i].data ),
    //                                       &( payload[running_total] ),
    //                                       &( decoded_data[decoded_running] ) );
    //     running_total += sizeof( data->variable_data[i].data.size );
    //     running_total += data->variable_data[i].data.size;
    //     decoded_running += data->variable_data[i].data.size;
    //     *used_devoded_variable_num = i;
    // }
    // *used_decoded_size = decoded_running;
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_decoded_size = 0;
    *payload_size      = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Instruction_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    /**
    _______________________________________________________
    |                         |                            |
    |     tick number {4}     |       remainging {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |       channel {6}       |          data {X}          |
    |_________________________|____________________________|

    */

    // size check
    size_t running_total          = 0;
    size_t predicted_payload_size = sizeof( data->tick_number ) + sizeof( data->remaining )
                                    + sizeof( data->channel.channel )
                                    + sizeof( data->channel.peripheral ) + data->data.size;
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Decode_U32_le( &( data->tick_number ), &( payload[running_total] ),
                                   &running_total );
    HIL_APPLICATION_Decode_U32_le( &( data->remaining ), &( payload[running_total] ),
                                   &running_total );
    HIL_APPLICATION_Channel_Id_decode( &( data->channel ), &( payload[running_total] ) );
    running_total += sizeof( data->channel.channel ) + sizeof( data->channel.peripheral );

    data->data.data = decoded_data;
    memcpy( &( data->data.size ), &( payload[running_total] ), sizeof( data->data.size ) );
    if ( data->data.size > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Byte_Span_decode( &( data->data ), &( payload[running_total] ), decoded_data );
    running_total += sizeof( data->data.size );
    running_total += data->data.size;
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_decoded_size = data->data.size;
    *payload_size      = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Execution_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Execution_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    /**
    _______________________________________________________
    |                         |                            |
    |       command {4}       |         flags {4}          |
    |_________________________|____________________________|

    */
    size_t   running_total          = 0;
    uint32_t predicted_payload_size = sizeof( data->command ) + sizeof( data->flags );
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( &( data->command ), &( payload[running_total] ), sizeof( data->command ) );
    running_total += sizeof( data->command );
    HIL_APPLICATION_Decode_U32_le( &( data->flags ), &( payload[running_total] ), &running_total );
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_decoded_size = 0;
    *payload_size      = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Global_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Global_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    /**
    _______________________________________________________
    |                         |                            |
    |       command {4}       |         flags {4}          |
    |_________________________|____________________________|

    */
    size_t   running_total          = 0;
    uint32_t predicted_payload_size = sizeof( data->command ) + sizeof( data->flags );
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( &( data->command ), &( payload[running_total] ), sizeof( data->command ) );
    running_total += sizeof( data->command );
    HIL_APPLICATION_Decode_U32_le( &( data->flags ), &( payload[running_total] ), &running_total );
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = running_total;
    *used_decoded_size = 0;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size,
    HIL_Application_Data_Declaration_T* decoded_variable_data, size_t max_decoded_variable_data_num,
    size_t* used_decoded_variable_num )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    _______________________________________________________
    |                         |                            |
    |     tick number {4}     |       Digital In {X}       |
    |_________________________|____________________________|
    |                         |                            |
    |      analog in {Y}      |         PWM in {Z}         |
    |_________________________|____________________________|
    |                         |                            |
    | variable data count {4} |    *varaible data {Q}      |
    |_________________________|____________________________|
    |                         |                            |
    |      condition {4}      |     problem detail {4}     |
    |_________________________|____________________________|

    *variable data expanded:
    _______________________________________________________
    |                         |                            |
    |     peripheral {4}      |        channel {2}         |
    |_________________________|____________________________|
    |                         |                            |
    |         size {4}        |          span {U}          |
    |_________________________|____________________________|
    */
    // tick number
    size_t   running_total = 0;
    uint32_t predicted_payload_size =
        sizeof( data->tick_number ) + sizeof( data->condition ) + sizeof( data->problem_detail );
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        predicted_payload_size += sizeof( data->digital_inputs[i].high );
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        predicted_payload_size += sizeof( data->analog_inputs[i].microvolts );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        predicted_payload_size += sizeof( data->pwm_inputs[i].period_nanoseconds );
        predicted_payload_size += sizeof( data->pwm_inputs[i].duty_cycle_permyriad );
    }
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Decode_U32_le( &( data->tick_number ), &( payload[running_total] ),
                                   &running_total );
    //  digital out
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        memcpy( &( data->digital_inputs[i].high ), &( payload[running_total] ),
                sizeof( data->digital_inputs[i].high ) );
        running_total += sizeof( data->digital_inputs[i].high );
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Decode_U32_le( &( data->analog_inputs[i].microvolts ),
                                       &( payload[running_total] ), &running_total );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Decode_U32_le( &( data->pwm_inputs[i].period_nanoseconds ),
                                       &( payload[running_total] ), &running_total );
        HIL_APPLICATION_Decode_U16_le( &( data->pwm_inputs[i].duty_cycle_permyriad ),
                                       &( payload[running_total] ), &running_total );
    }
    // variable data
    size_t decoded_running = 0;
    // memcpy( &( data->variable_data_count ), &( payload[running_total] ),
    //         sizeof( data->variable_data_count ) );
    // running_total += sizeof( data->variable_data_count );
    // data->variable_data = decoded_variable_data;
    // if ( data->variable_data_count > max_decoded_variable_data_num )
    // {
    //     return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    // }
    // for ( uint8_t i = 0; i < data->variable_data_count; i++ )
    // {
    //     memcpy( &( data->variable_data[i].channel.peripheral ), &( payload[running_total] ),
    //             sizeof( data->variable_data[i].channel.peripheral ) );
    //     running_total += sizeof( data->variable_data[i].channel.peripheral );
    //     memcpy( &( data->variable_data[i].channel.channel ), &( payload[running_total] ),
    //             sizeof( data->variable_data[i].channel.channel ) );
    //     running_total += sizeof( data->variable_data[i].channel.channel );
    //     memcpy( &( data->variable_data[i].data.size ), &( payload[running_total] ),
    //             sizeof( data->variable_data[i].data.size ) );
    //     if ( data->variable_data[i].data.size + decoded_running > max_decoded_data_size )
    //     {
    //         return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    //     }
    //     HIL_APPLICATION_Byte_Span_decode( &( data->variable_data[i].data ),
    //                                       &( payload[running_total] ),
    //                                       &decoded_data[decoded_running] );
    //     running_total += sizeof( data->variable_data[i].data.size );
    //     running_total += data->variable_data[i].data.size;
    //     decoded_running += data->variable_data[i].data.size;
    // }
    // Condition and problem
    memcpy( &( data->condition ), &( payload[running_total] ), sizeof( data->condition ) );
    running_total += sizeof( data->condition );
    HIL_APPLICATION_Decode_U32_le( &( data->problem_detail ), &( payload[running_total] ),
                                   &running_total );
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = running_total;
    *used_decoded_size = decoded_running;
    // *used_decoded_variable_num = data->variable_data_count;
    *used_decoded_variable_num = 0;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Result_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )payload;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    ( void )used_decoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    /**
    _______________________________________________________
    |                         |                            |
    |        scope {4}        |        outcome {4}         |
    |_________________________|____________________________|
    |                         |                            |
    |        reason {4}       |      tick number {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |   comtrol command {4}   | global control command {4} |
    |_________________________|____________________________|
    |                         |
    |        detail {4}       |
    |_________________________|

    */
    size_t   running_total = 0;
    uint32_t predicted_payload_size =
        sizeof( data->scope ) + sizeof( data->outcome ) + sizeof( data->reason )
        + sizeof( data->tick_number ) + sizeof( data->control_command )
        + sizeof( data->global_control_command ) + sizeof( data->detail );
    if ( max_payload_size < predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( &( data->scope ), &( payload[running_total] ), sizeof( data->scope ) );
    running_total += sizeof( data->scope );
    memcpy( &( data->outcome ), &( payload[running_total] ), sizeof( data->outcome ) );
    running_total += sizeof( data->outcome );
    memcpy( &( data->reason ), &( payload[running_total] ), sizeof( data->reason ) );
    running_total += sizeof( data->reason );
    HIL_APPLICATION_Decode_U32_le( &( data->tick_number ), &( payload[running_total] ),
                                   &running_total );
    memcpy( &( data->control_command ), &( payload[running_total] ),
            sizeof( data->control_command ) );
    running_total += sizeof( data->control_command );
    memcpy( &( data->global_control_command ), &( payload[running_total] ),
            sizeof( data->global_control_command ) );
    running_total += sizeof( data->global_control_command );
    HIL_APPLICATION_Decode_U32_le( &( data->detail ), &( payload[running_total] ), &running_total );
    if ( running_total != predicted_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = running_total;
    *used_decoded_size = 0;
    return HIL_APPLICATION_STATUS_OK;
}