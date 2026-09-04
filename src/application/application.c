/**
 * @file application.c
 * @brief Public stateless Application codec façade and message-family dispatch.
 *
 * @details This file owns publication semantics, configured message bounds,
 * exact complete-message length checks, and dispatch into existing family-specific
 * sizing/encoding/decoding/validation helpers. It does not own Transport or
 * stateful Application transaction behaviour.
 */

#include "hil_rig_protocol/application/application.h"

#include "application_decoding.h"
#include "application_encoding.h"
#include "application_internal.h"
#include "application_size.h"
#include "application_validation.h"

#include <string.h>

static HIL_Application_Status_T HIL_APPLICATION_Body_Size( const HIL_Application_Context_T* context,
                                                           const HIL_Application_Message_T* message,
                                                           size_t* payload_size )
{
    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            return HIL_APPLICATION_System_Info_Request_size(
                context, &message->subtype, message->test_id, &message->body.system_info_request,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            return HIL_APPLICATION_System_Info_Response_size(
                context, &message->subtype, message->test_id, &message->body.system_info_response,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            return HIL_APPLICATION_Test_Configuration_size(
                context, &message->subtype, message->test_id, &message->body.test_configuration,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            return HIL_APPLICATION_Test_Instructions_size(
                context, &message->subtype, message->test_id, &message->body.test_instruction,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            return HIL_APPLICATION_Variable_Instruction_Data_size(
                context, &message->subtype, message->test_id,
                &message->body.variable_instruction_data, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            return HIL_APPLICATION_Execution_Control_size(
                context, &message->subtype, message->test_id, &message->body.execution_control,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            return HIL_APPLICATION_Global_Control_size(
                context, &message->subtype, message->test_id, &message->body.global_control,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            return HIL_APPLICATION_Test_Result_size( context, &message->subtype, message->test_id,
                                                     &message->body.test_result, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            return HIL_APPLICATION_Variable_Result_Data_size(
                context, &message->subtype, message->test_id, &message->body.variable_result_data,
                payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            return HIL_APPLICATION_Response_size( context, &message->subtype, message->test_id,
                                                  &message->body.response, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }
}

static HIL_Application_Status_T
HIL_APPLICATION_Body_Encode( const HIL_Application_Context_T* context,
                             const HIL_Application_Message_T* message, size_t payload_capacity,
                             uint8_t* payload, size_t* payload_size )
{
    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            return HIL_APPLICATION_System_Info_Request_encode(
                context, &message->subtype, message->test_id, &message->body.system_info_request,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            return HIL_APPLICATION_System_Info_Response_encode(
                context, &message->subtype, message->test_id, &message->body.system_info_response,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            return HIL_APPLICATION_Test_Configuration_encode(
                context, &message->subtype, message->test_id, &message->body.test_configuration,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            return HIL_APPLICATION_Test_Instructions_encode(
                context, &message->subtype, message->test_id, &message->body.test_instruction,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            return HIL_APPLICATION_Variable_Instruction_Data_encode(
                context, &message->subtype, message->test_id,
                &message->body.variable_instruction_data, payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            return HIL_APPLICATION_Execution_Control_encode(
                context, &message->subtype, message->test_id, &message->body.execution_control,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            return HIL_APPLICATION_Global_Control_encode(
                context, &message->subtype, message->test_id, &message->body.global_control,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            return HIL_APPLICATION_Test_Result_encode( context, &message->subtype, message->test_id,
                                                       &message->body.test_result, payload_capacity,
                                                       payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            return HIL_APPLICATION_Variable_Result_Data_encode(
                context, &message->subtype, message->test_id, &message->body.variable_result_data,
                payload_capacity, payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            return HIL_APPLICATION_Response_encode( context, &message->subtype, message->test_id,
                                                    &message->body.response, payload_capacity,
                                                    payload, payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_Error_encode( context, &message->subtype, message->test_id,
                                                 &message->body.error, payload_capacity, payload,
                                                 payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }
}

static HIL_Application_Status_T HIL_APPLICATION_Body_Decode(
    const HIL_Application_Context_T* context, HIL_Application_Message_T* message,
    const uint8_t* payload, size_t payload_size, size_t* consumed_payload_size,
    uint8_t* decoded_data, size_t max_decoded_data_size, size_t* used_decoded_size )
{
    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            return HIL_APPLICATION_System_Info_Request_decode(
                context, &message->subtype, message->test_id, &message->body.system_info_request,
                payload, payload_size, consumed_payload_size, decoded_data, max_decoded_data_size,
                used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            return HIL_APPLICATION_System_Info_Response_decode(
                context, &message->subtype, message->test_id, &message->body.system_info_response,
                payload, payload_size, consumed_payload_size, decoded_data, max_decoded_data_size,
                used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            return HIL_APPLICATION_Test_Configuration_decode(
                context, &message->subtype, message->test_id, &message->body.test_configuration,
                payload, payload_size, consumed_payload_size, decoded_data, max_decoded_data_size,
                used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            return HIL_APPLICATION_Test_Instructions_decode(
                context, &message->subtype, message->test_id, &message->body.test_instruction,
                payload, payload_size, consumed_payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            return HIL_APPLICATION_Variable_Instruction_Data_decode(
                context, &message->subtype, message->test_id,
                &message->body.variable_instruction_data, payload, payload_size,
                consumed_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            return HIL_APPLICATION_Execution_Control_decode(
                context, &message->subtype, message->test_id, &message->body.execution_control,
                payload, payload_size, consumed_payload_size, decoded_data, max_decoded_data_size,
                used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            return HIL_APPLICATION_Global_Control_decode(
                context, &message->subtype, message->test_id, &message->body.global_control,
                payload, payload_size, consumed_payload_size, decoded_data, max_decoded_data_size,
                used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            return HIL_APPLICATION_Test_Result_decode( context, &message->subtype, message->test_id,
                                                       &message->body.test_result, payload,
                                                       payload_size, consumed_payload_size );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            return HIL_APPLICATION_Variable_Result_Data_decode(
                context, &message->subtype, message->test_id, &message->body.variable_result_data,
                payload, payload_size, consumed_payload_size, decoded_data, max_decoded_data_size,
                used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            return HIL_APPLICATION_Response_decode( context, &message->subtype, message->test_id,
                                                    &message->body.response, payload, payload_size,
                                                    consumed_payload_size, decoded_data,
                                                    max_decoded_data_size, used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_Error_decode( context, &message->subtype, message->test_id,
                                                 &message->body.error, payload, payload_size,
                                                 consumed_payload_size, decoded_data,
                                                 max_decoded_data_size, used_decoded_size );
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }
}

static HIL_Application_Status_T
HIL_APPLICATION_Decode_Internal( const HIL_Application_Context_T* context,
                                 const uint8_t* encoded_message, size_t encoded_message_size,
                                 HIL_Application_Message_T* out_message, uint8_t* decoded_data,
                                 size_t max_decoded_data_size, size_t* used_decoded_size )
{
    HIL_Application_Status_T   status;
    HIL_Application_Envelope_T envelope;
    size_t                     payload_size          = 0u;
    size_t                     total_size            = 0u;
    size_t                     consumed_payload_size = 0u;

    /* One bounded header parser is shared by full decode and storage-size classification. */
    status = HIL_APPLICATION_Header_Decoding( &envelope, encoded_message, encoded_message_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    out_message->has_test_id = envelope.has_test_id;
    out_message->test_id     = envelope.test_id;
    out_message->type        = envelope.type;
    out_message->subtype     = envelope.subtype;
    payload_size             = ( size_t )envelope.payload_length;
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_HEADER_SIZE_BYTES, payload_size,
                                            &total_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    /* Distinguish missing bytes from trailing bytes before exposing the body decoder. */
    if ( total_size > encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE;
    }
    if ( total_size != encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    /* The selected body sees payload bytes only, never the header or trailing input. */
    status = HIL_APPLICATION_Body_Decode(
        context, out_message, &encoded_message[HIL_APPLICATION_HEADER_SIZE_BYTES], payload_size,
        &consumed_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( consumed_payload_size != payload_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    return HIL_APPLICATION_Validate_Message( context, out_message );
}

HIL_Application_Status_T HIL_APPLICATION_Default_Config( HIL_Application_Config_T* config )
{
    if ( config == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    config->max_encoded_message_size    = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE;
    config->max_variable_data_size      = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE;
    config->max_peripheral_config_count = HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT;
    config->max_variable_transfers_per_tick =
        HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK;
    config->max_expected_tick_count = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Init( HIL_Application_Context_T*      context,
                                               const HIL_Application_Config_T* config )
{
    if ( context == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    /* Clear publication state first so every failed Init leaves a deterministic context. */
    context->initialized = 0u;
    memset( &context->config, 0, sizeof( context->config ) );
    if ( config == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    if ( config->max_encoded_message_size < HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    if ( config->max_encoded_message_size > HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( config->max_variable_data_size > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE
         || config->max_peripheral_config_count > HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT
         || config->max_variable_transfers_per_tick
                > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK )
    {
        return HIL_APPLICATION_STATUS_INVALID_COUNT;
    }
    if ( config->max_expected_tick_count > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }

    context->config      = *config;
    context->initialized = 1u;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Encoded_Size( const HIL_Application_Context_T* context,
                                                       const HIL_Application_Message_T* message,
                                                       size_t* encoded_size )
{
    HIL_Application_Status_T status;
    size_t                   body_size  = 0u;
    size_t                   total_size = 0u;

    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;
    if ( context == NULL || message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    status = HIL_APPLICATION_Validate_Common_Message_Fields( message );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    status = HIL_APPLICATION_Body_Size( context, message, &body_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( body_size > ( size_t )UINT16_MAX )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_HEADER_SIZE_BYTES, body_size,
                                            &total_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( total_size > context->config.max_encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    *encoded_size = total_size;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Encode_Message( const HIL_Application_Context_T* context,
                                                         const HIL_Application_Message_T* message,
                                                         uint8_t* out_buffer,
                                                         size_t   out_buffer_size,
                                                         size_t*  output_size )
{
    HIL_Application_Status_T status;
    size_t                   payload_capacity;
    size_t                   payload_size = 0u;
    size_t                   total_size   = 0u;
    uint16_t                 payload_size_u16;

    if ( output_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0u;
    if ( context == NULL || message == NULL || out_buffer == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    status = HIL_APPLICATION_Validate_Message( context, message );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( out_buffer_size < HIL_APPLICATION_HEADER_SIZE_BYTES )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }

    /* Header payload length starts at zero and is patched only after body encoding succeeds. */
    status = HIL_APPLICATION_Header_Encoding( message, out_buffer, out_buffer_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    /* A larger caller buffer cannot bypass the context's configured complete-message limit. */
    payload_capacity = out_buffer_size - HIL_APPLICATION_HEADER_SIZE_BYTES;
    {
        const size_t configured_payload_capacity =
            context->config.max_encoded_message_size - HIL_APPLICATION_HEADER_SIZE_BYTES;
        if ( payload_capacity > configured_payload_capacity )
        {
            payload_capacity = configured_payload_capacity;
        }
    }
    status = HIL_APPLICATION_Body_Encode( context, message, payload_capacity,
                                          &out_buffer[HIL_APPLICATION_HEADER_SIZE_BYTES],
                                          &payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( payload_size > payload_capacity
         || !HIL_APPLICATION_Size_To_U16( payload_size, &payload_size_u16 ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_HEADER_SIZE_BYTES, payload_size,
                                            &total_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( total_size > context->config.max_encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    /* Publish the fixed-width wire length only after the body-reported size is proven valid. */
    HIL_APPLICATION_Write_U16_Le(
        &out_buffer[HIL_APPLICATION_HEADER_SIZE_BYTES - HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES],
        payload_size_u16 );
    *output_size = total_size;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Decode_Storage_Size( const HIL_Application_Context_T* context,
                                     const uint8_t* encoded_message, size_t encoded_message_size,
                                     size_t* required_storage_size )
{
    HIL_Application_Envelope_T envelope;
    HIL_Application_Status_T   status;
    size_t                     payload_size = 0u;
    size_t                     total_size   = 0u;

    if ( required_storage_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *required_storage_size = 0u;
    if ( context == NULL || encoded_message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( encoded_message_size > context->config.max_encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    /* Envelope-only classification avoids a full HIL_Application_Message_T stack object. */
    status = HIL_APPLICATION_Header_Decoding( &envelope, encoded_message, encoded_message_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    payload_size = ( size_t )envelope.payload_length;
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_HEADER_SIZE_BYTES, payload_size,
                                            &total_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( total_size > encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE;
    }
    if ( total_size != encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    switch ( envelope.type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            /* Share exact fixed-body width validation with normal body decoding. */
            return HIL_APPLICATION_Fixed_Body_Validate_Size( envelope.type, payload_size );
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
}

HIL_Application_Status_T
HIL_APPLICATION_Decode_Message( const HIL_Application_Context_T* context,
                                const uint8_t* encoded_message, size_t encoded_message_size,
                                HIL_Application_Message_T* out_message, uint8_t* decoded_data,
                                size_t max_decoded_data_size, size_t* used_decoded_size )
{
    HIL_Application_Status_T status;

    if ( out_message != NULL )
    {
        out_message->type = HIL_APPLICATION_MESSAGE_TYPE_INVALID;
    }
    if ( used_decoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *used_decoded_size = 0u;
    if ( context == NULL || encoded_message == NULL || out_message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( max_decoded_data_size != 0u && decoded_data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( encoded_message_size > context->config.max_encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }

    status = HIL_APPLICATION_Decode_Internal( context, encoded_message, encoded_message_size,
                                              out_message, decoded_data, max_decoded_data_size,
                                              used_decoded_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        out_message->type  = HIL_APPLICATION_MESSAGE_TYPE_INVALID;
        *used_decoded_size = 0u;
    }
    return status;
}

HIL_Application_Status_T
HIL_APPLICATION_Validate_Message( const HIL_Application_Context_T* context,
                                  const HIL_Application_Message_T* message )
{
    HIL_Application_Status_T status;

    if ( context == NULL || message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    status = HIL_APPLICATION_Validate_Common_Message_Fields( message );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            return HIL_APPLICATION_System_Info_Request_validate(
                context, &message->body.system_info_request );
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            return HIL_APPLICATION_System_Info_Response_validate(
                context, &message->body.system_info_response );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            return HIL_APPLICATION_Test_Configuration_validate( context,
                                                                &message->body.test_configuration );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            return HIL_APPLICATION_Test_Instructions_validate( context,
                                                               &message->body.test_instruction );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            return HIL_APPLICATION_Variable_Instruction_Data_validate(
                context, &message->body.variable_instruction_data );
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            return HIL_APPLICATION_Execution_Control_validate( context,
                                                               &message->body.execution_control );
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            return HIL_APPLICATION_Global_Control_validate( context,
                                                            &message->body.global_control );
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            return HIL_APPLICATION_Test_Result_validate( context, &message->body.test_result );
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            return HIL_APPLICATION_Variable_Result_Data_validate(
                context, &message->body.variable_result_data );
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            return HIL_APPLICATION_Response_validate( context, &message->body.response );
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }
}

static HIL_Application_Status_T
HIL_APPLICATION_Validate_Decoded_Envelope( const HIL_Application_Context_T* context,
                                           const uint8_t*                   encoded_message,
                                           size_t                           encoded_message_size )
{
    HIL_Application_Message_T message;
    size_t                    used_storage = 0u;

    /* Full typed validation currently requires one temporary public message on the stack. */
    memset( &message, 0, sizeof( message ) );
    message.type = HIL_APPLICATION_MESSAGE_TYPE_INVALID;
    return HIL_APPLICATION_Decode_Internal( context, encoded_message, encoded_message_size,
                                            &message, NULL, 0u, &used_storage );
}

HIL_Application_Status_T HIL_APPLICATION_Validate_Encoded_Message(
    const HIL_Application_Context_T* context, const uint8_t* encoded_message,
    size_t encoded_message_size, size_t* required_decode_storage )
{
    HIL_Application_Status_T status;

    if ( required_decode_storage == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *required_decode_storage = 0u;
    if ( context == NULL || encoded_message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( encoded_message_size > context->config.max_encoded_message_size )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }

    status = HIL_APPLICATION_Decode_Storage_Size( context, encoded_message, encoded_message_size,
                                                  required_decode_storage );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    status =
        HIL_APPLICATION_Validate_Decoded_Envelope( context, encoded_message, encoded_message_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        *required_decode_storage = 0u;
        return status;
    }
    return HIL_APPLICATION_STATUS_OK;
}
