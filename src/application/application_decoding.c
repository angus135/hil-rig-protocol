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
#include "application_validation.h"

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
            expected_size = HIL_APPLICATION_CONTROL_FIXED_ENCODE_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_FINALIZE_TEST_UPLOAD:
            expected_size = HIL_APPLICATION_WIRE_U32_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            expected_size = HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            expected_size = HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
        case HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT:
            /* Test Configuration, Update Instruction, and Variable Test Result are variable-length
             * and are validated by dedicated bounded scanners, never this fixed-body helper. */
            return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }

    return payload_size == expected_size ? HIL_APPLICATION_STATUS_OK
                                         : HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
}

HIL_Application_Status_T HIL_APPLICATION_Error_Scan( const HIL_Application_Context_T* context,
                                                     const uint8_t* payload, size_t payload_size,
                                                     size_t* decoded_storage_size )
{
    size_t  complete_size   = 0u;
    uint8_t diagnostic_size = 0u;

    if ( context == NULL || payload == NULL || decoded_storage_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *decoded_storage_size = 0u;
    if ( payload_size < HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    diagnostic_size = payload[HIL_APPLICATION_ERROR_DIAGNOSTIC_LENGTH_OFFSET];
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE,
                                            diagnostic_size, &complete_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( payload_size < complete_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( payload_size != complete_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( diagnostic_size > context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    *decoded_storage_size = diagnostic_size;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_System_Info_Response_Scan( const HIL_Application_Context_T* context,
                                           const uint8_t* payload, size_t payload_size,
                                           size_t* decoded_storage_size )
{
    const size_t fixed_numeric_size = 6u * HIL_APPLICATION_WIRE_U16_SIZE;
    size_t       offset             = fixed_numeric_size;
    size_t       total_storage      = 0u;
    uint8_t      diagnostic_size;
    uint8_t      git_hash_size;

    if ( context == NULL || payload == NULL || decoded_storage_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *decoded_storage_size = 0u;
    if ( payload_size < fixed_numeric_size + 2u * HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    diagnostic_size = payload[offset++];
    if ( payload_size - offset < diagnostic_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( diagnostic_size > context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    offset += diagnostic_size;
    if ( payload_size - offset < HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    git_hash_size = payload[offset++];
    if ( payload_size - offset < git_hash_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( git_hash_size > context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    offset += git_hash_size;
    if ( offset != payload_size
         || !HIL_APPLICATION_Checked_Add_Size( diagnostic_size, git_hash_size, &total_storage ) )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    *decoded_storage_size = total_storage;
    return HIL_APPLICATION_STATUS_OK;
}

/**
 * @brief Common bounded scanner for a sequence of 4-byte-aligned TLV records.
 *
 * @details Validates the 4-byte TLV header, non-zero payload length bounded by
 * max_variable_data_size and UINT8_MAX, exact zero pad bytes to align the record
 * to a 4-byte boundary, and exact consumption of the declared payload extent.
 */
static HIL_Application_Status_T HIL_APPLICATION_Aligned_Records_Scan(
    const HIL_Application_Context_T* context, const uint8_t* payload, size_t payload_size,
    size_t offset_start, size_t record_count, HIL_Application_Message_Type_T type,
    size_t* total_payload_bytes )
{
    size_t   offset                                                        = offset_start;
    size_t   total_payload                                                 = 0u;
    uint16_t seen_peripheral_channels[HIL_APPLICATION_PERIPHERAL_CAN + 1u] = { 0u };

    for ( size_t i = 0u; i < record_count; ++i )
    {
        if ( payload_size - offset < HIL_APPLICATION_RECORD_HEADER_SIZE )
        {
            return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
        }
        const HIL_Application_Peripheral_Type_T peripheral_type =
            ( HIL_Application_Peripheral_Type_T )payload[offset];
        const uint8_t  channel            = payload[offset + 1u];
        const uint16_t record_payload_len = HIL_APPLICATION_Read_U16_Le( &payload[offset + 2u] );
        offset += HIL_APPLICATION_RECORD_HEADER_SIZE;

        if ( record_payload_len == 0u || record_payload_len > ( uint16_t )UINT8_MAX )
        {
            return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
        }
        if ( ( size_t )record_payload_len > context->config.max_variable_data_size )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if ( payload_size - offset < ( size_t )record_payload_len )
        {
            return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
        }
        HIL_Application_Byte_Span_T record_span = { &payload[offset],
                                                    ( uint8_t )record_payload_len };
        HIL_Application_Status_T    status      = HIL_APPLICATION_Record_Pair_Mark(
            seen_peripheral_channels, HIL_APPLICATION_PERIPHERAL_CAN + 1u, peripheral_type,
            channel );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
        status = type == HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION
                     ? HIL_APPLICATION_Logical_Operation_Fields_validate( context, peripheral_type,
                                                                          channel, &record_span )
                     : HIL_APPLICATION_Captured_Record_Fields_validate( context, peripheral_type,
                                                                        channel, &record_span );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
        offset += ( size_t )record_payload_len;

        const size_t pad = HIL_APPLICATION_Align4_Padding( ( size_t )record_payload_len );
        if ( payload_size - offset < pad )
        {
            return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
        }
        for ( size_t p = 0u; p < pad; ++p )
        {
            if ( payload[offset + p] != 0u )
            {
                return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
            }
        }
        offset += pad;

        if ( !HIL_APPLICATION_Checked_Add_Size( total_payload, ( size_t )record_payload_len,
                                                &total_payload ) )
        {
            return HIL_APPLICATION_STATUS_INVALID_LENGTH;
        }
    }

    if ( offset != payload_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    *total_payload_bytes = total_payload;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Update_Instruction_Scan( const HIL_Application_Context_T* context,
                                         const uint8_t* payload, size_t payload_size,
                                         size_t* decoded_storage_size )
{
    if ( context == NULL || payload == NULL || decoded_storage_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *decoded_storage_size = 0u;

    if ( payload_size < HIL_APPLICATION_UPDATE_INSTRUCTION_HEADER_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    const uint8_t  op_count = payload[HIL_APPLICATION_UPDATE_INSTRUCTION_COUNT_OFFSET];
    const uint32_t tick_number =
        HIL_APPLICATION_Read_U32_Le( &payload[HIL_APPLICATION_UPDATE_INSTRUCTION_TICK_OFFSET] );
    const uint8_t  flags = payload[HIL_APPLICATION_UPDATE_INSTRUCTION_FLAGS_OFFSET];
    const uint16_t reserved =
        HIL_APPLICATION_Read_U16_Le( &payload[HIL_APPLICATION_UPDATE_INSTRUCTION_RESERVED_OFFSET] );

    if ( reserved != 0u )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( flags > HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( op_count == 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    size_t                   total_payload_bytes = 0u;
    HIL_Application_Status_T status              = HIL_APPLICATION_Aligned_Records_Scan(
        context, payload, payload_size, HIL_APPLICATION_UPDATE_INSTRUCTION_HEADER_SIZE,
        ( size_t )op_count, HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION, &total_payload_bytes );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    size_t struct_array_size = 0u;
    if ( !HIL_APPLICATION_Checked_Mul_Size( ( size_t )op_count,
                                            sizeof( HIL_Application_Logical_Operation_T ),
                                            &struct_array_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    size_t aligned_struct_array_size = 0u;
    if ( !HIL_APPLICATION_Align_Up_Size( struct_array_size,
                                         HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT,
                                         &aligned_struct_array_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( aligned_struct_array_size, total_payload_bytes,
                                            decoded_storage_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }

    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Variable_Test_Result_Scan( const HIL_Application_Context_T* context,
                                           const uint8_t* payload, size_t payload_size,
                                           size_t* decoded_storage_size )
{
    if ( context == NULL || payload == NULL || decoded_storage_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *decoded_storage_size = 0u;

    if ( payload_size < HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    const uint8_t  rec_count = payload[HIL_APPLICATION_VARIABLE_TEST_RESULT_COUNT_OFFSET];
    const uint32_t tick_number =
        HIL_APPLICATION_Read_U32_Le( &payload[HIL_APPLICATION_VARIABLE_TEST_RESULT_TICK_OFFSET] );
    const uint8_t  condition      = payload[HIL_APPLICATION_VARIABLE_TEST_RESULT_CONDITION_OFFSET];
    const uint8_t  flags          = payload[HIL_APPLICATION_VARIABLE_TEST_RESULT_FLAGS_OFFSET];
    const uint8_t  reserved       = payload[HIL_APPLICATION_VARIABLE_TEST_RESULT_RESERVED_OFFSET];
    const uint32_t problem_detail = HIL_APPLICATION_Read_U32_Le(
        &payload[HIL_APPLICATION_VARIABLE_TEST_RESULT_PROBLEM_DETAIL_OFFSET] );

    if ( reserved != 0u )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( flags > HIL_APPLICATION_RESULT_FLAG_HAS_MORE_CHUNKS )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( condition != ( uint8_t )HIL_APPLICATION_RESULT_CONDITION_OK
         && condition != ( uint8_t )HIL_APPLICATION_RESULT_CONDITION_PARTIAL
         && condition != ( uint8_t )HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( condition == ( uint8_t )HIL_APPLICATION_RESULT_CONDITION_OK && problem_detail != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    if ( rec_count == 0u )
    {
        if ( payload_size != HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE )
        {
            return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
        }
        *decoded_storage_size = 0u;
        return HIL_APPLICATION_STATUS_OK;
    }

    size_t                   total_payload_bytes = 0u;
    HIL_Application_Status_T status              = HIL_APPLICATION_Aligned_Records_Scan(
        context, payload, payload_size, HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE,
        ( size_t )rec_count, HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT,
        &total_payload_bytes );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    size_t struct_array_size = 0u;
    if ( !HIL_APPLICATION_Checked_Mul_Size( ( size_t )rec_count,
                                            sizeof( HIL_Application_Captured_Record_T ),
                                            &struct_array_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    size_t aligned_struct_array_size = 0u;
    if ( !HIL_APPLICATION_Align_Up_Size( struct_array_size,
                                         HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT,
                                         &aligned_struct_array_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( aligned_struct_array_size, total_payload_bytes,
                                            decoded_storage_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }

    return HIL_APPLICATION_STATUS_OK;
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
    data->query = ( HIL_Application_System_Info_Query_T )payload[HIL_APPLICATION_WIRE_U8_SIZE];
    data->application_protocol_major = HIL_APPLICATION_Read_U16_Le( &payload[2] );
    data->application_protocol_minor = HIL_APPLICATION_Read_U16_Le( &payload[4] );
    data->application_protocol_patch = HIL_APPLICATION_Read_U16_Le( &payload[6] );
    *payload_size                    = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    *used_decoded_size               = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    size_t                   running_total     = 0u;
    size_t                   decoded_total     = 0u;
    size_t                   encoded_span_used = 0u;
    size_t                   decoded_span_used = 0u;
    HIL_Application_Status_T status;
    ( void )sub_type;
    ( void )test_id;
    status = HIL_APPLICATION_System_Info_Response_Scan( context, payload, max_payload_size,
                                                        &decoded_total );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( decoded_total > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
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
    decoded_total = decoded_span_used;

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
    HIL_APPLICATION_Decode_U16_Le( &data->filter_id, &payload[offset], &offset );
    HIL_APPLICATION_Decode_U16_Le( &data->filter_mask, &payload[offset], &offset );
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
    *size                = HIL_APPLICATION_TEST_CONFIG_SPI_RECORD_SIZE;
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
    *size                 = HIL_APPLICATION_TEST_CONFIG_UART_RECORD_SIZE;
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
    *size               = HIL_APPLICATION_TEST_CONFIG_I2C_RECORD_SIZE;
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

HIL_Application_Status_T
HIL_APPLICATION_Test_Configuration_Encoded_Validate( const HIL_Application_Context_T* context,
                                                     const uint8_t* payload, size_t payload_size,
                                                     size_t* decoded_storage_size )
{
    HIL_Application_Test_Configuration_T data = { 0 };
    size_t                               running_total;
    size_t                               record_size = 0u;

    if ( context == NULL || payload == NULL || decoded_storage_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *decoded_storage_size = 0u;
    if ( payload_size < HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    const uint8_t extension_size = payload[HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET];
    size_t        required_payload_size = 0u;
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE,
                                            extension_size, &required_payload_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( payload_size != required_payload_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( ( size_t )extension_size > context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    running_total = 0u;
    HIL_APPLICATION_Decode_U32_Le( &data.tick_duration_us.microseconds, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data.expected_tick_count, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data.flags, &payload[running_total], &running_total );

#define HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( array_, count_, decoder_ )                  \
    do                                                                                             \
    {                                                                                              \
        for ( size_t i_ = 0u; i_ < ( count_ ); ++i_ )                                              \
        {                                                                                          \
            decoder_( &( array_ )[i_], &payload[running_total], &record_size );                    \
            running_total += record_size;                                                          \
        }                                                                                          \
    } while ( 0 )

    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.digital_in,
                                                   HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Digital_Input_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.digital_out,
                                                   HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Digital_Output_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.analog_in,
                                                   HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Analog_Input_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.analog_out,
                                                   HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Analog_Output_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.pwm_in,
                                                   HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Pwm_Input_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.pwm_out,
                                                   HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Pwm_Output_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.can, HIL_APPLICATION_CAN_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Can_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.spi, HIL_APPLICATION_SPI_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Spi_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.uart, HIL_APPLICATION_UART_CHANNEL_COUNT,
                                                   HIL_APPLICATION_Uart_Config_decode );
    HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY( data.i2c, HIL_APPLICATION_I2C_CHANNEL_COUNT,
                                                   HIL_APPLICATION_I2c_Config_decode );

#undef HIL_APPLICATION_VALIDATE_ENCODED_CONFIG_ARRAY

    if ( running_total != HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    data.extension_data.size = extension_size;
    data.extension_data.data =
        extension_size == 0u ? NULL : &payload[HIL_APPLICATION_TEST_CONFIG_EXTENSION_DATA_OFFSET];
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Test_Configuration_validate( context, &data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    *decoded_storage_size = extension_size;
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

HIL_Application_Status_T HIL_APPLICATION_Update_Instruction_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Update_Instruction_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )sub_type;
    ( void )test_id;

    if ( payload_size == NULL || used_decoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *payload_size      = 0u;
    *used_decoded_size = 0u;
    if ( context == NULL || data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    size_t                   required_storage = 0u;
    HIL_Application_Status_T status           = HIL_APPLICATION_Update_Instruction_Scan(
        context, payload, max_payload_size, &required_storage );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( required_storage > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( required_storage != 0u && decoded_data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    size_t running_payload = 0u;
    HIL_APPLICATION_Decode_U32_Le( &data->tick_number, &payload[running_payload],
                                   &running_payload );
    data->operation_count = payload[running_payload++];
    data->flags           = payload[running_payload++];
    running_payload += 2u; /* skip reserved bytes */

    HIL_Application_Logical_Operation_T* operations =
        ( HIL_Application_Logical_Operation_T* )( void* )decoded_data;
    data->operations = operations;

    size_t struct_array_size =
        ( size_t )data->operation_count * sizeof( HIL_Application_Logical_Operation_T );
    size_t pool_offset = 0u;
    if ( !HIL_APPLICATION_Align_Up_Size( struct_array_size,
                                         HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT, &pool_offset ) )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    uint8_t* payload_pool         = decoded_data + pool_offset;
    size_t   payload_bytes_copied = 0u;

    for ( size_t i = 0u; i < ( size_t )data->operation_count; ++i )
    {
        operations[i].peripheral_type =
            ( HIL_Application_Peripheral_Type_T )payload[running_payload++];
        operations[i].channel         = payload[running_payload++];
        const uint16_t op_payload_len = HIL_APPLICATION_Read_U16_Le( &payload[running_payload] );
        running_payload += HIL_APPLICATION_WIRE_U16_SIZE;

        operations[i].payload.size = ( uint8_t )op_payload_len;
        operations[i].payload.data = &payload_pool[payload_bytes_copied];

        memcpy( &payload_pool[payload_bytes_copied], &payload[running_payload],
                ( size_t )op_payload_len );
        running_payload += ( size_t )op_payload_len;
        payload_bytes_copied += ( size_t )op_payload_len;

        const size_t pad = HIL_APPLICATION_Align4_Padding( ( size_t )op_payload_len );
        running_payload += pad;
    }

    if ( running_payload != max_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = running_payload;
    *used_decoded_size = pool_offset + payload_bytes_copied;
    return HIL_APPLICATION_STATUS_OK;
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

HIL_Application_Status_T HIL_APPLICATION_Finalize_Test_Upload_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Finalize_Test_Upload_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )decoded_data;
    ( void )max_decoded_data_size;
    if ( payload_size == NULL || used_decoded_size == NULL || data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *payload_size                         = 0u;
    *used_decoded_size                    = 0u;
    const HIL_Application_Status_T status = HIL_APPLICATION_Fixed_Body_Validate_Size(
        HIL_APPLICATION_MESSAGE_TYPE_FINALIZE_TEST_UPLOAD, max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    data->flags   = HIL_APPLICATION_Read_U32_Le( payload );
    *payload_size = HIL_APPLICATION_WIRE_U32_SIZE;
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

HIL_Application_Status_T HIL_APPLICATION_Variable_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    ( void )sub_type;
    ( void )test_id;

    if ( payload_size == NULL || used_decoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *payload_size      = 0u;
    *used_decoded_size = 0u;
    if ( context == NULL || data == NULL || payload == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    size_t                   required_storage = 0u;
    HIL_Application_Status_T status           = HIL_APPLICATION_Variable_Test_Result_Scan(
        context, payload, max_payload_size, &required_storage );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( required_storage > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( required_storage != 0u && decoded_data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    size_t running_payload = 0u;
    HIL_APPLICATION_Decode_U32_Le( &data->tick_number, &payload[running_payload],
                                   &running_payload );
    data->record_count = payload[running_payload++];
    data->condition    = ( HIL_Application_Result_Condition_T )payload[running_payload++];
    data->flags        = payload[running_payload++];
    running_payload += 1u; /* skip reserved byte */
    HIL_APPLICATION_Decode_U32_Le( &data->problem_detail, &payload[running_payload],
                                   &running_payload );

    if ( data->record_count == 0u )
    {
        data->records      = NULL;
        *payload_size      = running_payload;
        *used_decoded_size = 0u;
        return HIL_APPLICATION_STATUS_OK;
    }

    HIL_Application_Captured_Record_T* records =
        ( HIL_Application_Captured_Record_T* )( void* )decoded_data;
    data->records = records;

    size_t struct_array_size =
        ( size_t )data->record_count * sizeof( HIL_Application_Captured_Record_T );
    size_t pool_offset = 0u;
    if ( !HIL_APPLICATION_Align_Up_Size( struct_array_size,
                                         HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT, &pool_offset ) )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    uint8_t* payload_pool         = decoded_data + pool_offset;
    size_t   payload_bytes_copied = 0u;

    for ( size_t i = 0u; i < ( size_t )data->record_count; ++i )
    {
        records[i].peripheral_type =
            ( HIL_Application_Peripheral_Type_T )payload[running_payload++];
        records[i].channel          = payload[running_payload++];
        const uint16_t rec_data_len = HIL_APPLICATION_Read_U16_Le( &payload[running_payload] );
        running_payload += HIL_APPLICATION_WIRE_U16_SIZE;

        records[i].data.size = ( uint8_t )rec_data_len;
        records[i].data.data = &payload_pool[payload_bytes_copied];

        memcpy( &payload_pool[payload_bytes_copied], &payload[running_payload],
                ( size_t )rec_data_len );
        running_payload += ( size_t )rec_data_len;
        payload_bytes_copied += ( size_t )rec_data_len;

        const size_t pad = HIL_APPLICATION_Align4_Padding( ( size_t )rec_data_len );
        running_payload += pad;
    }

    if ( running_payload != max_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = running_payload;
    *used_decoded_size = pool_offset + payload_bytes_copied;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Response_decode( HIL_Application_Response_T* data,
                                                          const uint8_t*              payload,
                                                          size_t  max_payload_size,
                                                          size_t* payload_size,
                                                          size_t* used_decoded_size )
{
    HIL_Application_Status_T status;
    size_t                   running_total = HIL_APPLICATION_RESPONSE_TICK_NUMBER_OFFSET;

    status = HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
                                                       max_payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    data->scope =
        ( HIL_Application_Response_Scope_T )payload[HIL_APPLICATION_RESPONSE_SCOPE_OFFSET];
    data->outcome =
        ( HIL_Application_Response_Outcome_T )payload[HIL_APPLICATION_RESPONSE_OUTCOME_OFFSET];
    data->reason =
        ( HIL_Application_Response_Reason_T )payload[HIL_APPLICATION_RESPONSE_REASON_OFFSET];
    HIL_APPLICATION_Decode_U32_Le(
        &data->tick_number, &payload[HIL_APPLICATION_RESPONSE_TICK_NUMBER_OFFSET], &running_total );
    data->control_command = ( HIL_Application_Control_Command_T )
        payload[HIL_APPLICATION_RESPONSE_CONTROL_COMMAND_OFFSET];
    data->global_control_command = ( HIL_Application_Global_Control_Command_T )
        payload[HIL_APPLICATION_RESPONSE_GLOBAL_CONTROL_COMMAND_OFFSET];
    running_total = HIL_APPLICATION_RESPONSE_DETAIL_OFFSET;
    HIL_APPLICATION_Decode_U32_Le( &data->detail, &payload[HIL_APPLICATION_RESPONSE_DETAIL_OFFSET],
                                   &running_total );
    if ( running_total != HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = running_total;
    *used_decoded_size = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Error_decode( const HIL_Application_Context_T* context,
                              HIL_Application_Error_T* data, const uint8_t* payload,
                              size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
                              size_t max_decoded_data_size, size_t* used_decoded_size )
{
    size_t                   required_storage = 0u;
    size_t                   running_total    = HIL_APPLICATION_ERROR_TICK_NUMBER_OFFSET;
    size_t                   span_encoded     = 0u;
    size_t                   span_decoded     = 0u;
    HIL_Application_Status_T status;

    status = HIL_APPLICATION_Error_Scan( context, payload, max_payload_size, &required_storage );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( required_storage > max_decoded_data_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    data->category =
        ( HIL_Application_Error_Category_T )payload[HIL_APPLICATION_ERROR_CATEGORY_OFFSET];
    data->recoverable     = payload[HIL_APPLICATION_ERROR_RECOVERABLE_OFFSET];
    data->has_tick_number = payload[HIL_APPLICATION_ERROR_HAS_TICK_NUMBER_OFFSET];
    HIL_APPLICATION_Decode_U32_Le(
        &data->tick_number, &payload[HIL_APPLICATION_ERROR_TICK_NUMBER_OFFSET], &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data->detail, &payload[HIL_APPLICATION_ERROR_DETAIL_OFFSET],
                                   &running_total );
    if ( running_total != HIL_APPLICATION_ERROR_DIAGNOSTIC_LENGTH_OFFSET )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }

    status = HIL_APPLICATION_Byte_Span_decode(
        &data->diagnostic_data, &payload[running_total], max_payload_size - running_total,
        decoded_data, max_decoded_data_size, &span_encoded, &span_decoded );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( running_total + span_encoded != max_payload_size )
    {
        return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *payload_size      = max_payload_size;
    *used_decoded_size = span_decoded;
    return HIL_APPLICATION_STATUS_OK;
}
