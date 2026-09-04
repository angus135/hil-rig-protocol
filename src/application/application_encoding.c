/**
 * @file application_encoding.c
 * @brief Message-specific Application payload encoders.
 *
 * @details The common 23-byte envelope is encoded by application_message.c.
 * These functions encode payload bytes only and use explicit fixed-width,
 * little-endian wire fields. There is no payload-end marker.
 */

#include "hil_rig_protocol/application/application_message.h"
#include "application_size.h"
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"
#include "application_encoding.h"
#include "application_internal.h"

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
                                                           uint8_t* payload,
                                                           size_t   payload_capacity,
                                                           size_t*  used_size )
{
    size_t required_size = 0u;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( data->size != 0u && data->data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE,
                                            ( size_t )data->size, &required_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( payload_capacity < required_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    /* Publish the one-byte wire length only after the complete span is known to fit. */
    payload[0] = data->size;
    if ( data->size != 0u )
    {
        memcpy( &payload[HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE], data->data, data->size );
    }
    *used_size = required_size;
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
     * Payload = 2 bytes: request_firmware_git_hash {1}, query {1}.
     */
    const size_t payload_size = 2u;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( data->request_firmware_git_hash > 1u
         || data->query < HIL_APPLICATION_SYSTEM_INFO_QUERY_INVALID
         || data->query > HIL_APPLICATION_SYSTEM_INFO_QUERY_RESERVED )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    uint8_t wire_query = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->query, &wire_query ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[0] = data->request_firmware_git_hash;
    payload[1] = wire_query;
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
     * Payload = six little-endian uint16_t version fields {12}, followed by
     * diagnostic_data as a one-byte length plus X bytes, then firmware_git_hash
     * as a one-byte length plus Y bytes. Total = 14 + X + Y bytes.
     */
    size_t       running_total = 0u;
    const size_t payload_size =
        6u * HIL_APPLICATION_WIRE_U16_SIZE + 2u * HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE
        + ( size_t )data->firmware_git_hash.size + ( size_t )data->diagnostic_data.size;
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
    size_t                   span_size = 0u;
    HIL_Application_Status_T span_status =
        HIL_APPLICATION_Byte_Span_encode( &( data->diagnostic_data ), &( payload[running_total] ),
                                          max_payload_size - running_total, &span_size );
    if ( span_status != HIL_APPLICATION_STATUS_OK )
    {
        return span_status;
    }
    running_total += span_size;
    span_status =
        HIL_APPLICATION_Byte_Span_encode( &( data->firmware_git_hash ), &( payload[running_total] ),
                                          max_payload_size - running_total, &span_size );
    if ( span_status != HIL_APPLICATION_STATUS_OK )
    {
        return span_status;
    }
    running_total += span_size;
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Channel_Id_encode( const HIL_Application_Channel_Id_T* data, uint8_t* payload )
{
    /* Wire channel ID: peripheral u8, then channel uint16 little-endian (3 bytes). */
    size_t running_total = 0u;
    if ( data->peripheral < HIL_APPLICATION_PERIPHERAL_INVALID
         || data->peripheral > HIL_APPLICATION_PERIPHERAL_RESERVED )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    uint8_t wire_peripheral = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->peripheral, &wire_peripheral ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[running_total++] = wire_peripheral;
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->channel, &running_total );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Digital_Config_encode( const HIL_Application_Digital_Config_T* data,
                                       uint8_t* payload, size_t* size )
{
    /* Fixed Digital config: 3-byte channel ID followed by one-byte voltage enum. */
    size_t                   running_total = 0u;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    uint8_t wire_voltage_level = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->voltage_level, &wire_voltage_level ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[running_total++] = wire_voltage_level;
    *size                    = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Analog_Config_encode( const HIL_Application_Analog_Config_T* data, uint8_t* payload,
                                      size_t* size )
{
    /* Fixed Analogue config: 3-byte channel ID followed by one-byte voltage enum. */
    size_t                   running_total = 0u;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    uint8_t wire_voltage_level = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->voltage_level, &wire_voltage_level ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[running_total++] = wire_voltage_level;
    *size                    = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Pwm_Config_encode( const HIL_Application_Pwm_Config_T* data, uint8_t* payload,
                                   size_t* size )
{
    /**
     * Fixed PWM configuration body: channel ID {3}, period nanoseconds {4},
     * initial duty-cycle permyriad {2}, voltage-level enum {1}.
     */
    size_t                   running_total = 0u;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Channel_Id_encode( &( data->channel ), &( payload[running_total] ) );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->period_nanoseconds,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->initial_duty_cycle_permyriad,
                                   &running_total );
    uint8_t wire_voltage_level = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->voltage_level, &wire_voltage_level ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[running_total++] = wire_voltage_level;
    *size                    = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Can_Config_encode( const HIL_Application_Can_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /** Communication-family wire encoding is deliberately deferred. */
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    ( void )size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Spi_Config_encode( const HIL_Application_Spi_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /** Communication-family wire encoding is deliberately deferred. */
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    ( void )size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Uart_Config_encode( const HIL_Application_Uart_Config_T* data,
                                    size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /** Communication-family wire encoding is deliberately deferred. */
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    ( void )size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_I2c_Config_encode( const HIL_Application_I2c_Config_T* data,
                                   size_t max_payload_size, uint8_t* payload, size_t* size )
{
    /** Communication-family wire encoding is deliberately deferred. */
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    ( void )size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_encode( const HIL_Application_Peripheral_Config_T* data,
                                          size_t max_payload_size, uint8_t* payload, size_t* size )
{
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    if ( size != NULL )
    {
        *size = 0u;
    }
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
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
     * Current fixed Test Configuration payload:
     * - tick duration in microseconds {4}, expected tick count {4}, flags {4};
     * - 10 digital inputs {40} then 10 digital outputs {40};
     * - 2 analogue inputs {8} then 6 analogue outputs {24};
     * - 2 PWM inputs {20} then 2 PWM outputs {20}; and
     * - extension_data as a one-byte length plus X bytes.
     * Fixed bytes including the extension length field = 165.
     */
    size_t payload_size = 3u * HIL_APPLICATION_WIRE_U32_SIZE
                          + 4u * HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT
                          + 4u * HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT
                          + 4u * HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT
                          + 4u * HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT
                          + 10u * HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT
                          + 10u * HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t running_total = 0;
    /* Tick duration is an integer microsecond value; PWM periods below remain nanoseconds. */
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_duration_us.microseconds,
                                   &running_total );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->expected_tick_count,
                                   &running_total );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->flags, &running_total );
    size_t var_size = 0;
    for ( uint32_t i = 0; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_Application_Status_T config_status = HIL_APPLICATION_Digital_Config_encode(
            &( data->digital_in[i] ), &( payload[running_total] ), &var_size );
        if ( config_status != HIL_APPLICATION_STATUS_OK )
        {
            return config_status;
        }
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_Application_Status_T config_status = HIL_APPLICATION_Digital_Config_encode(
            &( data->digital_out[i] ), &( payload[running_total] ), &var_size );
        if ( config_status != HIL_APPLICATION_STATUS_OK )
        {
            return config_status;
        }
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_Application_Status_T config_status = HIL_APPLICATION_Analog_Config_encode(
            &( data->analog_in[i] ), &( payload[running_total] ), &var_size );
        if ( config_status != HIL_APPLICATION_STATUS_OK )
        {
            return config_status;
        }
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_Application_Status_T config_status = HIL_APPLICATION_Analog_Config_encode(
            &( data->analog_out[i] ), &( payload[running_total] ), &var_size );
        if ( config_status != HIL_APPLICATION_STATUS_OK )
        {
            return config_status;
        }
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_Application_Status_T config_status = HIL_APPLICATION_Pwm_Config_encode(
            &( data->pwm_in[i] ), &( payload[running_total] ), &var_size );
        if ( config_status != HIL_APPLICATION_STATUS_OK )
        {
            return config_status;
        }
        running_total += var_size;
        var_size = 0;
    }
    for ( uint32_t i = 0; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; i++ )
    {
        HIL_Application_Status_T config_status = HIL_APPLICATION_Pwm_Config_encode(
            &( data->pwm_out[i] ), &( payload[running_total] ), &var_size );
        if ( config_status != HIL_APPLICATION_STATUS_OK )
        {
            return config_status;
        }
        running_total += var_size;
        var_size = 0;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( payload_size,
                                            HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE
                                                + ( size_t )data->extension_data.size,
                                            &payload_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t                   span_size = 0u;
    HIL_Application_Status_T span_status =
        HIL_APPLICATION_Byte_Span_encode( &( data->extension_data ), &( payload[running_total] ),
                                          max_payload_size - running_total, &span_size );
    if ( span_status != HIL_APPLICATION_STATUS_OK )
    {
        return span_status;
    }
    running_total += span_size;
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
     * Current fixed Test Instruction payload = tick number {4}, 10 digital
     * outputs {10}, 6 analogue outputs {24}, and 2 PWM outputs {12}: 50 bytes.
     * Variable instruction declarations/data remain deliberately deferred and
     * are not encoded in this fixed body.
     */

    // variable data count validation
    // if ( data->variable_data_count > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK )
    // {
    //     return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    // }

    // size calculation
    size_t payload_size =
        HIL_APPLICATION_WIRE_U32_SIZE
        + HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U8_SIZE
        + HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U32_SIZE
        + HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT
              * ( HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_U16_SIZE );
    /* Variable instruction declaration sizing remains deliberately deferred. */
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
        payload[running_total++] = data->digital_outputs[i].high;
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
    /* Variable instruction declaration encoding remains deliberately deferred. */
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
    ( void )data;
    ( void )max_payload_size;
    ( void )payload;
    if ( used_size != NULL )
    {
        *used_size = 0u;
    }
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Execution_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /* Fixed control body: command u8 followed by flags uint32 little-endian. */
    const size_t payload_size = HIL_APPLICATION_WIRE_ENUM_SIZE + HIL_APPLICATION_WIRE_U32_SIZE;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    uint8_t wire_command = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->command, &wire_command ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[0]           = wire_command;
    size_t running_total = HIL_APPLICATION_WIRE_ENUM_SIZE;
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
    /* Fixed control body: command u8 followed by flags uint32 little-endian. */
    const size_t payload_size = HIL_APPLICATION_WIRE_ENUM_SIZE + HIL_APPLICATION_WIRE_U32_SIZE;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    uint8_t wire_command = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->command, &wire_command ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[0]           = wire_command;
    size_t running_total = HIL_APPLICATION_WIRE_ENUM_SIZE;
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
     * Current fixed Test Result payload = tick number {4}, 10 digital inputs
     * {10}, 2 analogue inputs {8}, 2 PWM inputs {12}, condition {1}, and
     * problem detail {4}: 39 bytes. Variable result declarations/data remain
     * deliberately deferred and are not encoded in this fixed body.
     */
    // size calculation
    size_t payload_size = HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_ENUM_SIZE
                          + HIL_APPLICATION_WIRE_U32_SIZE;
    payload_size += HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U8_SIZE;
    payload_size += HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U32_SIZE;
    payload_size += HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT
                    * ( HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_U16_SIZE );
    /* Variable result declaration sizing remains deliberately deferred. */
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    // encoding
    // tick number
    size_t running_total = 0;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    //  digital out
    for ( uint8_t i = 0; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; i++ )
    {
        payload[running_total++] = data->digital_inputs[i].high;
    }
    // Analog out
    for ( uint8_t i = 0; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                       data->analog_inputs[i].microvolts, &running_total );
    }
    // pwm out
    for ( uint8_t i = 0; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; i++ )
    {
        HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ),
                                       data->pwm_inputs[i].period_nanoseconds, &running_total );
        HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ),
                                       data->pwm_inputs[i].duty_cycle_permyriad, &running_total );
    }
    /* Variable result declaration encoding remains deliberately deferred. */
    // Condition and problem
    uint8_t wire_condition = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->condition, &wire_condition ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[running_total++] = wire_condition;
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
    ( void )used_size;
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
    |        scope {1}        |        outcome {1}         |
    |_________________________|____________________________|
    |                         |                            |
    |        reason {1}       |      tick number {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |   control command {1}   | global control command {1} |
    |_________________________|____________________________|
    |                         |
    |        detail {4}       |
    |_________________________|

    */
    const size_t payload_size =
        5u * HIL_APPLICATION_WIRE_ENUM_SIZE + 2u * HIL_APPLICATION_WIRE_U32_SIZE;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t  running_total               = 0u;
    uint8_t wire_scope                  = 0u;
    uint8_t wire_outcome                = 0u;
    uint8_t wire_reason                 = 0u;
    uint8_t wire_control_command        = 0u;
    uint8_t wire_global_control_command = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->scope, &wire_scope )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->outcome, &wire_outcome )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->reason, &wire_reason )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->control_command, &wire_control_command )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->global_control_command,
                                         &wire_global_control_command ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[running_total++] = wire_scope;
    payload[running_total++] = wire_outcome;
    payload[running_total++] = wire_reason;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    payload[running_total++] = wire_control_command;
    payload[running_total++] = wire_global_control_command;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->detail, &running_total );
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Error_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Error_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    /**
    _______________________________________________________
    |                         |                            |
    |    error category {1}    |      recoverable {1}       |
    |_________________________|____________________________|
    |                         |                            |
    |   has tick number {1}   |       tick number {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        detail {4}       |     diagnostic_data {X}    |
    |_________________________|____________________________|

    */
    size_t payload_size = HIL_APPLICATION_WIRE_ENUM_SIZE + 2u * HIL_APPLICATION_WIRE_U8_SIZE
                          + 2u * HIL_APPLICATION_WIRE_U32_SIZE
                          + HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE
                          + ( size_t )data->diagnostic_data.size;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    uint8_t wire_category = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->category, &wire_category ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[0]               = wire_category;
    size_t running_total     = HIL_APPLICATION_WIRE_ENUM_SIZE;
    payload[running_total++] = data->recoverable;
    payload[running_total++] = data->has_tick_number;
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->tick_number, &running_total );
    HIL_APPLICATION_Encode_U32_Le( &( payload[running_total] ), data->detail, &running_total );
    size_t                   span_size = 0u;
    HIL_Application_Status_T span_status =
        HIL_APPLICATION_Byte_Span_encode( &( data->diagnostic_data ), &( payload[running_total] ),
                                          max_payload_size - running_total, &span_size );
    if ( span_status != HIL_APPLICATION_STATUS_OK )
    {
        return span_status;
    }
    running_total += span_size;
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}
