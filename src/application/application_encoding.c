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
#include "application_test_config_internal.h"
#include "application_validation.h"

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
     * Payload = request_firmware_git_hash {1}, query {1}, and protocol
     * major/minor/patch {6}.
     */
    const size_t payload_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    if ( context == NULL || data == NULL || payload == NULL || used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data->application_protocol_major != HIL_RIG_PROTOCOL_VERSION_MAJOR
         || data->application_protocol_minor != HIL_RIG_PROTOCOL_VERSION_MINOR
         || data->application_protocol_patch != HIL_RIG_PROTOCOL_VERSION_PATCH )
    {
        return HIL_APPLICATION_STATUS_VERSION_MISMATCH;
    }
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( data->request_firmware_git_hash > 1u
         || data->query != HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC )
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
    HIL_APPLICATION_Write_U16_Le( &payload[2], data->application_protocol_major );
    HIL_APPLICATION_Write_U16_Le( &payload[4], data->application_protocol_minor );
    HIL_APPLICATION_Write_U16_Le( &payload[6], data->application_protocol_patch );
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
    size_t                   running_total = 0u;
    size_t                   span_size     = 0u;
    HIL_Application_Status_T span_status;
    if ( context == NULL || data == NULL || payload == NULL || used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data->application_protocol_major != HIL_RIG_PROTOCOL_VERSION_MAJOR
         || data->application_protocol_minor != HIL_RIG_PROTOCOL_VERSION_MINOR
         || data->application_protocol_patch != HIL_RIG_PROTOCOL_VERSION_PATCH )
    {
        return HIL_APPLICATION_STATUS_VERSION_MISMATCH;
    }
    const size_t payload_size =
        6u * HIL_APPLICATION_WIRE_U16_SIZE + 2u * HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE
        + ( size_t )data->firmware_git_hash.size + ( size_t )data->diagnostic_data.size;
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Encode_U16_Le( payload, data->application_protocol_major, &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->application_protocol_minor,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->application_protocol_patch,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->firmware_version_major,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->firmware_version_minor,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &( payload[running_total] ), data->firmware_version_patch,
                                   &running_total );
    span_status =
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

static HIL_Application_Status_T HIL_APPLICATION_Config_Enum_encode( int value, uint8_t* payload )
{
    uint8_t wire_value = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( value, &wire_value ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[0] = wire_value;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Digital_Input_Config_encode( const HIL_Application_Digital_Input_Config_T* data,
                                             uint8_t* payload, size_t* size )
{
    payload[0] = data->enabled;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->voltage_level, &payload[1] );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Digital_Output_Config_encode( const HIL_Application_Digital_Output_Config_T* data,
                                              uint8_t* payload, size_t* size )
{
    payload[0] = data->enabled;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->voltage_level, &payload[1] );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    payload[2] = data->initial_high;
    *size      = HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Analog_Input_Config_encode( const HIL_Application_Analog_Input_Config_T* data,
                                            uint8_t* payload, size_t* size )
{
    payload[0] = data->enabled;
    *size      = HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Analog_Output_Config_encode( const HIL_Application_Analog_Output_Config_T* data,
                                             uint8_t* payload, size_t* size )
{
    payload[0] = data->enabled;
    *size      = HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Pwm_Input_Config_encode( const HIL_Application_Pwm_Input_Config_T* data,
                                         uint8_t* payload, size_t* size )
{
    payload[0] = data->enabled;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->voltage_level, &payload[1] );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Pwm_Output_Config_encode( const HIL_Application_Pwm_Output_Config_T* data,
                                          uint8_t* payload, size_t* size )
{
    size_t running_total     = 0u;
    payload[running_total++] = data->enabled;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->voltage_level, &payload[running_total] );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    ++running_total;
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->initial_period_nanoseconds,
                                   &running_total );
    HIL_APPLICATION_Encode_U16_Le( &payload[running_total], data->initial_duty_cycle_permyriad,
                                   &running_total );
    if ( running_total != HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_RECORD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Can_Config_encode( const HIL_Application_Can_Config_T* data, uint8_t* payload,
                                   size_t* size )
{
    size_t running_total     = 0u;
    payload[running_total++] = data->enabled;
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->bit_rate, &running_total );
    HIL_APPLICATION_Encode_U16_Le( &payload[running_total], data->filter_id, &running_total );
    HIL_APPLICATION_Encode_U16_Le( &payload[running_total], data->filter_mask, &running_total );
    if ( running_total != HIL_APPLICATION_TEST_CONFIG_CAN_RECORD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_CAN_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Spi_Config_encode( const HIL_Application_Spi_Config_T* data, uint8_t* payload,
                                   size_t* size )
{
    size_t running_total     = 0u;
    payload[running_total++] = data->enabled;
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->bit_rate, &running_total );
    HIL_Application_Status_T status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->role, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->data_width, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->bit_order, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status = HIL_APPLICATION_Config_Enum_encode( ( int )data->clock_polarity,
                                                 &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->clock_phase, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    if ( running_total != HIL_APPLICATION_TEST_CONFIG_SPI_RECORD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_SPI_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Uart_Config_encode( const HIL_Application_Uart_Config_T* data, uint8_t* payload,
                                    size_t* size )
{
    size_t running_total     = 0u;
    payload[running_total++] = data->enabled;
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->baud_rate, &running_total );
    HIL_Application_Status_T status = HIL_APPLICATION_Config_Enum_encode(
        ( int )data->electrical_mode, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->word_length, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status = HIL_APPLICATION_Config_Enum_encode( ( int )data->parity, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->stop_bits, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    payload[running_total++] = data->rx_enabled;
    payload[running_total++] = data->tx_enabled;
    if ( running_total != HIL_APPLICATION_TEST_CONFIG_UART_RECORD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_UART_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_I2c_Config_encode( const HIL_Application_I2c_Config_T* data, uint8_t* payload,
                                   size_t* size )
{
    size_t running_total     = 0u;
    payload[running_total++] = data->enabled;
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->bit_rate, &running_total );
    HIL_Application_Status_T status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->role, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    HIL_APPLICATION_Encode_U16_Le( &payload[running_total], data->own_address_7bit,
                                   &running_total );
    status =
        HIL_APPLICATION_Config_Enum_encode( ( int )data->voltage_level, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    status = HIL_APPLICATION_Config_Enum_encode( ( int )data->pull_up, &payload[running_total++] );
    if ( status != HIL_APPLICATION_STATUS_OK )
        return status;
    if ( running_total != HIL_APPLICATION_TEST_CONFIG_I2C_RECORD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = HIL_APPLICATION_TEST_CONFIG_I2C_RECORD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    size_t                   required_size = 0u;
    size_t                   running_total = 0u;
    size_t                   record_size   = 0u;
    HIL_Application_Status_T status;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    status =
        HIL_APPLICATION_Test_Configuration_size( context, sub_type, test_id, data, &required_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    /* Prove that the complete body fits before writing any Test Configuration byte. */
    if ( max_payload_size < required_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->tick_duration_us.microseconds,
                                   &running_total );
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->expected_tick_count,
                                   &running_total );
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->flags, &running_total );

#define HIL_APPLICATION_ENCODE_CONFIG_ARRAY( array_, count_, encoder_ )                            \
    do                                                                                             \
    {                                                                                              \
        for ( size_t i_ = 0u; i_ < ( count_ ); ++i_ )                                              \
        {                                                                                          \
            status = encoder_( &( array_ )[i_], &payload[running_total], &record_size );           \
            if ( status != HIL_APPLICATION_STATUS_OK )                                             \
            {                                                                                      \
                return status;                                                                     \
            }                                                                                      \
            running_total += record_size;                                                          \
        }                                                                                          \
    } while ( 0 )

    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->digital_in,
                                         HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Digital_Input_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->digital_out,
                                         HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Digital_Output_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->analog_in,
                                         HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Analog_Input_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->analog_out,
                                         HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Analog_Output_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->pwm_in, HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Pwm_Input_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->pwm_out, HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Pwm_Output_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->can, HIL_APPLICATION_CAN_CHANNEL_COUNT,
                                         HIL_APPLICATION_Can_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->spi, HIL_APPLICATION_SPI_CHANNEL_COUNT,
                                         HIL_APPLICATION_Spi_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->uart, HIL_APPLICATION_UART_CHANNEL_COUNT,
                                         HIL_APPLICATION_Uart_Config_encode );
    HIL_APPLICATION_ENCODE_CONFIG_ARRAY( data->i2c, HIL_APPLICATION_I2C_CHANNEL_COUNT,
                                         HIL_APPLICATION_I2c_Config_encode );

#undef HIL_APPLICATION_ENCODE_CONFIG_ARRAY

    status = HIL_APPLICATION_Byte_Span_encode( &data->extension_data, &payload[running_total],
                                               max_payload_size - running_total, &record_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += record_size;
    if ( running_total != required_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    size_t running_total = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( max_payload_size < HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->tick_number, &running_total );
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        payload[running_total++] = data->digital_outputs[i].high;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->analog_outputs[i].microvolts,
                                       &running_total );
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Encode_U32_Le( &payload[running_total],
                                       data->pwm_outputs[i].period_nanoseconds, &running_total );
        HIL_APPLICATION_Encode_U16_Le( &payload[running_total],
                                       data->pwm_outputs[i].duty_cycle_permyriad, &running_total );
    }
    if ( running_total != HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

/**
 * @brief Encode one 4-byte-aligned TLV record into the payload buffer.
 *
 * @details Writes the one-byte peripheral identifier, one-byte channel, two-byte
 * little-endian payload length, payload bytes, and zero padding bytes to reach a
 * 4-byte boundary.
 *
 * @param[in]     peripheral_type Logical peripheral type.
 * @param[in]     channel         Logical peripheral channel index.
 * @param[in]     span            Payload byte span.
 * @param[out]    payload         Destination payload buffer.
 * @param[in,out] running_total   Accumulated payload bytes written.
 * @return Application status.
 */
static HIL_Application_Status_T
HIL_APPLICATION_Aligned_Record_encode( HIL_Application_Peripheral_Type_T peripheral_type,
                                       uint8_t channel, const HIL_Application_Byte_Span_T* span,
                                       uint8_t* payload, size_t* running_total )
{
    uint8_t wire_periph = 0u;

    if ( !HIL_APPLICATION_Enum_To_U8( ( int )peripheral_type, &wire_periph ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    payload[( *running_total )++] = wire_periph;
    payload[( *running_total )++] = channel;
    HIL_APPLICATION_Encode_U16_Le( &payload[*running_total], ( uint16_t )span->size,
                                   running_total );

    if ( span->size != 0u )
    {
        memcpy( &payload[*running_total], span->data, span->size );
        *running_total += span->size;
    }

    const size_t pad = HIL_APPLICATION_Align4_Padding( ( size_t )span->size );
    for ( size_t p = 0u; p < pad; ++p )
    {
        payload[( *running_total )++] = 0u;
    }

    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Update_Instruction_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Update_Instruction_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    HIL_Application_Status_T status;
    size_t                   required_size = 0u;
    size_t                   running_total = 0u;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    status =
        HIL_APPLICATION_Update_Instruction_size( context, sub_type, test_id, data, &required_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( max_payload_size < required_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->tick_number, &running_total );
    payload[running_total++] = data->operation_count;
    payload[running_total++] = data->flags;
    HIL_APPLICATION_Encode_U16_Le( &payload[running_total], 0u, &running_total );

    for ( size_t i = 0u; i < ( size_t )data->operation_count; ++i )
    {
        const HIL_Application_Logical_Operation_T* op = &data->operations[i];
        status = HIL_APPLICATION_Aligned_Record_encode( op->peripheral_type, op->channel,
                                                        &op->payload, payload, &running_total );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }

    if ( running_total != required_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }

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

HIL_Application_Status_T HIL_APPLICATION_Finalize_Test_Upload_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Finalize_Test_Upload_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    ( void )sub_type;
    ( void )test_id;
    if ( data == NULL || payload == NULL || used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( max_payload_size < HIL_APPLICATION_WIRE_U32_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Finalize_Test_Upload_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    HIL_APPLICATION_Write_U32_Le( payload, data->flags );
    *used_size = HIL_APPLICATION_WIRE_U32_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Result_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    size_t  running_total  = 0u;
    uint8_t wire_condition = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( max_payload_size < HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->condition, &wire_condition ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->tick_number, &running_total );
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        payload[running_total++] = data->digital_inputs[i].high;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->analog_inputs[i].microvolts,
                                       &running_total );
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Encode_U32_Le( &payload[running_total],
                                       data->pwm_inputs[i].period_nanoseconds, &running_total );
        HIL_APPLICATION_Encode_U16_Le( &payload[running_total],
                                       data->pwm_inputs[i].duty_cycle_permyriad, &running_total );
    }
    payload[running_total++] = wire_condition;
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->problem_detail, &running_total );
    if ( running_total != HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Test_Result_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Test_Result_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size )
{
    HIL_Application_Status_T status;
    size_t                   required_size  = 0u;
    size_t                   running_total  = 0u;
    uint8_t                  wire_condition = 0u;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_APPLICATION_Variable_Test_Result_size( context, sub_type, test_id, data,
                                                        &required_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( max_payload_size < required_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->condition, &wire_condition ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->tick_number, &running_total );
    payload[running_total++] = data->record_count;
    payload[running_total++] = wire_condition;
    payload[running_total++] = data->flags;
    payload[running_total++] = 0u; /* reserved */
    HIL_APPLICATION_Encode_U32_Le( &payload[running_total], data->problem_detail, &running_total );

    for ( size_t i = 0u; i < ( size_t )data->record_count; ++i )
    {
        const HIL_Application_Captured_Record_T* rec = &data->records[i];
        status = HIL_APPLICATION_Aligned_Record_encode( rec->peripheral_type, rec->channel,
                                                        &rec->data, payload, &running_total );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }

    if ( running_total != required_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }

    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Response_encode( const HIL_Application_Context_T*  context,
                                                          const HIL_Application_Response_T* data,
                                                          size_t max_payload_size, uint8_t* payload,
                                                          size_t* used_size )
{
    uint8_t wire_scope                  = 0u;
    uint8_t wire_outcome                = 0u;
    uint8_t wire_reason                 = 0u;
    uint8_t wire_control_command        = 0u;
    uint8_t wire_global_control_command = 0u;
    size_t  running_total               = HIL_APPLICATION_RESPONSE_TICK_NUMBER_OFFSET;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    {
        const HIL_Application_Status_T status = HIL_APPLICATION_Response_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    if ( max_payload_size < HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->scope, &wire_scope )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->outcome, &wire_outcome )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->reason, &wire_reason )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->control_command, &wire_control_command )
         || !HIL_APPLICATION_Enum_To_U8( ( int )data->global_control_command,
                                         &wire_global_control_command ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[HIL_APPLICATION_RESPONSE_SCOPE_OFFSET]   = wire_scope;
    payload[HIL_APPLICATION_RESPONSE_OUTCOME_OFFSET] = wire_outcome;
    payload[HIL_APPLICATION_RESPONSE_REASON_OFFSET]  = wire_reason;
    HIL_APPLICATION_Encode_U32_Le( &payload[HIL_APPLICATION_RESPONSE_TICK_NUMBER_OFFSET],
                                   data->tick_number, &running_total );
    if ( running_total != HIL_APPLICATION_RESPONSE_CONTROL_COMMAND_OFFSET )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    payload[HIL_APPLICATION_RESPONSE_CONTROL_COMMAND_OFFSET]        = wire_control_command;
    payload[HIL_APPLICATION_RESPONSE_GLOBAL_CONTROL_COMMAND_OFFSET] = wire_global_control_command;
    running_total = HIL_APPLICATION_RESPONSE_DETAIL_OFFSET;
    HIL_APPLICATION_Encode_U32_Le( &payload[HIL_APPLICATION_RESPONSE_DETAIL_OFFSET], data->detail,
                                   &running_total );
    if ( running_total != HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Error_encode( const HIL_Application_Context_T* context,
                                                       const HIL_Application_Error_T*   data,
                                                       size_t max_payload_size, uint8_t* payload,
                                                       size_t* used_size )
{
    size_t  payload_size  = 0u;
    size_t  span_size     = 0u;
    size_t  running_total = HIL_APPLICATION_ERROR_TICK_NUMBER_OFFSET;
    uint8_t wire_category = 0u;

    if ( used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    {
        const HIL_Application_Status_T status = HIL_APPLICATION_Error_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE,
                                            data->diagnostic_data.size, &payload_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( max_payload_size < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )data->category, &wire_category ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    payload[HIL_APPLICATION_ERROR_CATEGORY_OFFSET]        = wire_category;
    payload[HIL_APPLICATION_ERROR_RECOVERABLE_OFFSET]     = data->recoverable;
    payload[HIL_APPLICATION_ERROR_HAS_TICK_NUMBER_OFFSET] = data->has_tick_number;
    HIL_APPLICATION_Encode_U32_Le( &payload[HIL_APPLICATION_ERROR_TICK_NUMBER_OFFSET],
                                   data->tick_number, &running_total );
    HIL_APPLICATION_Encode_U32_Le( &payload[HIL_APPLICATION_ERROR_DETAIL_OFFSET], data->detail,
                                   &running_total );
    if ( running_total != HIL_APPLICATION_ERROR_DIAGNOSTIC_LENGTH_OFFSET )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Byte_Span_encode( &data->diagnostic_data, &payload[running_total],
                                              max_payload_size - running_total, &span_size );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    if ( running_total + span_size != payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *used_size = payload_size;
    return HIL_APPLICATION_STATUS_OK;
}
