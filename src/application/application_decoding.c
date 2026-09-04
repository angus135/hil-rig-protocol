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
            expected_size =
                HIL_APPLICATION_WIRE_U32_SIZE
                + HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U8_SIZE
                + HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U32_SIZE
                + HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT
                      * ( HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_U16_SIZE );
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            expected_size = HIL_APPLICATION_WIRE_ENUM_SIZE + HIL_APPLICATION_WIRE_U32_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            expected_size =
                HIL_APPLICATION_WIRE_U32_SIZE
                + HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U8_SIZE
                + HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U32_SIZE
                + HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT
                      * ( HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_U16_SIZE )
                + HIL_APPLICATION_WIRE_ENUM_SIZE + HIL_APPLICATION_WIRE_U32_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            expected_size =
                5u * HIL_APPLICATION_WIRE_ENUM_SIZE + 2u * HIL_APPLICATION_WIRE_U32_SIZE;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
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

static HIL_Application_Status_T
HIL_APPLICATION_Channel_Id_decode( HIL_Application_Channel_Id_T* data, const uint8_t* payload )
{
    data->peripheral = ( HIL_Application_Peripheral_Type_T )payload[0];
    data->channel    = HIL_APPLICATION_Read_U16_Le( &payload[HIL_APPLICATION_WIRE_ENUM_SIZE] );
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Digital_Config_decode( HIL_Application_Digital_Config_T* data,
                                       const uint8_t* payload, size_t* size )
{
    HIL_APPLICATION_Channel_Id_decode( &data->channel, payload );
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )
        payload[HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE];
    *size = HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE + HIL_APPLICATION_WIRE_ENUM_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Analog_Config_decode( HIL_Application_Analog_Config_T* data, const uint8_t* payload,
                                      size_t* size )
{
    HIL_APPLICATION_Channel_Id_decode( &data->channel, payload );
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )
        payload[HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE];
    *size = HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE + HIL_APPLICATION_WIRE_ENUM_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Pwm_Config_decode( HIL_Application_Pwm_Config_T* data, const uint8_t* payload,
                                   size_t* size )
{
    size_t offset = HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
    HIL_APPLICATION_Channel_Id_decode( &data->channel, payload );
    HIL_APPLICATION_Decode_U32_Le( &data->period_nanoseconds, &payload[offset], &offset );
    HIL_APPLICATION_Decode_U16_Le( &data->initial_duty_cycle_permyriad, &payload[offset], &offset );
    data->voltage_level = ( HIL_Application_Peripheral_Config_Voltage_Level_T )payload[offset++];
    *size               = offset;
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

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_decode( HIL_Application_Peripheral_Config_T* data,
                                          const uint8_t* payload, size_t* size )
{
    /* Reserved for the later variable peripheral-configuration representation. */
    ( void )data;
    ( void )payload;
    if ( size != NULL )
    {
        *size = 0u;
    }
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Configuration_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size )
{
    const size_t digital_size =
        HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE + HIL_APPLICATION_WIRE_ENUM_SIZE;
    const size_t pwm_size = HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE + HIL_APPLICATION_WIRE_U32_SIZE
                            + HIL_APPLICATION_WIRE_U16_SIZE + HIL_APPLICATION_WIRE_ENUM_SIZE;
    size_t                   fixed_size    = 3u * HIL_APPLICATION_WIRE_U32_SIZE;
    size_t                   running_total = 0u;
    size_t                   item_size     = 0u;
    size_t                   span_encoded  = 0u;
    size_t                   span_decoded  = 0u;
    size_t                   count_size    = 0u;
    HIL_Application_Status_T status;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( !HIL_APPLICATION_Checked_Mul_Size( HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT
                                                + HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT
                                                + HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT
                                                + HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT,
                                            digital_size, &count_size )
         || !HIL_APPLICATION_Checked_Add_Size( fixed_size, count_size, &fixed_size )
         || !HIL_APPLICATION_Checked_Mul_Size( HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT
                                                   + HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT,
                                               pwm_size, &count_size )
         || !HIL_APPLICATION_Checked_Add_Size( fixed_size, count_size, &fixed_size )
         || !HIL_APPLICATION_Checked_Add_Size( fixed_size, HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE,
                                               &fixed_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( max_payload_size < fixed_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    /* Test tick duration is encoded in microseconds; PWM periods later remain nanoseconds. */
    HIL_APPLICATION_Decode_U32_Le( &data->tick_duration_us.microseconds, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data->expected_tick_count, &payload[running_total],
                                   &running_total );
    HIL_APPLICATION_Decode_U32_Le( &data->flags, &payload[running_total], &running_total );

    /* Fixed configuration arrays are encoded in input-before-output order for each family. */
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Digital_Config_decode( &data->digital_in[i], &payload[running_total],
                                               &item_size );
        running_total += item_size;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Digital_Config_decode( &data->digital_out[i], &payload[running_total],
                                               &item_size );
        running_total += item_size;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Analog_Config_decode( &data->analog_in[i], &payload[running_total],
                                              &item_size );
        running_total += item_size;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Analog_Config_decode( &data->analog_out[i], &payload[running_total],
                                              &item_size );
        running_total += item_size;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Pwm_Config_decode( &data->pwm_in[i], &payload[running_total], &item_size );
        running_total += item_size;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        HIL_APPLICATION_Pwm_Config_decode( &data->pwm_out[i], &payload[running_total], &item_size );
        running_total += item_size;
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
