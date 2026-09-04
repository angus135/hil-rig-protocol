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
#include "hil_rig_protocol/application/application_encoding.h"

#include "hil_rig_protocol/version.h"

#include <string.h>

void HIL_APPLICATION_Encode_U16_Le( uint8_t* dst, const uint16_t value, size_t* running_total )
{
    dst[0]         = ( uint8_t )( value );
    dst[1]         = ( uint8_t )( value >> 8 );
    *running_total = *running_total + 2;
}

void HIL_APPLICATION_Encode_U32_Le( uint8_t* dst, const uint32_t value, size_t* running_total )
{
    dst[0]         = ( uint8_t )( value );
    dst[1]         = ( uint8_t )( value >> 8 );
    dst[2]         = ( uint8_t )( value >> 16 );
    dst[3]         = ( uint8_t )( value >> 24 );
    *running_total = *running_total + 4;
}

void HIL_APPLICATION_Encode_U64_Le( uint8_t* dst, const uint64_t value, size_t* running_total )
{
    dst[0]         = ( uint8_t )( value );
    dst[1]         = ( uint8_t )( value >> 8 );
    dst[2]         = ( uint8_t )( value >> 16 );
    dst[3]         = ( uint8_t )( value >> 24 );
    dst[4]         = ( uint8_t )( value >> 32 );
    dst[5]         = ( uint8_t )( value >> 40 );
    dst[6]         = ( uint8_t )( value >> 48 );
    dst[7]         = ( uint8_t )( value >> 56 );
    *running_total = *running_total + 8;
}

HIL_Application_Status_T HIL_APPLICATION_Byte_Span_encode( const HIL_Application_Byte_Span_T* data,
                                                           uint8_t* payload )
{
    /**
    Payload = 1 + X Bytes:
    ________________________________
    |               |               |
    |    size {1}   |    span {X}   |
    |_______________|_______________|
    */
    memcpy( payload, &( data->size ), sizeof( data->size ) );
    memcpy( &( payload[sizeof( data->size )] ), data->data, data->size );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    Payload = 5 Bytes:
    ________________________________
    |               |               |
    |  git hash {1} |   query {4}   |
    |_______________|_______________|
    */
    uint32_t payload_size = sizeof( data->request_firmware_git_hash ) + sizeof( data->query );
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->request_firmware_git_hash ),
            sizeof( data->request_firmware_git_hash ) );
    memcpy( &( payload[sizeof( data->request_firmware_git_hash )] ), &( data->query ),
            sizeof( data->query ) );
    *used_size = payload_size;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
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
    uint32_t payload_size =
        sizeof( data->application_protocol_major ) + sizeof( data->application_protocol_minor )
        + sizeof( data->application_protocol_patch ) + sizeof( data->firmware_version_major )
        + sizeof( data->firmware_version_minor ) + sizeof( data->firmware_version_patch )
        + data->firmware_git_hash.size + data->diagnostic_data.size;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    uint16_t protocol_patch = HIL_RIG_PROTOCOL_VERSION_PATCH;
    uint16_t protocol_major = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    uint16_t protocol_minor = HIL_RIG_PROTOCOL_VERSION_MINOR;
    HIL_APPLICATION_Encode_U16_Le( payload, protocol_major, &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), protocol_minor, &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), protocol_patch, &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->firmware_version_major,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->firmware_version_minor,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->firmware_version_patch,
                                   &running_total );
    HIL_APPLICATION_Byte_Span_encode( &( data->diagnostic_data ), &( payload[running_total] ) );
    running_total += sizeof( data->diagnostic_data.size );
    running_total += data->diagnostic_data.size;
    HIL_APPLICATION_Byte_Span_encode( &( data->firmware_git_hash ), &( payload[running_total] ) );
    running_total += sizeof( data->firmware_git_hash.size );
    running_total += data->firmware_git_hash.size;
    *used_size = running_total;
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
    size_t running_total = 0;
    memcpy( payload, &( data->peripheral ), sizeof( data->peripheral ) );
    running_total += sizeof( data->peripheral );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->channel, &running_total );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Digital_Config_encode( const HIL_Application_Digital_Config_T* data,
                                       size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /**
    Digital :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |     voltage_level {4}      |
    |_________________________|____________________________|

    */
    size_t running_total = 0;
    HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    memcpy( &( payload[running_total] ), &( data->voltage_level ), sizeof( data->voltage_level ) );
    running_total += sizeof( data->voltage_level );
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Analog_Config_encode( const HIL_Application_Analog_Config_T* data,
                                      size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /**
    Analog :
    _______________________________________________________
    |                         |                            |
    |      channel {6}        |     voltage level {4}      |
    |_________________________|____________________________|

    */
    size_t running_total = 0;
    HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    memcpy( &( payload[running_total] ), &( data->voltage_level ), sizeof( data->voltage_level ) );
    running_total += sizeof( data->voltage_level );
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Pwm_Config_encode( const HIL_Application_Pwm_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
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
    HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->period_nanoseconds,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->initial_duty_cycle_permyriad,
                                   &running_total );
    memcpy( &( payload[running_total] ), &( data->voltage_level ), sizeof( data->voltage_level ) );
    running_total += sizeof( data->voltage_level );
    *size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Can_Config_encode( const HIL_Application_Can_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /**
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Spi_Config_encode( const HIL_Application_Spi_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /**
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Uart_Config_encode( const HIL_Application_Uart_Config_T* data,
                                    size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /**
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_I2c_Config_encode( const HIL_Application_I2c_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /**
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_encode( const HIL_Application_Peripheral_Config_T* data,
                                          size_t max_payload_size, uint8_t* payload, size_t* size )
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
    uint32_t payload_size = sizeof( data->type );
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->type ), sizeof( data->type ) );
    size_t running_total = sizeof( data->type );
    switch ( data->type )
    {
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            payload_size += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE
                            + sizeof( data->value.digital.voltage_level );
            if ( max_payload_size < payload_size )
            {
                return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
            }
            HIL_APPLICATION_Channel_Id_encode( &( data->value.digital.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.digital.voltage_level ),
                    sizeof( data->value.digital.voltage_level ) );
            running_total += sizeof( data->value.digital.voltage_level );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            payload_size +=
                HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE + sizeof( data->value.analog.voltage_level );
            if ( max_payload_size < payload_size )
            {
                return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
            }
            HIL_APPLICATION_Channel_Id_encode( &( data->value.analog.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.analog.voltage_level ),
                    sizeof( data->value.analog.voltage_level ) );
            running_total += sizeof( data->value.analog.voltage_level );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            payload_size += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE
                            + sizeof( data->value.pwm.period_nanoseconds )
                            + sizeof( data->value.pwm.initial_duty_cycle_permyriad )
                            + sizeof( data->value.pwm.voltage_level );
            if ( max_payload_size < payload_size )
            {
                return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
            }
            HIL_APPLICATION_Channel_Id_encode( &( data->value.pwm.channel ),
                                               &( payload[running_total] ) );
            running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            memcpy( &( payload[running_total] ), &( data->value.pwm.period_nanoseconds ),
                    sizeof( data->value.pwm.period_nanoseconds ) );
            HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                           data->value.pwm.period_nanoseconds, &running_total );
            memcpy( &( payload[running_total] ), &( data->value.pwm.initial_duty_cycle_permyriad ),
                    sizeof( data->value.pwm.initial_duty_cycle_permyriad ) );
            HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ),
                                           data->value.pwm.initial_duty_cycle_permyriad,
                                           &running_total );
            memcpy( &( payload[running_total] ), &( data->value.pwm.voltage_level ),
                    sizeof( data->value.pwm.voltage_level ) );
            running_total += sizeof( data->value.pwm.voltage_level );
            break;
        // case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:
        // payload_size += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE
        //                 + sizeof( data->value.communication.bit_rate )
        //                 + sizeof( data->value.communication.flags )
        //                 + sizeof( data->value.communication.capture_limit_bytes );
        // if ( max_payload_size < payload_size )
        // {
        //     return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
        // }
        // HIL_APPLICATION_Channel_Id_encode( &( data->value.communication.channel ),
        //                                    &( payload[running_total] ) );
        // running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
        // memcpy( &( payload[running_total] ), &( data->value.communication.bit_rate ),
        //         sizeof( data->value.communication.bit_rate ) );
        // running_total += sizeof( data->value.communication.bit_rate );
        // memcpy( &( payload[running_total] ), &( data->value.communication.flags ),
        //         sizeof( data->value.communication.flags ) );
        // running_total += sizeof( data->value.communication.flags );
        // memcpy( &( payload[running_total] ), &( data->value.communication.capture_limit_bytes ),
        //         sizeof( data->value.communication.capture_limit_bytes ) );
        // running_total += sizeof( data->value.communication.capture_limit_bytes );
        // break;
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
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
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
    size_t payload_size =
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
              * HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t running_total = 0;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_duration_us.useconds,
                                   &running_total );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->expected_tick_count,
                                   &running_total );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->flags, &running_total );
    size_t var_size = 0;
    for ( uint32_t i = 0; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Digital_Config_encode( &( data->digital_in[i] ),
                                               ( max_payload_size - running_total ),
                                               &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Digital_Config_encode( &( data->digital_out[i] ),
                                               ( max_payload_size - running_total ),
                                               &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Analog_Config_encode( &( data->analog_in[i] ),
                                              ( max_payload_size - running_total ),
                                              &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Analog_Config_encode( &( data->analog_out[i] ),
                                              ( max_payload_size - running_total ),
                                              &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Pwm_Config_encode( &( data->pwm_in[i] ),
                                           ( max_payload_size - running_total ),
                                           &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Pwm_Config_encode( &( data->pwm_out[i] ),
                                           ( max_payload_size - running_total ),
                                           &( payload[running_total] ), &var_size );
        running_total += var_size;
        var_size = 0;
    }
    payload_size += data->extension_data.size + sizeof( data->extension_data.size );
    if ( max_payload_size < payload_size )
    {
        *used_size = running_total;
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Byte_Span_encode( &( data->extension_data ), &( payload[running_total] ) );
    running_total += sizeof( data->extension_data.size );
    running_total += data->extension_data.size;
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
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

    // variable data count validation
    // if ( data->variable_data_count > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK )
    // {
    //     return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    // }

    // size calculation
    // uint32_t payload_size = sizeof( data->tick_number ) + sizeof( data->variable_data_count );
    uint32_t payload_size = sizeof( data->tick_number );
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        payload_size += sizeof( data->digital_outputs[i].high );
    }
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        payload_size += sizeof( data->analog_outputs[i].microvolts );
    }
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        payload_size += sizeof( data->pwm_outputs[i].period_nanoseconds );
        payload_size += sizeof( data->pwm_outputs[i].duty_cycle_permyriad );
    }
    // for ( uint8_t i = 0; i < data->variable_data_count; i++ )
    // {
    //     payload_size += sizeof( data->variable_data->channel.peripheral );
    //     payload_size += sizeof( data->variable_data->channel.channel );
    //     payload_size += data->variable_data->data.size;
    // }
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    // Encoding
    // tick number
    size_t running_total = 0;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    //  digital out
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        memcpy( &( payload[running_total] ), &( data->digital_outputs[i].high ),
                sizeof( data->digital_outputs[i].high ) );
        running_total += sizeof( data->digital_outputs[i].high );
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                       data->analog_outputs[i].microvolts, &running_total );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                       data->pwm_outputs[i].period_nanoseconds, &running_total );
        HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ),
                                       data->pwm_outputs[i].duty_cycle_permyriad, &running_total );
    }
    // variable data
    // memcpy( &( payload[running_total] ), &( data->variable_data_count ),
    //         sizeof( data->variable_data_count ) );
    // running_total += sizeof( data->variable_data_count );
    // for ( uint8_t i = 0; i < data->variable_data_count; i++ )
    // {
    //     memcpy( &( payload[running_total] ), &( data->variable_data[i].channel.peripheral ),
    //             sizeof( data->variable_data[i].channel.peripheral ) );
    //     running_total += sizeof( data->variable_data[i].channel.peripheral );
    //     memcpy( &( payload[running_total] ), &( data->variable_data[i].channel.channel ),
    //             sizeof( data->variable_data[i].channel.channel ) );
    //     running_total += sizeof( data->variable_data[i].channel.channel );
    //     HIL_APPLICATION_Byte_Span_encode( &( data->variable_data[i].data ),
    //                                       &( payload[running_total] ) );
    //     running_total += sizeof( data->variable_data[i].data.size );
    //     running_total += data->variable_data[i].data.size;
    // }
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T                    test_id,
    const HIL_Application_Variable_Instruction_Data_T* data, size_t max_payload_size,
    uint8_t* payload, size_t* used_size )
{
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
    size_t payload_size = sizeof( data->tick_number ) + sizeof( data->remaining )
                          + sizeof( data->channel.channel ) + sizeof( data->channel.peripheral )
                          + data->data.size;
    if ( payload_size > max_payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t running_total = 0;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->remaining, &running_total );
    HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    running_total += sizeof( data->channel.channel ) + sizeof( data->channel.peripheral );
    HIL_APPLICATION_Byte_Span_encode( &( data->data ), &( payload[running_total] ) );
    running_total += sizeof( data->data.size );
    running_total += data->data.size;
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Execution_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    _______________________________________________________
    |                         |                            |
    |       command {4}       |         flags {4}          |
    |_________________________|____________________________|

    */
    uint32_t payload_size = sizeof( data->command ) + sizeof( data->flags );
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->command ), sizeof( data->command ) );
    size_t running_total = sizeof( data->command );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->flags, &running_total );
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Global_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    _______________________________________________________
    |                         |                            |
    |       command {4}       |         flags {4}          |
    |_________________________|____________________________|

    */
    uint32_t payload_size = sizeof( data->command ) + sizeof( data->flags );
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->command ), sizeof( data->command ) );
    size_t running_total = sizeof( data->command );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->flags, &running_total );
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Result_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
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
    // size calculation
    uint32_t payload_size =
        sizeof( data->tick_number ) + sizeof( data->condition ) + sizeof( data->problem_detail );
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        payload_size += sizeof( data->digital_inputs[i].high );
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        payload_size += sizeof( data->analog_inputs[i].microvolts );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        payload_size += sizeof( data->pwm_inputs[i].period_nanoseconds );
        payload_size += sizeof( data->pwm_inputs[i].duty_cycle_permyriad );
    }
    // variable data
    // payload_size += sizeof( data->variable_data_count );
    // for ( uint8_t i = 0; i < data->variable_data_count; i++ )
    // {
    //     payload_size += sizeof( data->variable_data->channel.peripheral );
    //     payload_size += sizeof( data->variable_data->channel.channel );
    //     payload_size += data->variable_data->data.size;
    // }
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    // encoding
    // tick number
    size_t running_total = 0;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    //  digital out
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        memcpy( &( payload[running_total] ), &( data->digital_inputs[i].high ),
                sizeof( data->digital_inputs[i].high ) );
        running_total += sizeof( data->digital_inputs[i].high );
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                       data->analog_inputs[i].microvolts, &running_total );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                       data->pwm_inputs[i].period_nanoseconds, &running_total );
        HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ),
                                       data->pwm_inputs[i].duty_cycle_permyriad, &running_total );
    }
    // variable data
    // memcpy( &( payload[running_total] ), &( data->variable_data_count ),
    //         sizeof( data->variable_data_count ) );
    // running_total += sizeof( data->variable_data_count );
    // for ( uint8_t i = 0; i < data->variable_data_count; i++ )
    // {
    //     memcpy( &( payload[running_total] ), &( data->variable_data[i].channel.peripheral ),
    //             sizeof( data->variable_data[i].channel.peripheral ) );
    //     running_total += sizeof( data->variable_data[i].channel.peripheral );
    //     memcpy( &( payload[running_total] ), &( data->variable_data[i].channel.channel ),
    //             sizeof( data->variable_data[i].channel.channel ) );
    //     running_total += sizeof( data->variable_data[i].channel.channel );
    //     HIL_APPLICATION_Byte_Span_encode( &( data->variable_data[i].data ),
    //                                       &( payload[running_total] ) );
    //     running_total += sizeof( data->variable_data[i].data.size );
    //     running_total += data->variable_data[i].data.size;
    // }
    // Condition and problem
    memcpy( &( payload[running_total] ), &( data->condition ), sizeof( data->condition ) );
    running_total += sizeof( data->condition );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->problem_detail,
                                   &running_total );
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Result_Data_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
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
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
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
    uint32_t payload_size = sizeof( data->scope ) + sizeof( data->outcome ) + sizeof( data->reason )
                            + sizeof( data->tick_number ) + sizeof( data->control_command )
                            + sizeof( data->global_control_command ) + sizeof( data->detail );
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( payload, &( data->scope ), sizeof( data->scope ) );
    size_t running_total = sizeof( data->scope );
    memcpy( &( payload[running_total] ), &( data->outcome ), sizeof( data->outcome ) );
    running_total += sizeof( data->outcome );
    memcpy( &( payload[running_total] ), &( data->reason ), sizeof( data->reason ) );
    running_total += sizeof( data->reason );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    memcpy( &( payload[running_total] ), &( data->control_command ),
            sizeof( data->control_command ) );
    running_total += sizeof( data->control_command );
    memcpy( &( payload[running_total] ), &( data->global_control_command ),
            sizeof( data->global_control_command ) );
    running_total += sizeof( data->global_control_command );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->detail, &running_total );
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}
