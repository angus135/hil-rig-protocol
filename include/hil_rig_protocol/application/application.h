/**
 * @file application.h
 * @brief Normal caller-facing HIL-RIG Application Layer C API.
 *
 * @details This umbrella header defines the stateless shared message-codec
 * boundary used by firmware and Python bindings. The future implementation
 * converts typed data to one complete Application message and reverses that
 * conversion after Transport has reassembled one complete message.
 *
 * The surrounding Application protocol specification defines message
 * direction, correlation, exchange order, Response meaning, transaction
 * creation/completion/invalidation, and permitted follow-up actions. Firmware
 * and host integration implement that contract outside this codec. The codec
 * performs no semantic acceptance and tracks no transaction. It is
 * direction-neutral: endpoint integration, not encoding or decoding, enforces
 * whether Python or firmware may send a structurally valid message family.
 *
 * The initial codec has one version named by
 * HIL_APPLICATION_PROTOCOL_VERSION_MAJOR and
 * HIL_APPLICATION_PROTOCOL_VERSION_MINOR. Encoding always produces that
 * compiled-in version and decoding accepts only it. Callers cannot select or
 * negotiate another encoding version through config, context, messages, or
 * per-call arguments. The eventual common envelope must identify the version,
 * but its complete wire layout remains undefined.
 *
 * Application never performs COBS framing, CRC, Transport sequencing,
 * acknowledgements, retransmission, fragmentation/reassembly, advertised-window
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

// TO DO, select reasonalbe max bounds for these values
#define HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE ( 10000u )
#define HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT ( 100000u )
#define HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK ( 10000u )
#define HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT ( 10000u )

/**
 * @brief Populate safe, policy-disabled Application configuration values.
 *
 * @details A future implementation must initialize every field
 * deterministically without choosing production codec capacities. Zero values
 * require integration to select explicit capacities. This function allocates
 * nothing and mutates no context.
 *
 * @param[out] config Configuration object to clear/populate; must not be NULL.
 *
 * @retval HIL_APPLICATION_STATUS_OK Configuration initialized.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT config is NULL.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 *
 * @post On future OK, all fields are initialized. The current stub defensively
 * zeroes config when non-NULL and returns NOT_IMPLEMENTED.
 */
HIL_Application_Status_T HIL_APPLICATION_Default_Config( HIL_Application_Config_T* config );
/**
 * @brief Initialize a lightweight context with copied structural codec limits.
 *
 * @details A future implementation validates every configured limit and their
 * relationships, then copies the complete configuration and sets initialized
 * only after successful validation. It performs checked reasoning for values
 * later used in size arithmetic but allocates no memory and retains no pointer
 * from config.
 *
 * The context remains logically read-only after initialization. Sizing,
 * validation, encoding, and decoding inspect only copied bounds. They do not
 * store messages, output bytes, decode storage, an active Test ID, upload/tick
 * progress, result-transfer progress, retention ownership, execution state, or
 * statistics.
 *
 * config is borrowed only for this call and may be released immediately after
 * future success.
 *
 * @param[out] context Caller-allocated codec context to initialize.
 * @param[in] config Explicit structural codec limits; must not be NULL.
 *
 * @retval HIL_APPLICATION_STATUS_OK Configuration validated and copied.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT Invalid pointer or limit.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 *
 * @warning All later operations for context must be made by its single owning
 * execution context. Calls must not be distributed across tasks, callbacks, or
 * interrupts even with external locking. The library is not thread-safe or
 * re-entrant and adds no locks, atomics, callbacks, or RTOS dependencies.
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
 * A future envelope includes the compiled-in Application protocol version.
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
 * @brief Encode typed data into one complete Application message.
 *
 * @details The future implementation validates the tagged message, calculates
 * size with checked arithmetic, explicitly serializes approved fixed-width
 * fields, serializes all fixed tick-array elements in deterministic channel
 * order, writes the compiled-in Application protocol version into the future
 * envelope, copies every variable array/span, rejects inconsistent
 * declarations, and publishes output_size only after complete success. It never
 * performs a Global Control operation, Transport framing/fragmentation, or
 * pointer retention. No API parameter selects another encoding version.
 *
 * On OK output_size is the bytes copied. On BUFFER_TOO_SMALL it is the required
 * size and out_buffer remains unusable as a message. On other failures and
 * NOT_IMPLEMENTED it is zero. NULL out_buffer with size zero is a nonmutating
 * size query and returns BUFFER_TOO_SMALL with the required nonzero size.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] message Complete tagged typed message borrowed synchronously.
 * @param[out] out_buffer Complete-message destination; NULL only when
 *             out_buffer_size is zero.
 * @param[in] out_buffer_size Writable destination capacity.
 * @param[out] output_size Copied, required, or zero byte count per result.
 *
 * @retval HIL_APPLICATION_STATUS_OK Complete message encoded.
 * @retval HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL Destination/query too small.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT Invalid pointer combination.
 * @retval HIL_APPLICATION_STATUS_UNINITIALIZED Invalid context.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 * @return Other structural statuses documented by HIL_APPLICATION_Encoded_Size.
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
 * and body without publishing typed output, accepts only the compiled-in
 * Application protocol version, rejects missing/trailing bytes, and totals
 * storage required for every decoded array and byte span using checked
 * arithmetic. The reported value is the required usable byte capacity assuming
 * storage begins at HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT. The query never
 * mutates context or input.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] encoded_message Complete reassembled Application bytes; NULL only
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
 * @brief Decode one complete Application message into typed caller storage.
 *
 * @details Transport must supply exactly one complete reassembled Application
 * message. The future implementation validates envelope/type/subtype/test-ID
 * presence, compiled-in Application protocol version, and exact length;
 * calculates/reserves caller decode storage; copies variable arrays and byte
 * spans; decodes fixed tick arrays directly into the typed output; decodes the
 * selected union body; rejects trailing or missing bytes; and publishes
 * out_message only after complete success.
 *
 * Variable pointers in out_message point into decode_storage and remain valid
 * until the caller modifies/releases that region. No pointer references
 * encoded_message, so the Transport receive item may be released after return.
 * Context retains neither input nor decode storage.
 *
 * On OK decode_storage_size is bytes used and out_message is fully initialized.
 * On BUFFER_TOO_SMALL it reports required bytes and out_message is INVALID. On
 * all other failure and NOT_IMPLEMENTED it is zero and out_message is INVALID.
 * NULL storage with zero capacity is a size query when required storage is
 * nonzero. A zero-storage fixed message may decode with that combination.
 * Every non-NULL decode_storage pointer must be aligned to
 * HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT. A future implementation returns
 * HIL_APPLICATION_STATUS_INVALID_ARGUMENT for a misaligned pointer; the current
 * intentional stub performs no runtime alignment check.
 *
 * Decoding RESET_APPLICATION never performs recovery or affects Transport.
 * Decoding also never creates, completes, or invalidates a transaction;
 * semantically accepts or retains a test; performs control; queries firmware
 * modules; or creates a Response.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] encoded_message Complete reassembled bytes, borrowed for this call.
 * @param[in] encoded_message_size Exact complete-message bytes.
 * @param[out] decode_storage Caller workspace for variable arrays/spans; NULL
 *             only when decode_storage_capacity is zero. A non-NULL pointer
 *             must satisfy HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT.
 * @param[in] decode_storage_capacity Writable workspace capacity.
 * @param[out] out_message Typed output; set to INVALID on failure.
 * @param[out] decode_storage_size Used, required, or zero bytes per result.
 *
 * @retval HIL_APPLICATION_STATUS_OK Message fully decoded.
 * @retval HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL Decode workspace insufficient.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT Invalid pointer combination.
 * @retval HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE Version is incompatible.
 * @retval HIL_APPLICATION_STATUS_UNINITIALIZED Invalid context.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 * @return Other codec statuses documented by Decode_Storage_Size.
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
HIL_Application_Status_T HIL_APPLICATION_Decode_Message( const HIL_Application_Context_T* context,
                                                         const uint8_t* encoded_message,
                                                         size_t         encoded_message_size,
                                                         HIL_Application_Message_T* out_message);

/**
 * @brief Structurally validate one typed tagged message.
 *
 * @details Future validation covers type/subtype/test-ID presence, enum values,
 * variable pointer/count pairs, configured bounds, fixed-array element values,
 * channel-family consistency, nonzero and unique variable declarations, unique
 * peripheral/channel configuration records, nonzero expected_tick_count, zero
 * reserved flags, empty unsupported extension data, and checked size
 * relationships. Nonzero reserved flags or nonempty Test Configuration
 * extension_data return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE. Validation
 * enforces that Global Control forbids a Test ID and that test-scoped messages
 * require one. It does not check sender direction,
 * tick_number against an active Test Configuration, transaction prerequisites,
 * hardware support/electrical limits, retention availability, upload
 * completion, execution-manager permission, or whole-test consistency.
 *
 * @param[in] context Successfully initialized context.
 * @param[in] message Typed message borrowed synchronously.
 *
 * @retval HIL_APPLICATION_STATUS_OK Typed structure is codec-valid.
 * @retval HIL_APPLICATION_STATUS_UNINITIALIZED Invalid context.
 * @retval HIL_APPLICATION_STATUS_INVALID_ARGUMENT message is NULL.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Current intentional stub.
 * @return A specific message, subtype, length, count, or unsupported status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Validate_Message( const HIL_Application_Context_T* context,
                                  const HIL_Application_Message_T* message );

/**
 * @brief Structurally validate one complete encoded Application message.
 *
 * @details Future validation safely parses without publishing typed output,
 * accepts only the compiled-in Application protocol version, checks exact
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
