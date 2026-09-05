/**
 * @file application_decoding.c
 * @brief Message-specific Application payload decoders.
 *
 * @details These helpers receive payload bytes only. For normal façade decoding
 * the supplied payload extent is exactly the length declared by the already
 * parsed common envelope. Encoded truncation inside a body is MALFORMED_MESSAGE;
 * BUFFER_TOO_SMALL is reserved for insufficient caller decode storage.
 */

#include "application_decoding.h"
#include "application_internal.h"
#include "application_test_config_internal.h"
#include "application_size.h"

#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"

#include <string.h>

HIL_Application_Status_T
HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_Application_Message_Type_T type, size_t payload_size )
{
    size_t expected_size;

    switch ( type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            expected_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            expected_size = HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            expected_size = HIL_APPLICATION_WIRE_ENUM_SIZE + HIL_APPLICATION_WIRE_U32_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            expected_size = HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            expected_size =
                5u * HIL_APPLICATION_WIRE_ENUM_SIZE + 2u * HIL_APPLICATION_WIRE_U32_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            /* Test Configuration is variable-length because of its extension and is
             * validated by the dedicated bounded scanner, never this fixed-body helper. */
            return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }

    return payload_size == expected_size ? HIL_APPLICATION_STATUS_OK
                                         : HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
}

void HIL_APPLICATION_Decode_U16_Le( uint16_t* dest, const uint8_t* src, size_t* running_total )
{
    *dest = HIL_APPLICATION_Read_U16_Le( src );
    *running_total += HIL_APPLICATION_WIRE_U16_SIZE;
}

void HIL_APPLICATION_Decode_U32_Le( uint32_t* dest, const uint8_t* src, size_t* running_total )
{
    *dest = ( ( uint32_t )src[0] ) | ( ( uint32_t )src[1] << 8 ) | ( ( uint32_t )src[2] << 16 )
            | ( ( uint32_t )src[3] << 24 );
    *running_total += HIL_APPLICATION_WIRE_U32_SIZE;
}

void HIL_APPLICATION_Decode_U64_Le( uint64_t* dest, const uint8_t* src, size_t* running_total )
{
    *dest = ( ( uint64_t )src[0] ) | ( ( uint64_t )src[1] << 8 ) | ( ( uint64_t )src[2] << 16 )
            | ( ( uint64_t )src[3] << 24 ) | ( ( uint64_t )src[4] << 32 )
            | ( ( uint64_t )src[5] << 40 ) | ( ( uint64_t )src[6] << 48 )
            | ( ( uint64_t )src[7] << 56 );
    *running_total += HIL_APPLICATION_WIRE_U64_SIZE;
}

static HIL_Application_Status_T
HIL_APPLICATION_Byte_Span_decode( HIL_Application_Byte_Span_T* byte_span, const uint8_t* payload,
                                  size_t payload_size, uint8_t* decoded_data_dest,
                                  size_t decoded_data_capacity, size_t* encoded_used,
                                  size_t* decoded_used )
{
    size_t  required_encoded = 0u;
    uint8_t span_size        = 0u;

    if ( encoded_used == NULL || decoded_used == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_used = 0u;
    *decoded_used = 0u;
    if ( byte_span == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    /* Missing encoded span metadata is malformed peer input, not a caller-buffer shortage. */
    if ( payload_size < HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    span_size = payload[0];
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE,
                                            ( size_t )span_size, &required_encoded ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    /* Prove the complete encoded span is present before considering destination storage. */
    if ( payload_size < required_encoded )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( ( size_t )span_size > decoded_data_capacity )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( span_size != 0u && decoded_data_dest == NULL )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    if ( span_size != 0u )
    {
        memcpy( decoded_data_dest, &payload[HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE], span_size );
    }
    /* Publish the span and consumed counts only after every check and copy succeeds. */
    byte_span->size = span_size;
    byte_span->data = ( span_size == 0u ) ? NULL : decoded_data_dest;
    *encoded_used   = required_encoded;
    *decoded_used   = ( size_t )span_size;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Request_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    const size_t max_decoded_data_size, size_t* used_decoded_size )
{
    HIL_Application_Status_T status;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;

    status = HIL_APPLICATION_Fixed_Body_Validate_Size(
        HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST, max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    data->request_firmware_git_hash = payload[0];
    data->query   = ( HIL_Application_System_Info_Query_T )payload[HIL_APPLICATION_WIRE_U8_SIZE];
    *payload_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    *used_decoded_size = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    const size_t             fixed_numeric_size = 6u * HIL_APPLICATION_WIRE_U16_SIZE;
    size_t                   running_total      = 0u;
    size_t                   decoded_total      = 0u;
    size_t                   encoded_span_used  = 0u;
    size_t                   decoded_span_used  = 0u;
    HIL_Application_Status_T status;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( max_payload_size < fixed_numeric_size + 2u * HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    HIL_APPLICATION_Decode_U16_Le( &data->application_protocol_major, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U16_Le( &data->application_protocol_minor, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U16_Le( &data->application_protocol_patch, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U16_Le( &data->firmware_version_major, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U16_Le( &data->firmware_version_minor, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U16_Le( &data->firmware_version_patch, &payload[running_total],
                                   &running_total );

    status = HIL_APPLICATION_Byte_Span_decode(
        &data->diagnostic_data, &payload[running_total], max_payload_size - running_total,
        decoded_data, max_decoded_data_size, &encoded_span_used, &decoded_span_used );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += encoded_span_used;
    decoded_total += decoded_span_used;

    status = HIL_APPLICATION_Byte_Span_decode(
        &data->firmware_git_hash, &payload[running_total], max_payload_size - running_total,
        decoded_data == NULL ? NULL : &decoded_data[decoded_total],
        max_decoded_data_size - decoded_total, &encoded_span_used, &decoded_span_used );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += encoded_span_used;
    decoded_total += decoded_span_used;

    *payload_size      = running_total;
    *used_decoded_size = decoded_total;
    return HIL_APPLICATION_STATUS_OK;
}

static void
HIL_APPLICATION_Digital_Input_Config_decode( HIL_Application_Digital_Input_Config_T* data,
                                             const uint8_t* payload, size_t* size )
{
    data->enabled       = payload[0];
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )payload[1];
    *size               = HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_RECORD_SIZE;
}

static void
HIL_APPLICATION_Digital_Output_Config_decode( HIL_Application_Digital_Output_Config_T* data,
                                              const uint8_t* payload, size_t* size )
{
    data->enabled       = payload[0];
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )payload[1];
    data->initial_high  = payload[2];
    *size               = HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_RECORD_SIZE;
}

static void HIL_APPLICATION_Analog_Input_Config_decode( HIL_Application_Analog_Input_Config_T* data,
                                                        const uint8_t* payload, size_t* size )
{
    data->enabled = payload[0];
    *size         = HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_RECORD_SIZE;
}

static void
HIL_APPLICATION_Analog_Output_Config_decode( HIL_Application_Analog_Output_Config_T* data,
                                             const uint8_t* payload, size_t* size )
{
    data->enabled = payload[0];
    *size         = HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_RECORD_SIZE;
}

static void HIL_APPLICATION_Pwm_Input_Config_decode( HIL_Application_Pwm_Input_Config_T* data,
                                                     const uint8_t* payload, size_t* size )
{
    data->enabled       = payload[0];
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )payload[1];
    *size               = HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_RECORD_SIZE;
}

static void HIL_APPLICATION_Pwm_Output_Config_decode( HIL_Application_Pwm_Output_Config_T* data,
                                                      const uint8_t* payload, size_t* size )
{
    size_t offset       = 0u;
    data->enabled       = payload[offset++];
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->initial_period_nanoseconds, &payload[offset], &offset );
    HIL_APPLICATION_Decode_U16_Le( &data->initial_duty_cycle_permyriad, &payload[offset], &offset );
    *size = HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_RECORD_SIZE;
}

static void HIL_APPLICATION_Can_Config_decode( HIL_Application_Can_Config_T* data,
                                               const uint8_t* payload, size_t* size )
{
    size_t offset = 0u;
    data->enabled = payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->bit_rate, &payload[offset], &offset );
    data->termination_enabled = payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->capture_limit_bytes, &payload[offset], &offset );
    *size = HIL_APPLICATION_TEST_CONFIG_CAN_RECORD_SIZE;
}

static void HIL_APPLICATION_Spi_Config_decode( HIL_Application_Spi_Config_T* data,
                                               const uint8_t* payload, size_t* size )
{
    size_t offset = 0u;
    data->enabled = payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->bit_rate, &payload[offset], &offset );
    data->role           = ( HIL_Application_Bus_Role_T )payload[offset++];
    data->data_width     = ( HIL_Application_Spi_Data_Width_T )payload[offset++];
    data->bit_order      = ( HIL_Application_Spi_Bit_Order_T )payload[offset++];
    data->clock_polarity = ( HIL_Application_Spi_Clock_Polarity_T )payload[offset++];
    data->clock_phase    = ( HIL_Application_Spi_Clock_Phase_T )payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->capture_limit_bytes, &payload[offset], &offset );
    *size = HIL_APPLICATION_TEST_CONFIG_SPI_RECORD_SIZE;
}

static void HIL_APPLICATION_Uart_Config_decode( HIL_Application_Uart_Config_T* data,
                                                const uint8_t* payload, size_t* size )
{
    size_t offset = 0u;
    data->enabled = payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->baud_rate, &payload[offset], &offset );
    data->electrical_mode = ( HIL_Application_Uart_Electrical_Mode_T )payload[offset++];
    data->word_length     = ( HIL_Application_Uart_Word_Length_T )payload[offset++];
    data->parity          = ( HIL_Application_Uart_Parity_T )payload[offset++];
    data->stop_bits       = ( HIL_Application_Uart_Stop_Bits_T )payload[offset++];
    data->rx_enabled      = payload[offset++];
    data->tx_enabled      = payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->capture_limit_bytes, &payload[offset], &offset );
    *size = HIL_APPLICATION_TEST_CONFIG_UART_RECORD_SIZE;
}

static void HIL_APPLICATION_I2c_Config_decode( HIL_Application_I2c_Config_T* data,
                                               const uint8_t* payload, size_t* size )
{
    size_t offset = 0u;
    data->enabled = payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->bit_rate, &payload[offset], &offset );
    data->role = ( HIL_Application_Bus_Role_T )payload[offset++];
    HIL_APPLICATION_Decode_U16_Le( &data->own_address_7bit, &payload[offset], &offset );
    data->voltage_level = ( HIL_Application_I2c_Voltage_Level_T )payload[offset++];
    data->pull_up       = ( HIL_Application_I2c_Pull_Up_T )payload[offset++];
    HIL_APPLICATION_Decode_U32_Le( &data->capture_limit_bytes, &payload[offset], &offset );
    *size = HIL_APPLICATION_TEST_CONFIG_I2C_RECORD_SIZE;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Configuration_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    size_t                   running_total         = 0u;
    size_t                   record_size           = 0u;
    size_t                   span_encoded          = 0u;
    size_t                   span_decoded          = 0u;
    size_t                   required_payload_size = 0u;
    uint8_t                  extension_size;
    HIL_Application_Status_T status;
    ( void )sub_type;
    ( void )test_id;

    if ( payload_size == NULL || used_decoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *payload_size      = 0u;
    *used_decoded_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    /* No array loop may begin until the complete fixed body, including extension length, exists. */
    if ( max_payload_size < HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    extension_size = payload[HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET];
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE,
                                            ( size_t )extension_size, &required_payload_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    /* Preserve malformed-shape precedence over configured resource-policy checks. */
    if ( max_payload_size != required_payload_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( ( size_t )extension_size > context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    memset( data, 0, sizeof( *data ) );
    HIL_APPLICATION_Decode_U32_Le( &data->tick_duration_us.microseconds, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data->expected_tick_count, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data->flags, &payload[running_total], &running_total );

#define HIL_APPLICATION_DECODE_CONFIG_ARRAY( array_, count_, decoder_ )                            \
    do                                                                                             \
    {                                                                                              \
        for ( size_t i_ = 0u; i_ < ( count_ ); ++i_ )                                              \
        {                                                                                          \
            decoder_( &( array_ )[i_], &payload[running_total], &record_size );                    \
            running_total += record_size;                                                          \
        }                                                                                          \
    } while ( 0 )

    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->digital_in,
                                         HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Digital_Input_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->digital_out,
                                         HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Digital_Output_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->analog_in,
                                         HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Analog_Input_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->analog_out,
                                         HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Analog_Output_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->pwm_in, HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Pwm_Input_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->pwm_out, HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT,
                                         HIL_APPLICATION_Pwm_Output_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->can, HIL_APPLICATION_CAN_CHANNEL_COUNT,
                                         HIL_APPLICATION_Can_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->spi, HIL_APPLICATION_SPI_CHANNEL_COUNT,
                                         HIL_APPLICATION_Spi_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->uart, HIL_APPLICATION_UART_CHANNEL_COUNT,
                                         HIL_APPLICATION_Uart_Config_decode );
    HIL_APPLICATION_DECODE_CONFIG_ARRAY( data->i2c, HIL_APPLICATION_I2C_CHANNEL_COUNT,
                                         HIL_APPLICATION_I2c_Config_decode );

#undef HIL_APPLICATION_DECODE_CONFIG_ARRAY

    if ( running_total != HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    status = HIL_APPLICATION_Byte_Span_decode(
        &data->extension_data, &payload[running_total], max_payload_size - running_total,
        decoded_data, max_decoded_data_size, &span_encoded, &span_decoded );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += span_encoded;
    *payload_size      = running_total;
    *used_decoded_size = span_decoded;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Instruction_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size )
{
    HIL_Application_Status_T status;
    size_t                   running_total = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( payload_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *payload_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_APPLICATION_Fixed_Body_Validate_Size(
        HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION, max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    HIL_APPLICATION_Decode_U32_Le( &data->tick_number, &payload[running_total], &running_total );
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        data->digital_outputs[i].high = payload[running_total++];
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Decode_U32_Le( &data->analog_outputs[i].microvolts, &payload[running_total],
                                       &running_total );
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Decode_U32_Le( &data->pwm_outputs[i].period_nanoseconds,
                                       &payload[running_total], &running_total );
        HIL_APPLICATION_Decode_U16_Le( &data->pwm_outputs[i].duty_cycle_permyriad,
                                       &payload[running_total], &running_total );
    }
    if ( running_total != HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size = running_total;
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
    ( void )data;
    ( void )payload;
    ( void )max_payload_size;
    ( void )payload_size;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    ( void )used_decoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Execution_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Execution_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    HIL_Application_Status_T status;
    size_t                   running_total = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;

    status = HIL_APPLICATION_Fixed_Body_Validate_Size(
        HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL, max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    data->command = ( HIL_Application_Control_Command_T )payload[running_total++];
    HIL_APPLICATION_Decode_U32_Le( &data->flags, &payload[running_total], &running_total );
    *payload_size      = running_total;
    *used_decoded_size = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Global_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Global_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    HIL_Application_Status_T status;
    size_t                   running_total = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;

    status = HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL,
                                                       max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    data->command = ( HIL_Application_Global_Control_Command_T )payload[running_total++];
    HIL_APPLICATION_Decode_U32_Le( &data->flags, &payload[running_total], &running_total );
    *payload_size      = running_total;
    *used_decoded_size = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size )
{
    HIL_Application_Status_T status;
    size_t                   running_total = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( payload_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *payload_size = 0u;
    if ( data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT,
                                                       max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    HIL_APPLICATION_Decode_U32_Le( &data->tick_number, &payload[running_total], &running_total );
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        data->digital_inputs[i].high = payload[running_total++];
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Decode_U32_Le( &data->analog_inputs[i].microvolts, &payload[running_total],
                                       &running_total );
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Decode_U32_Le( &data->pwm_inputs[i].period_nanoseconds,
                                       &payload[running_total], &running_total );
        HIL_APPLICATION_Decode_U16_Le( &data->pwm_inputs[i].duty_cycle_permyriad,
                                       &payload[running_total], &running_total );
    }
    data->condition = ( HIL_Application_Result_Condition_T )payload[running_total++];
    HIL_APPLICATION_Decode_U32_Le( &data->problem_detail, &payload[running_total], &running_total );
    if ( running_total != HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size = running_total;
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
    ( void )max_payload_size;
    ( void )payload_size;
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
    HIL_Application_Status_T status;
    size_t                   running_total = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;

    status = HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
                                                       max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    data->scope   = ( HIL_Application_Response_Scope_T )payload[running_total++];
    data->outcome = ( HIL_Application_Response_Outcome_T )payload[running_total++];
    data->reason  = ( HIL_Application_Response_Reason_T )payload[running_total++];
    HIL_APPLICATION_Decode_U32_Le( &data->tick_number, &payload[running_total], &running_total );
    data->control_command = ( HIL_Application_Control_Command_T )payload[running_total++];
    data->global_control_command =
        ( HIL_Application_Global_Control_Command_T )payload[running_total++];
    HIL_APPLICATION_Decode_U32_Le( &data->detail, &payload[running_total], &running_total );
    *payload_size      = running_total;
    *used_decoded_size = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Error_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Error_T* data, const uint8_t* payload,
    size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    const size_t fixed_size = HIL_APPLICATION_WIRE_ENUM_SIZE + 2u * HIL_APPLICATION_WIRE_U8_SIZE
                              + 2u * HIL_APPLICATION_WIRE_U32_SIZE;
    size_t                   running_total = 0u;
    size_t                   span_encoded  = 0u;
    size_t                   span_decoded  = 0u;
    HIL_Application_Status_T status;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( max_payload_size < fixed_size + HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    data->category        = ( HIL_Application_Error_Category_T )payload[running_total++];
    data->recoverable     = payload[running_total++];
    data->has_tick_number = payload[running_total++];
    HIL_APPLICATION_Decode_U32_Le( &data->tick_number, &payload[running_total], &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data->detail, &payload[running_total], &running_total );

    status = HIL_APPLICATION_Byte_Span_decode(
        &data->diagnostic_data, &payload[running_total], max_payload_size - running_total,
        decoded_data, max_decoded_data_size, &span_encoded, &span_decoded );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    running_total += span_encoded;
    *payload_size      = running_total;
    *used_decoded_size = span_decoded;
    return HIL_APPLICATION_STATUS_OK;
}
