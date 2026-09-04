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
 |                                              |
 |            End Payload Flag {1}              |
 |______________________________________________|
*/

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/application/application_size.h"
#include "hil_rig_protocol/application/application_encoding.h"
#include "hil_rig_protocol/application/application_decoding.h"
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
#include "hil_rig_protocol/application/application_validation.h"

#include <string.h>
#include <stdio.h>

/**
 * @brief Initialise an Application layer configuration with default limits.
 *
 * Sets all configurable Application layer limits to their protocol-defined
 * maximum values. No memory is allocated and no Application context is
 * modified.
 *
 * @param[out] config Pointer to the configuration structure to initialise.
 *
 * @return HIL_APPLICATION_STATUS_OK
 *         Configuration was successfully initialised.
 * @return HIL_APPLICATION_STATUS_INVALID_ARGUMENT
 *         The config pointer is NULL.
 */
HIL_Application_Status_T HIL_APPLICATION_Default_Config( HIL_Application_Config_T* config )
{

    if ( config == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    /**
     * Largest complete encoded Application message accepted or produced.
     *
     * Integration must configure Transport's maximum Application-message size
     * to at least this value. The codec does not inspect Transport configuration.
     */
    config->max_encoded_message_size = HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE;
    /** Largest byte span in one variable instruction/result/error field. */
    config->max_variable_data_size = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE;
    /** Maximum peripheral configuration records in one typed configuration. */
    config->max_peripheral_config_count = HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT;
    /** Maximum variable-data declarations in one typed tick body. */
    config->max_variable_transfers_per_tick =
        HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK;
    /**
     * Largest expected_tick_count value accepted structurally.
     *
     * This is not retained upload capacity and causes no per-tick allocation.
     */
    config->max_expected_tick_count = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT;
    return HIL_APPLICATION_STATUS_OK;
}

/**
 * @brief Initialise an Application context with a supplied configuration.
 *
 * - Validates the supplied Application configuration against the absolute
 *   protocol limits.
 * - Copies the validated configuration into the Application context.
 * - Marks the Application context as initialised after successful validation.
 * - Does not allocate memory.
 * - Does not retain a pointer to the supplied configuration.
 * - The Transport layer must support at least the configured maximum encoded
 *   Application message size.
 *
 * @param[out] context
 *        Pointer to the Application context to initialise.
 * @param[in] config
 *        Pointer to the Application configuration to validate and copy.
 *
 * @return HIL_APPLICATION_STATUS_OK
 *         The Application context was successfully initialised.
 * @return HIL_APPLICATION_STATUS_INVALID_ARGUMENT
 *         The context or config pointer is NULL.
 * @return HIL_APPLICATION_STATUS_INVALID_LENGTH
 *         The configured maximum expected tick count exceeds the protocol
 *         limit.
 * @return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL
 *         The configured maximum encoded message size is smaller than the
 *         required protocol maximum.
 * @return HIL_APPLICATION_STATUS_INVALID_COUNT
 *         A configured variable-data size, peripheral count, or variable
 *         transfers-per-tick count exceeds its protocol limit.
 */
HIL_Application_Status_T HIL_APPLICATION_Init( HIL_Application_Context_T*      context,
                                               const HIL_Application_Config_T* config )
{

    if ( config == NULL || context == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    // validate config
    if ( config->max_expected_tick_count > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    if ( config->max_encoded_message_size < HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    /** Largest byte span in one variable instruction/result/error field. */
    if ( config->max_variable_data_size > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE )
    {
        return HIL_APPLICATION_STATUS_INVALID_COUNT;
    }
    if ( config->max_peripheral_config_count > HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT )
    {
        return HIL_APPLICATION_STATUS_INVALID_COUNT;
    }
    if ( config->max_variable_transfers_per_tick
         > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK )
    {
        return HIL_APPLICATION_STATUS_INVALID_COUNT;
    }

    // assign config
    context->config      = *config;
    context->initialized = 1;

    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Encoded_Size( const HIL_Application_Context_T* context,
                                                       const HIL_Application_Message_T* message,
                                                       size_t* encoded_size )
{

    ( void )context;
    ( void )message;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;  // Size functions are not implemented. size is
                                                    // calculated and checked in the encode/decode
                                                    // functions
    // size_t size = 0;
    // if ( encoded_size == NULL || context == NULL || message == NULL )
    // {
    //     return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    // }
    // switch ( message->type )
    // {
    //     case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
    //         return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
    //         HIL_APPLICATION_System_Info_Request_size(
    //             context, &( message->subtype ), message->test_id,
    //             &( message->body.system_info_request ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
    //         HIL_APPLICATION_System_Info_Response_size(
    //             context, &( message->subtype ), message->test_id,
    //             &( message->body.system_info_response ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
    //         HIL_APPLICATION_Test_Configuration_size( context, &( message->subtype ),
    //                                                  message->test_id,
    //                                                  &( message->body.test_configuration ), &size
    //                                                  );
    //         ;
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
    //         HIL_APPLICATION_Test_Instructions_size( context, &( message->subtype ),
    //                                                 message->test_id,
    //                                                 &( message->body.test_instruction ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
    //         HIL_APPLICATION_Variable_Instruction_Data_size(
    //             context, &( message->subtype ), message->test_id,
    //             &( message->body.variable_instruction_data ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
    //         HIL_APPLICATION_Execution_Control_size( context, &( message->subtype ),
    //                                                 message->test_id,
    //                                                 &( message->body.execution_control ), &size
    //                                                 );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
    //         HIL_APPLICATION_Global_Control_size( context, &( message->subtype ),
    //         message->test_id,
    //                                              &( message->body.global_control ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
    //         HIL_APPLICATION_Test_Result_size( context, &( message->subtype ), message->test_id,
    //                                           &( message->body.test_result ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
    //         HIL_APPLICATION_Variable_Result_Data_size(
    //             context, &( message->subtype ), message->test_id,
    //             &( message->body.variable_result_data ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
    //         HIL_APPLICATION_Response_size( context, &( message->subtype ), message->test_id,
    //                                        &( message->body.response ), &size );
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
    //         return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    //         break;

    //     case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
    //         return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
    //         break;

    //     default:
    //         return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
    //         break;
    // }
    // *encoded_size = size + HIL_APPLICATION_HEADER_SIZE_BYTES;
    // return HIL_APPLICATION_STATUS_OK;
}

/**
 * @brief Encode an Application message into a byte buffer.
 *
 * - Encodes the Application message into the supplied output buffer.
 * - Encodes the Application header before encoding the message payload.
 * - Selects the appropriate message-specific encoder based on the message type.
 * - Writes the encoded payload size into the Application header.
 * - Appends the end-of-payload flag after the encoded payload.
 * - Returns the total number of bytes written through output_size.
 * - Clears the output buffer before encoding.
 * - Clears the output buffer if an encoding error occurs.
 *
 * @param[in] context
 *        Pointer to the Application context.
 * @param[in] message
 *        Pointer to the Application message to encode.
 * @param[out] out_buffer
 *        Buffer into which the encoded message is written.
 * @param[in] out_buffer_size
 *        Total capacity of out_buffer in bytes.
 * @param[out] output_size
 *        Pointer to receive the number of bytes written to out_buffer.
 *
 * @return HIL_APPLICATION_STATUS_OK
 *         The message was successfully encoded.
 * @return HIL_APPLICATION_STATUS_INVALID_ARGUMENT
 *         A required pointer argument is NULL.
 * @return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL
 *         The output buffer is too small to contain the required message
 *         header and payload end flag.
 * @return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE
 *         The message type is invalid or unsupported.
 * @return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED
 *         The message type is reserved and not implemented.
 * @return Other HIL_Application_Status_T values
 *         An error returned by the message-specific encoder.
 */
HIL_Application_Status_T HIL_APPLICATION_Encode_Message( const HIL_Application_Context_T* context,
                                                         const HIL_Application_Message_T* message,
                                                         uint8_t* out_buffer,
                                                         size_t   out_buffer_size,
                                                         size_t*  output_size )
{

    if ( output_size == NULL || out_buffer == NULL || context == NULL || message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( out_buffer_size < HIL_APPLICATION_HEADER_SIZE_BYTES + 1 )  // +1 for the payload end flag
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    size_t max_payload_size =
        out_buffer_size - HIL_APPLICATION_HEADER_SIZE_BYTES
        - 1;  // calculate the maximum allow-able payload size (-1 for end payload flag)
    size_t   payload_size         = 0;
    uint8_t* payload_size_pointer = &(
        out_buffer[HIL_APPLICATION_HEADER_SIZE_BYTES - HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES] );
    // Set available memory to 0
    memset( out_buffer, 0, out_buffer_size );
    // Encode Header
    HIL_Application_Status_T tracker =
        HIL_APPLICATION_Header_Encoding( message, context, out_buffer );
    if ( tracker != HIL_APPLICATION_STATUS_OK )
    {
        // clear available memory
        memset( out_buffer, 0, out_buffer_size );
        return tracker;
    };
    uint8_t* payload =
        &( out_buffer[HIL_APPLICATION_HEADER_SIZE_BYTES] );  // create pointer for the payload
    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            tracker = HIL_APPLICATION_System_Info_Request_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.system_info_request ), max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            tracker = HIL_APPLICATION_System_Info_Response_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.system_info_response ), max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            tracker = HIL_APPLICATION_Test_Configuration_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.test_configuration ), max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            tracker = HIL_APPLICATION_Test_Instructions_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.test_instruction ), max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            tracker = HIL_APPLICATION_Variable_Instruction_Data_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.variable_instruction_data ), max_payload_size, payload,
                &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            tracker = HIL_APPLICATION_Execution_Control_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.execution_control ), max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            tracker = HIL_APPLICATION_Global_Control_encode(
                context, &( message->subtype ), message->test_id, &( message->body.global_control ),
                max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            tracker = HIL_APPLICATION_Test_Result_encode(
                context, &( message->subtype ), message->test_id, &( message->body.test_result ),
                max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            tracker = HIL_APPLICATION_Variable_Result_Data_encode(
                context, &( message->subtype ), message->test_id,
                &( message->body.variable_result_data ), max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            tracker = HIL_APPLICATION_Response_encode(
                context, &( message->subtype ), message->test_id, &( message->body.response ),
                max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            tracker = HIL_APPLICATION_Error_encode( context, &( message->subtype ),
                                                    message->test_id, &( message->body.error ),
                                                    max_payload_size, payload, &payload_size );
            memcpy( payload_size_pointer, &payload_size,
                    HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                // clear available memory
                memset( out_buffer, 0, out_buffer_size );
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;

        default:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
    }
    // Check to make sure the flag is still 0 (message write didn't go out of bounds)
    if ( payload[payload_size + HIL_APPLICATION_HEADER_SIZE_BYTES] != 0 )
    {
        // clear available memory
        memset( out_buffer, 0, out_buffer_size );
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    payload[payload_size + HIL_APPLICATION_HEADER_SIZE_BYTES] = 1U;
    *output_size = payload_size + HIL_APPLICATION_HEADER_SIZE_BYTES + 1;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Decode_Storage_Size( const HIL_Application_Context_T* context,
                                     const uint8_t* encoded_message, size_t encoded_message_size,
                                     size_t* required_storage_size )
{

    if ( required_storage_size != NULL )
    {
        *required_storage_size = 0U;
    }
    ( void )context;
    ( void )encoded_message;
    ( void )encoded_message_size;

    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Decode an encoded Application message.
 *
 * - Decodes an encoded Application message into a typed
 *   HIL_Application_Message_T structure.
 * - Decodes the Application header before decoding the message payload.
 * - Selects the appropriate message-specific decoder based on the message type.
 * - Decodes variable-length data into caller-provided storage.
 * - Returns the number of decoded data bytes used through used_decoded_size.
 * - Returns the number of decoded variable-data declarations used through
 *   used_decoded_variable_num.
 * - Verifies that the decoded payload size matches the payload size specified
 *   in the Application header.
 * - Verifies that the end-of-payload flag is present at the expected location.
 *
 * @param[in] context
 *        Pointer to the Application context.
 * @param[in] encoded_message
 *        Pointer to the encoded Application message.
 * @param[in] max_encoded_message_size
 *        Maximum number of bytes available in encoded_message.
 * @param[out] out_message
 *        Pointer to the structure in which the decoded message is stored.
 * @param[out] decoded_data
 *        Caller-provided storage for decoded variable-length data.
 * @param[in] max_decoded_data_size
 *        Maximum capacity of decoded_data in bytes.
 * @param[out] used_decoded_size
 *        Pointer to receive the number of decoded_data bytes used.
 * @param[out] decoded_peripherals
 *        Caller-provided storage for decoded peripheral configuration records.
 * @param[in] max_decoded_peripherals_num
 *        Maximum number of peripheral configuration records that can be
 *        stored in decoded_peripherals.
 * @param[out] decoded_variable_data
 *        Caller-provided storage for decoded variable-data declarations.
 * @param[in] max_decoded_variable_data_num
 *        Maximum number of variable-data declarations that can be stored in
 *        decoded_variable_data.
 * @param[out] used_decoded_variable_num
 *        Pointer to receive the number of decoded variable-data declarations
 *        used.
 *
 * @return HIL_APPLICATION_STATUS_OK
 *         The Application message was successfully decoded.
 * @return HIL_APPLICATION_STATUS_INVALID_ARGUMENT
 *         A required context, encoded-message, or output-message pointer is
 *         NULL.
 * @return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE
 *         The message type is invalid or unsupported.
 * @return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED
 *         The message type is reserved and not implemented.
 * @return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE
 *         The decoded payload size does not match the header or the
 *         end-of-payload flag is missing or invalid.
 * @return Other HIL_Application_Status_T values
 *         An error returned by the message-specific decoder.
 */
HIL_Application_Status_T HIL_APPLICATION_Decode_Message(
    const HIL_Application_Context_T* context, const uint8_t* encoded_message,
    size_t max_encoded_message_size, HIL_Application_Message_T* out_message, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size,
    HIL_Application_Peripheral_Config_T* decoded_peripherals, size_t max_decoded_peripherals_num,
    HIL_Application_Data_Declaration_T* decoded_variable_data, size_t max_decoded_variable_data_num,
    size_t* used_decoded_variable_num )
{

    if ( encoded_message == NULL || out_message == NULL || context == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    size_t                   expected_payload_size = 0;
    size_t                   actual_payload_size   = 0;
    HIL_Application_Status_T tracker               = HIL_APPLICATION_Header_decoding(
        context, out_message, encoded_message, &expected_payload_size );
    if ( tracker != HIL_APPLICATION_STATUS_OK )
    {
        return tracker;
    };
    const uint8_t* payload =
        &( encoded_message[HIL_APPLICATION_HEADER_SIZE_BYTES] );  // create pointer for the payload
    switch ( out_message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            tracker = HIL_APPLICATION_System_Info_Request_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.system_info_request ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            tracker = HIL_APPLICATION_System_Info_Response_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.system_info_response ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            tracker = HIL_APPLICATION_Test_Configuration_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.test_configuration ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_peripherals, max_decoded_peripherals_num,
                decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            tracker = HIL_APPLICATION_Test_Instructions_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.test_instruction ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size,
                decoded_variable_data, max_decoded_variable_data_num, used_decoded_variable_num );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            tracker = HIL_APPLICATION_Variable_Instruction_Data_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.variable_instruction_data ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            tracker = HIL_APPLICATION_Execution_Control_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.execution_control ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            tracker = HIL_APPLICATION_Global_Control_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.global_control ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            tracker = HIL_APPLICATION_Test_Result_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.test_result ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size,
                decoded_variable_data, max_decoded_variable_data_num, used_decoded_variable_num );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            tracker = HIL_APPLICATION_Variable_Result_Data_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.variable_result_data ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            tracker = HIL_APPLICATION_Response_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.response ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            tracker = HIL_APPLICATION_Error_decode(
                context, &( out_message->subtype ), out_message->test_id,
                &( out_message->body.error ), payload, max_encoded_message_size,
                &actual_payload_size, decoded_data, max_decoded_data_size, used_decoded_size );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;

        default:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
    }
    // Check the actual vs the expected payload size and the end payload flag
    if ( actual_payload_size != expected_payload_size
         || payload[actual_payload_size + HIL_APPLICATION_HEADER_SIZE_BYTES] != 1U )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }

    return HIL_APPLICATION_STATUS_OK;
}

/**
 * @brief Validate a typed Application message.
 *
 * - Validates the supplied Application message according to its message type.
 * - Selects the appropriate message-specific validation function.
 * - Validates the contents of the message body using the corresponding
 *   message-specific validator.
 * - Does not perform cross-message transaction validation.
 * - Does not perform Transport-layer validation or behaviour.
 *
 * @param[in] context
 *        Pointer to the Application context.
 * @param[in] message
 *        Pointer to the Application message to validate.
 *
 * @return HIL_APPLICATION_STATUS_OK
 *         The message is valid.
 * @return HIL_APPLICATION_STATUS_INVALID_ARGUMENT
 *         The context or message pointer is NULL.
 * @return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE
 *         The message type is invalid or unsupported.
 * @return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED
 *         The message type is reserved and not implemented.
 * @return HIL_APPLICATION_STATUS_INTERNAL_ERROR
 *         The message type does not currently support validation.
 * @return Other HIL_Application_Status_T values
 *         An error returned by the message-specific validation function.
 */
HIL_Application_Status_T
HIL_APPLICATION_Validate_Message( const HIL_Application_Context_T* context,
                                  const HIL_Application_Message_T* message )
{
    /*
     * TODO: Perform typed codec validation only: initialized bounds,
     * type/subtype/test-ID rules (Global Control absent, test controls present),
     * active union member, enum validity (Execution Control supports only START
     * and ABORT, with no ARM or FINALIZE_TEST command), every fixed signal array
     * element, variable pointer/count combinations, nonzero declaration/data
     * lengths, unique (peripheral, channel) pairs within declaration and
     * configuration arrays, channel-family consistency, nonzero
     * expected_tick_count, zero reserved flags, empty Test Configuration
     * extension_data, duty/range/count constraints, and checked size arithmetic.
     * Return UNSUPPORTED_MESSAGE for reserved flags or extension data. Do not
     * compare tick_number with an active Test Configuration, match variable
     * messages across calls, enforce endpoint direction, serialize outstanding
     * operations, or enforce increasing stop-and-wait upload order.
     * Integration owns those checks, rejects duplicate variable messages, and
     * invalidates rejected ticks. Integration accepts each complete tick only
     * after taking responsibility for retaining its fixed and declared variable
     * data, automatically performs whole-test validation after all N ticks and
     * data arrive, and makes exactly N fixed results available after a
     * successfully started test. Firmware integration sends fixed results in
     * increasing tick order, each followed by its variable results in
     * declaration order. Early execution failure uses EXECUTION_PROBLEM for
     * remaining fixed results unless communication/reset prevents delivery.
     * Result-condition selection and cross-message result ordering are
     * integration-owned enforcement of the shared transaction contract, not
     * single-message structural validation. Do not evaluate retention
     * medium/queue policy,
     * firmware electrical/hardware/execution-manager policy, execute
     * recovery/control, mutate anything, or retain a pointer.
     */

    HIL_Application_Status_T tracker;
    if ( context == NULL || message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            tracker = HIL_APPLICATION_System_Info_Request_validate(
                context, &( message->body.system_info_request ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            tracker = HIL_APPLICATION_System_Info_Response_validate(
                context, &( message->body.system_info_response ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            tracker = HIL_APPLICATION_Test_Configuration_validate(
                context, &( message->body.test_configuration ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            tracker = HIL_APPLICATION_Test_Instructions_validate(
                context, &( message->body.test_instruction ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            tracker = HIL_APPLICATION_Variable_Instruction_Data_validate(
                context, &( message->body.variable_instruction_data ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            tracker = HIL_APPLICATION_Execution_Control_validate(
                context, &( message->body.execution_control ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            tracker = HIL_APPLICATION_Global_Control_validate( context,
                                                               &( message->body.global_control ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            tracker =
                HIL_APPLICATION_Test_Result_validate( context, &( message->body.test_result ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            tracker = HIL_APPLICATION_Variable_Result_Data_validate(
                context, &( message->body.variable_result_data ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            tracker = HIL_APPLICATION_Response_validate( context, &( message->body.response ) );
            if ( tracker != HIL_APPLICATION_STATUS_OK )
            {
                return tracker;
            };
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_STATUS_INTERNAL_ERROR;

        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;

        default:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
    }

    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Validate_Encoded_Message(
    const HIL_Application_Context_T* context, const uint8_t* encoded_message,
    size_t encoded_message_size, size_t* required_decode_storage )
{
    /*
     * TODO: Safely parse one complete message without publishing typed data;
     * validate the future envelope and accept only the compiled-in Application
     * protocol version, returning UNSUPPORTED_MESSAGE for an incompatible
     * version; validate type/subtype/test-ID presence including Global Control
     * rules, exact fixed-array and body lengths,
     * enum/count/channel/declaration relationships including nonzero and unique
     * declarations/configurations, initial zero flags/empty extension data, and
     * configured bounds; reject missing and trailing bytes; calculate variable
     * decode storage using checked arithmetic and the public alignment contract;
     * and publish it only for a fully valid message.
     * Do not infer active-configuration tick range/order, transaction
     * prerequisites/completion/invalidation, or reset/result behavior.
     * Transport session events are reported to integration and never mutate an
     * Application transaction here. Do not mutate context, consume or retain
     * input, or perform integration-semantic, hardware, or Transport behavior.
     */
    if ( required_decode_storage == NULL || context == NULL || encoded_message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    ( void )context;
    ( void )encoded_message;
    ( void )encoded_message_size;

    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}
