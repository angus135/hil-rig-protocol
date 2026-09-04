/**
 * @file application.h
 * @brief Stateless HIL-RIG Application message codec public API.
 *
 * @details The codec converts between HIL_Application_Message_T and exactly one
 * complete Application wire message. It performs no I/O, Transport behaviour,
 * firmware hardware actions, transaction tracking, heap allocation, or caller-
 * pointer retention. Local HIL_Application_Status_T values are API outcomes and
 * are distinct from on-wire Application Responses and Transport acknowledgements.
 *
 * Every complete encoded message is a 23-byte common envelope followed by its
 * declared payload. The envelope contains repository-wide protocol major/minor
 * bytes, a Test-ID-present flag, a 16-byte Test ID, one-byte type/subtype values,
 * and a little-endian uint16_t payload length. There is no payload-end marker.
 *
 * @see docs/application_layer/application_wire_format.md
 * @see docs/application_layer/application_layer.md
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
 * @brief Populate the default stateless codec configuration.
 *
 * @details The default complete-message limit is
 * HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE (512 bytes). The returned
 * configuration is accepted by HIL_APPLICATION_Init().
 *
 * @param[out] config Configuration to populate.
 *
 * @retval HIL_APPLICATION_STATUS_OK Configuration populated successfully.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT config is NULL.
 */
HIL_Application_Status_T HIL_APPLICATION_Default_Config( HIL_Application_Config_T* config );

/**
 * @brief Initialize an Application codec context from caller configuration.
 *
 * @details max_encoded_message_size is an operational upper bound, not a
 * requirement to provision the theoretical maximum. A usable configuration
 * permits at least HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE bytes and no more
 * than HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE bytes. The context is marked
 * initialized only after every check succeeds. Any failure leaves it
 * deterministically uninitialized with cleared copied configuration.
 *
 * @param[out] context Context to initialize.
 * @param[in]  config  Structural codec limits to copy.
 *
 * @retval HIL_APPLICATION_STATUS_OK Context initialized successfully.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT context or config is NULL.
 * @retval HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL The configured complete-message limit cannot hold
 *         the smallest supported complete message.
 * @retval HIL_APPLICATION_STATUS_INVALID_LENGTH A configured length exceeds an absolute wire limit.
 * @retval HIL_APPLICATION_STATUS_INVALID_COUNT A configured count exceeds an absolute codec limit.
 */
HIL_Application_Status_T HIL_APPLICATION_Init( HIL_Application_Context_T*      context,
                                               const HIL_Application_Config_T* config );

/**
 * @brief Calculate the exact complete encoded size for one typed message.
 *
 * @details The result includes the fixed 23-byte envelope. The function clears
 * encoded_size before validation. Message families whose body-size calculation
 * is deliberately unfinished return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED.
 *
 * @param[in]  context      Initialized codec context.
 * @param[in]  message      Typed message to size.
 * @param[out] encoded_size Exact complete-message size on success; zero on failure.
 *
 * @retval HIL_APPLICATION_STATUS_OK Size calculated successfully.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Selected body sizing is deliberately unfinished.
 * @return Otherwise, the most specific argument, context, validation, length,
 *         or message-type error.
 */
HIL_Application_Status_T HIL_APPLICATION_Encoded_Size( const HIL_Application_Context_T* context,
                                                       const HIL_Application_Message_T* message,
                                                       size_t* encoded_size );

/**
 * @brief Encode one complete Application message.
 *
 * @details output_size is set to zero before any validation or write. On
 * success it is exactly the 23-byte envelope plus encoded payload. On failure
 * output_size remains zero and out_buffer contents are unspecified; callers
 * must use output_size as the publication indicator. The codec does not clear
 * the entire buffer, allocate memory, or retain any pointer.
 *
 * The usable output extent is bounded by both out_buffer_size and the
 * initialized context's max_encoded_message_size.
 *
 * @param[in]  context         Initialized codec context.
 * @param[in]  message         Typed message to encode.
 * @param[out] out_buffer      Caller-owned destination buffer.
 * @param[in]  out_buffer_size Available bytes in out_buffer.
 * @param[out] output_size     Complete encoded size on success; zero on failure.
 *
 * @retval HIL_APPLICATION_STATUS_OK Complete message encoded successfully.
 * @retval HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL Caller output capacity is insufficient.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Selected body encoder is deliberately unfinished.
 * @return Otherwise, the most specific argument, context, validation, length,
 *         or message-type error.
 */
HIL_Application_Status_T HIL_APPLICATION_Encode_Message( const HIL_Application_Context_T* context,
                                                         const HIL_Application_Message_T* message,
                                                         uint8_t* out_buffer,
                                                         size_t   out_buffer_size,
                                                         size_t*  output_size );

/**
 * @brief Calculate caller storage required to decode one complete message.
 *
 * @details encoded_message_size is the actual size of one complete encoded
 * message, not buffer capacity. The same bounded common-envelope parser used by
 * normal decoding classifies the body. Supported fixed-size messages require
 * an exact family-specific payload width and zero additional storage; malformed
 * undersized or oversized fixed bodies return MALFORMED_MESSAGE. Variable-storage
 * families whose sizing remains unfinished return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED.
 *
 * This operation uses a lightweight private envelope object rather than a full
 * HIL_Application_Message_T merely to classify the family.
 *
 * @param[in]  context               Initialized codec context.
 * @param[in]  encoded_message       Complete encoded message bytes.
 * @param[in]  encoded_message_size  Actual number of bytes in encoded_message.
 * @param[out] required_storage_size Additional decode-storage bytes required; zero on failure.
 *
 * @retval HIL_APPLICATION_STATUS_OK Storage requirement calculated successfully.
 * @retval HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE Input ends before its declared complete length.
 * @retval HIL_APPLICATION_STATUS_MALFORMED_MESSAGE Input contains trailing or malformed body bytes.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Storage sizing for the selected family is
 *         unfinished.
 * @return Otherwise, the most specific argument, context, version, type, subtype, or length error.
 */
HIL_Application_Status_T
HIL_APPLICATION_Decode_Storage_Size( const HIL_Application_Context_T* context,
                                     const uint8_t* encoded_message, size_t encoded_message_size,
                                     size_t* required_storage_size );

/**
 * @brief Decode exactly one complete encoded Application message.
 *
 * @details encoded_message_size is the actual input length, not capacity. The
 * input must equal 23 + the envelope's little-endian uint16_t payload length.
 * Input shorter than that declared total is TRUNCATED_MESSAGE; trailing bytes
 * or an internally malformed body are MALFORMED_MESSAGE. Body decoders receive
 * exactly the declared payload extent and must consume it exactly.
 *
 * used_decoded_size is set to zero and out_message->type is set to
 * HIL_APPLICATION_MESSAGE_TYPE_INVALID before parsing. Both publication guards
 * remain in that state on every failure. Other out_message fields and
 * decode_data contents are unspecified after failure. On successful variable-
 * data decoding, nested spans point into caller-owned decoded_data.
 *
 * @param[in]  context                Initialized codec context.
 * @param[in]  encoded_message        Complete encoded message bytes.
 * @param[in]  encoded_message_size   Actual number of bytes in encoded_message.
 * @param[out] out_message            Typed decoded message.
 * @param[out] decoded_data           Caller-owned storage for decoded byte spans, or NULL when zero
 *                                    storage is required.
 * @param[in]  max_decoded_data_size  Available bytes in decoded_data.
 * @param[out] used_decoded_size      Bytes consumed from decoded_data; zero on failure.
 *
 * @retval HIL_APPLICATION_STATUS_OK Message decoded and structurally validated.
 * @retval HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE Input is shorter than the envelope-declared
 *         total.
 * @retval HIL_APPLICATION_STATUS_MALFORMED_MESSAGE Input has trailing bytes or malformed body
 *         syntax.
 * @retval HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL Caller-provided decoded-data storage is
 *         insufficient.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Selected body decoder/validator is unfinished.
 * @return Otherwise, the most specific argument, context, version, type, subtype,
 *         or validation error.
 */
HIL_Application_Status_T
HIL_APPLICATION_Decode_Message( const HIL_Application_Context_T* context,
                                const uint8_t* encoded_message, size_t encoded_message_size,
                                HIL_Application_Message_T* out_message, uint8_t* decoded_data,
                                size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Validate a typed Application message without encoding it.
 *
 * @details Common validation covers context state, defined type/subtype,
 * boolean Test-ID presence, Test-ID structural rules, and existing family-
 * specific structural validation. It deliberately does not enforce stateful
 * transactions, firmware capabilities, or deferred detailed fixed-I/O rules.
 *
 * @param[in] context Initialized codec context.
 * @param[in] message Typed message to validate.
 *
 * @retval HIL_APPLICATION_STATUS_OK Message satisfies currently implemented structural rules.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Selected family's validation remains unfinished.
 * @return Otherwise, the most specific argument, context, type, subtype, Test-ID,
 *         or validation error.
 */
HIL_Application_Status_T
HIL_APPLICATION_Validate_Message( const HIL_Application_Context_T* context,
                                  const HIL_Application_Message_T* message );

/**
 * @brief Structurally validate exactly one complete encoded Application message.
 *
 * @details This reuses the bounded normal decode path rather than defining a
 * second wire parser. required_decode_storage is cleared before work and is
 * zero on failure. No caller storage is allocated. The current implementation
 * uses one temporary HIL_Application_Message_T on the stack for body decoding
 * and typed validation; this is intentionally separate from the lightweight
 * envelope-only path used by HIL_APPLICATION_Decode_Storage_Size().
 *
 * @param[in]  context                 Initialized codec context.
 * @param[in]  encoded_message         Complete encoded message bytes.
 * @param[in]  encoded_message_size    Actual number of bytes in encoded_message.
 * @param[out] required_decode_storage Decode-storage requirement on success; zero on failure.
 *
 * @retval HIL_APPLICATION_STATUS_OK Encoded message is structurally valid.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Selected body/storage validation is unfinished.
 * @return Otherwise, the same structural/status classification used by normal decoding.
 */
HIL_Application_Status_T HIL_APPLICATION_Validate_Encoded_Message(
    const HIL_Application_Context_T* context, const uint8_t* encoded_message,
    size_t encoded_message_size, size_t* required_decode_storage );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_H */
