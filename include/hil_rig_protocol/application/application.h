/**
 * @file application.h
 * @brief Normal caller-facing HIL-RIG Application Layer C API.
 *
 * @details This umbrella header defines the stateless shared message-codec
 * boundary used by firmware and Python bindings. The future implementation
 * converts typed data to one complete Application message and reverses that
 * conversion after the MVP Transport returns one complete message from one frame.
 *
 * The surrounding Application protocol specification defines message
 * direction, correlation, exchange order, Response meaning, transaction
 * creation/completion/invalidation, and permitted follow-up actions. Firmware
 * and host integration implement that contract outside this codec. The codec
 * performs no semantic acceptance and tracks no transaction. It is
 * direction-neutral: endpoint integration, not encoding or decoding, enforces
 * whether Python or firmware may send a structurally valid message family.
 *
 * The public constants
 * HIL_APPLICATION_PROTOCOL_VERSION_MAJOR and
 * HIL_APPLICATION_PROTOCOL_VERSION_MINOR reserve Application version 1.0. A
 * future codec will encode and validate that version. The current
 * NOT_IMPLEMENTED encoder and decoder perform no version handling. Callers
 * cannot select or negotiate another encoding version through config, context,
 * messages, or per-call arguments. The eventual common envelope must identify
 * the version, but its complete wire layout remains undefined.
 *
 * Application never performs COBS framing, CRC, Transport sequencing,
 * acknowledgements, retransmission, Transport framing, advertised-window
 * management, communication I/O, RTOS work, hardware access, test execution,
 * hardware scheduling, or Python UI behavior. It has no Transport-header
 * dependency. A Transport ACK confirms delivery only; semantic acceptance uses
 * HIL_APPLICATION_MESSAGE_TYPE_RESPONSE.
 *
 * @note This API/source-structure PR contains intentional NOT_IMPLEMENTED
 * stubs. Application wire encoding, decoding, and structural validation are not
 * implemented. Semantic acceptance, transaction bookkeeping, test retention,
 * execution-manager decisions, hardware behavior, and Python client progress
 * belong to endpoint integration. Firmware remains authoritative for its own
 * internal states and transitions; this API defines no firmware or protocol
 * state machine.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

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
HIL_Application_Status_T HIL_APPLICATION_Default_Config( HIL_Application_Config_T* config );

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
                                               const HIL_Application_Config_T* config );

/**
 * @brief Calculate exact encoded size for one tagged typed message.
 *
 * @details The future implementation performs codec-level structural
 * validation, checks message/subtype/test-ID presence rules (including
 * test-independent Global Control), validates every variable pointer/count and
 * declared length, rejects zero/duplicate declarations and duplicate
 * peripheral/channel configuration records, requires nonzero tick count and
 * zero reserved flags/empty extension data, includes every element of each
 * fixed tick array, and adds explicit future envelope/body fields using checked
 * arithmetic. It does not mutate context or message and retains no pointer.
 *
 * A future envelope includes the reserved Application protocol version.
 * Exact fields, widths, byte order, and layout remain TODO; callers cannot
 * select an alternate version.
 *
 * @param[in] context Successfully initialized context providing local bounds.
 * @param[in] message Complete typed message borrowed for this call.
 * @param[out] encoded_size Exact required output bytes on OK; zero on failure.
 *
 * @retval HIL_APPLICATION_STATUS_OK Required size calculated.
 * @retval HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE Invalid/reserved tag.
 * @retval HIL_APPLICATION_STATUS_INVALID_SUBTYPE Type/subtype mismatch.
 * @retval HIL_APPLICATION_STATUS_INVALID_LENGTH Invalid span or declaration.
 * @retval HIL_APPLICATION_STATUS_INVALID_COUNT Count violates local bounds.
 * @retval HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE Valid but unsupported data.
 * @retval HIL_APPLICATION_STATUS_UNINITIALIZED Invalid context.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 */
HIL_Application_Status_T HIL_APPLICATION_Encoded_Size( const HIL_Application_Context_T* context,
                                                       const HIL_Application_Message_T* message,
                                                       size_t* encoded_size );

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
                                                         size_t*  output_size );

/**
 * @brief Calculate variable storage required to decode one complete message.
 *
 * @details The future implementation validates the complete encoded envelope
 * and body without publishing typed output, accepts only the reserved
 * Application protocol version, rejects missing/trailing bytes, and totals
 * storage required for every decoded array and byte span using checked
 * arithmetic. The reported value is the required usable byte capacity assuming
 * storage begins at HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT. The query never
 * mutates context or input.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] encoded_message One complete Application message; NULL only
 *            when encoded_message_size is zero.
 * @param[in] encoded_message_size Exact complete-message byte count.
 * @param[out] required_storage_size Required decode bytes on OK; zero on error.
 *
 * @retval HIL_APPLICATION_STATUS_OK Required storage calculated.
 * @retval HIL_APPLICATION_STATUS_MALFORMED_MESSAGE Invalid envelope/body.
 * @retval HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE Declared bytes are missing.
 * @retval HIL_APPLICATION_STATUS_INVALID_LENGTH Length arithmetic inconsistent.
 * @retval HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE Version is incompatible.
 * @retval HIL_APPLICATION_STATUS_UNINITIALIZED Invalid context.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 */
HIL_Application_Status_T
HIL_APPLICATION_Decode_Storage_Size( const HIL_Application_Context_T* context,
                                     const uint8_t* encoded_message, size_t encoded_message_size,
                                     size_t* required_storage_size );

/**
 * @brief Decode one complete encoded Application message into typed caller storage.
 *
 * @details The MVP Transport supplies exactly one complete Application message
 * from one frame. The future implementation validates envelope/type/subtype/test-ID
 * presence, reserved Application protocol version, and exact length;
 * calculates/reserves caller decode storage; copies variable arrays and byte
 * spans; decodes fixed tick arrays directly into the typed output; decodes the
 * selected union body; rejects trailing or missing bytes; and publishes
 * out_message only after complete success.
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
 * Decoding RESET_APPLICATION never performs recovery or affects Transport.
 * Decoding also never creates, completes, or invalidates a transaction;
 * semantically accepts or retains a test; performs control; queries firmware
 * modules; or creates a Response.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] encoded_message Complete Application bytes, borrowed for this call.
 * @param[in] encoded_message_size Exact complete-message bytes.
 * @param[out] decode_storage Caller workspace for variable arrays/spans; NULL
 *             only when decode_storage_capacity is zero. A non-NULL pointer
 *             must satisfy HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT.
 * @param[in] decode_storage_capacity Writable workspace capacity.
 * @param[out] out_message Typed output; set to INVALID on failure.
 * @param[out] decode_storage_size Used, required, or zero bytes per result.
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
 *
 * @par C11 static storage
 * @code{.c}
 * _Alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT )
 * static uint8_t decode_storage[2048u];
 * @endcode
 *
 * @par C++ static storage
 * @code{.cpp}
 * alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT )
 * static uint8_t decode_storage[2048u];
 * @endcode
 */
HIL_Application_Status_T
HIL_APPLICATION_Decode_Message( const HIL_Application_Context_T* context,
                                const uint8_t* encoded_message, size_t encoded_message_size,
                                HIL_Application_Message_T* out_message, uint8_t* decoded_data,
                                size_t max_decoded_data_size, size_t* used_decoded_size );

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
                                  const HIL_Application_Message_T* message );

/**
 * @brief Structurally validate one complete encoded Application message.
 *
 * @details Future validation safely parses without publishing typed output,
 * accepts only the reserved Application protocol version, checks exact
 * encoded length and all internal declarations, rejects trailing or missing
 * bytes, and reports decode storage required for valid content. It does not
 * mutate context, consume Transport data, enforce sender/transaction policy, or
 * perform integration-workflow or hardware semantic validation.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] encoded_message Complete message bytes; NULL only when size is zero.
 * @param[in] encoded_message_size Exact available bytes.
 * @param[out] required_decode_storage Decode bytes required on OK; zero on error.
 *
 * @retval HIL_APPLICATION_STATUS_OK Encoded message is structurally valid.
 * @retval HIL_APPLICATION_STATUS_UNINITIALIZED Invalid context.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT Invalid pointer combination.
 * @retval HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE Version is incompatible.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 * @return A specific malformed, truncated, type, subtype, length, or count status.
 */
HIL_Application_Status_T HIL_APPLICATION_Validate_Encoded_Message(
    const HIL_Application_Context_T* context, const uint8_t* encoded_message,
    size_t encoded_message_size, size_t* required_decode_storage );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_H */
