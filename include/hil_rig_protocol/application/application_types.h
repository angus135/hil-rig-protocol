/**
 * @file application_types.h
 * @brief Common identifiers, spans, channel values, and context metadata.
 *
 * @details Types in this header are public C API representations, not packed
 * wire structures. A future codec must serialize each approved field with an
 * explicit fixed width and byte order. Native enum size, uint32_t, structure
 * padding, and pointers must never be copied directly into a message.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TYPES_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Number of opaque bytes in every HIL-RIG test identifier. */
#define HIL_APPLICATION_TEST_ID_SIZE ( 16u )

/** Compiled-in Application protocol major version produced and accepted. */
#define HIL_APPLICATION_PROTOCOL_VERSION_MAJOR ( 1u )

/** Compiled-in Application protocol minor version produced and accepted. */
#define HIL_APPLICATION_PROTOCOL_VERSION_MINOR ( 0u )

/**
 * @brief Minimum alignment required for caller-provided decode storage.
 *
 * @details The value is a C11/C++ constant expression and is sufficient for
 * every public typed object that the decoder may place in caller storage.
 * HIL_APPLICATION_Decode_Storage_Size() reports usable byte capacity assuming
 * the supplied storage begins at this alignment. The requirement is
 * intentionally independent of the future wire representation.
 */
#if defined( __cplusplus )
#define HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ( alignof( max_align_t ) )
#else
#define HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ( _Alignof( max_align_t ) )
#endif

/**
 * @name Fixed signal-channel counts
 *
 * @details These extents deliberately describe the current physical HIL-RIG
 * protocol profile. For every fixed signal array, index i maps to external
 * logical channel i. They do not expose MCU pins or peripheral instances.
 * Changing an extent changes every affected typed and future encoded message
 * and therefore requires protocol-version and compatibility review.
 * @{
 */
/** Number of physical HIL-RIG digital-output channels in every instruction. */
#define HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT ( 10u )

/** Number of physical HIL-RIG digital-input channels in every result. */
#define HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT ( 10u )

/** Number of physical HIL-RIG analogue-output channels in every instruction. */
#define HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT ( 6u )

/** Number of physical HIL-RIG analogue-input channels in every result. */
#define HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT ( 2u )

/** Number of physical HIL-RIG PWM-output channels in every instruction. */
#define HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT ( 2u )

/** Number of physical HIL-RIG PWM-input channels in every result. */
#define HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT ( 2u )
/** @} */

/**
 * @brief Opaque identifier shared by every message belonging to one test.
 *
 * @details The Application library carries and copies this byte array and
 * structurally validates whether an envelope requires it, but never generates
 * it or assigns arithmetic, ordering, or timestamp meaning to it. The codec may
 * carry any 16-byte value; all-zero bytes are structurally valid and message
 * envelopes use an explicit presence flag instead of a zero-value convention.
 *
 * The protocol integration contract separately requires the Python host to
 * generate a fresh random 128-bit value for every new upload, reuse it for all
 * messages in that test, and keep it independent of Transport session identity.
 * Firmware integration compares all 16 bytes.
 */
typedef struct
{
    /** Opaque bytes; compare every byte when testing identity. */
    uint8_t bytes[HIL_APPLICATION_TEST_ID_SIZE];
} HIL_Application_Test_Id_T;

/**
 * @brief Synchronously borrowed immutable byte span.
 *
 * @details data may be NULL only when size is zero. Message validation and
 * encoding borrow the bytes only for the duration of the call and never retain
 * the pointer. Successful decoding points spans into caller-owned decode
 * storage, never into the encoded Transport-reassembled input.
 */
typedef struct
{
    /** First byte, or NULL for an empty span. */
    const uint8_t* data;

    /** Number of readable bytes at data. */
    uint32_t size;
} HIL_Application_Byte_Span_T;

/**
 * @brief Protocol-level peripheral or signal family.
 *
 * @warning Assigned values may become wire identifiers. Changing a published
 * value can break compatibility and therefore requires protocol-version review.
 */
typedef enum
{
    /** Invalid sentinel; never valid in an encoded message. */
    HIL_APPLICATION_PERIPHERAL_INVALID = 0,
    /** Digital input signal. */
    HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT = 1,
    /** Digital output signal. */
    HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT = 2,
    /** Analogue input signal represented in protocol units. */
    HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT = 3,
    /** Analogue output signal represented in protocol units. */
    HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT = 4,
    /** PWM capture input. */
    HIL_APPLICATION_PERIPHERAL_PWM_INPUT = 5,
    /** PWM generation output. */
    HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT = 6,
    /** UART communication channel. */
    HIL_APPLICATION_PERIPHERAL_UART = 16,
    /** SPI communication channel. */
    HIL_APPLICATION_PERIPHERAL_SPI = 17,
    /** I2C communication channel. */
    HIL_APPLICATION_PERIPHERAL_I2C = 18,
    /** CAN communication channel. */
    HIL_APPLICATION_PERIPHERAL_CAN = 19,
    /** Reserved sentinel; not an operating peripheral identifier. */
    HIL_APPLICATION_PERIPHERAL_RESERVED = 255
} HIL_Application_Peripheral_Type_T;

/**
 * @brief Protocol-level channel identity independent of MCU mappings.
 *
 * @details channel is a logical identifier agreed by Application integration.
 * It is never a GPIO address, register number, DMA stream, or driver handle.
 */
typedef struct
{
    /** Signal or communication family containing the channel. */
    HIL_Application_Peripheral_Type_T peripheral;

    /** Logical channel number within that family. */
    uint16_t channel;
} HIL_Application_Channel_Id_T;

/**
 * @brief Tick duration expressed explicitly in nanoseconds.
 *
 * @details This represents configured protocol data only. It does not select a
 * timer, interrupt, scheduling strategy, or achievable hardware precision.
 */
typedef struct
{
    /** Requested duration of one tick; zero is structurally invalid. */
    uint32_t nanoseconds;
} HIL_Application_Tick_Duration_T;

/**
 * @brief State of one digital output at its fixed array index.
 *
 * @details Array index i identifies external HIL-RIG DIGITAL_OUTPUT channel i;
 * no channel field is encoded in this API value.
 */
typedef struct
{
    /** Zero for inactive/low; one for active/high; other values are invalid. */
    uint8_t high;
} HIL_Application_Digital_Output_Value_T;

/**
 * @brief Captured state of one digital input at its fixed array index.
 *
 * @details Array index i identifies external HIL-RIG DIGITAL_INPUT channel i.
 */
typedef struct
{
    /** Zero for inactive/low; one for active/high; other values are invalid. */
    uint8_t high;
} HIL_Application_Digital_Input_Value_T;

/**
 * @brief One analogue output value in unit-explicit microvolts.
 *
 * @details Array index i identifies external HIL-RIG ANALOG_OUTPUT channel i.
 * Hardware conversion, calibration, range support, and electrical safety are
 * semantic responsibilities of the firmware integration.
 */
typedef struct
{
    /** Requested signed output in microvolts. */
    int32_t microvolts;
} HIL_Application_Analog_Output_Value_T;

/**
 * @brief One captured analogue input value in unit-explicit microvolts.
 *
 * @details Array index i identifies external HIL-RIG ANALOG_INPUT channel i.
 */
typedef struct
{
    /** Captured signed input in microvolts. */
    int32_t microvolts;
} HIL_Application_Analog_Input_Value_T;

/**
 * @brief One PWM output setting for a fixed tick instruction.
 *
 * @details Array index i identifies external HIL-RIG PWM_OUTPUT channel i.
 * duty_cycle_permyriad uses 0..10000 for 0..100 percent. A zero period disables
 * the output for that tick; a nonzero period requests generation. Firmware
 * determines whether the requested period and duty are achievable.
 */
typedef struct
{
    /** Requested period in nanoseconds; zero disables this channel. */
    uint32_t period_nanoseconds;
    /** Requested duty in 1/10000 units; values above 10000 are invalid. */
    uint16_t duty_cycle_permyriad;
} HIL_Application_Pwm_Output_Value_T;

/**
 * @brief One captured PWM input measurement at its fixed array index.
 *
 * @details Array index i identifies external HIL-RIG PWM_INPUT channel i. A
 * disabled or unconfigured channel is encoded as deterministic zero and
 * ignored by Python. For configured channels, the enclosing result condition
 * determines validity for the complete fixed capture set; there is no
 * per-channel validity representation in the initial protocol.
 */
typedef struct
{
    /** Captured period in nanoseconds; zero for disabled/unconfigured input. */
    uint32_t period_nanoseconds;
    /** Captured duty in 1/10000 units; values above 10000 are invalid. */
    uint16_t duty_cycle_permyriad;
} HIL_Application_Pwm_Input_Value_T;

/**
 * @brief Declared variable-data transfer associated with one fixed tick.
 *
 * @details byte_length must be nonzero; a channel with no variable data is
 * omitted from the declaration array. Within one fixed instruction or result,
 * each (peripheral, channel) pair may be declared at most once. The future
 * codec validates those rules from the complete fixed message.
 *
 * Under the initial transaction contract, each declaration has exactly one
 * matching variable instruction/result message. A duplicate variable message
 * for the same declaration is invalid. Endpoint integration tracks that
 * cross-message relationship; the stateless codec does not. Multi-part
 * Application transfers are deferred; Transport fragmentation is transparent.
 */
typedef struct
{
    /** UART, SPI, I2C, or CAN channel carrying the variable bytes. */
    HIL_Application_Channel_Id_T channel;
    /** Exact bytes expected in the matching variable-data message. */
    uint32_t byte_length;
} HIL_Application_Data_Declaration_T;

/**
 * @brief Variable communication bytes associated with one test tick.
 *
 * @details The enclosing message envelope supplies the test ID. tick_number,
 * channel, and data.size provide Application correlation without a separate
 * sequence number. Encoding borrows data during the call; decoding copies it
 * into caller-provided storage and points data there. data.size must be
 * nonzero because channels without variable data are omitted rather than sent
 * as empty variable-data messages.
 */
typedef struct
{
    /** Zero-based tick containing this transfer; integration validates its range. */
    uint32_t tick_number;
    /** UART, SPI, I2C, or CAN logical channel. */
    HIL_Application_Channel_Id_T channel;
    /** Complete declared transfer bytes; size is the declared byte length. */
    HIL_Application_Byte_Span_T data;
} HIL_Application_Peripheral_Data_T;

/**
 * @brief Caller-selected structural bounds for stateless codec operations.
 *
 * @details HIL_APPLICATION_Init() copies this structure into the lightweight
 * codec context. Values limit message sizing, encoding, decoding, and structural
 * validation; they are local resource/policy bounds, not final wire maxima and
 * not reservations for an uploaded test.
 *
 * In particular, max_expected_tick_count limits whether the value carried by a
 * Test Configuration is structurally acceptable. The codec never allocates
 * storage for that many ticks, remembers received ticks, or decides that an
 * upload is complete. Firmware and host Application logic own those tasks.
 *
 * Zero disables the corresponding capacity until integration deliberately
 * configures it. This design does not invent production defaults. Configuration
 * does not select an Application protocol version; encoding and decoding use
 * the library's compiled-in version.
 */
typedef struct
{
    /**
     * Largest complete encoded Application message accepted or produced.
     *
     * Integration must configure Transport's maximum Application-message size
     * to at least this value. The codec does not inspect Transport configuration.
     */
    uint32_t max_encoded_message_size;

    /** Largest byte span in one variable instruction/result/error field. */
    uint32_t max_variable_data_size;

    /** Maximum peripheral configuration records in one typed configuration. */
    uint32_t max_peripheral_config_count;

    /** Maximum variable-data declarations in one typed tick body. */
    uint32_t max_variable_transfers_per_tick;

    /**
     * Largest expected_tick_count value accepted structurally.
     *
     * This is not retained upload capacity and causes no per-tick allocation.
     */
    uint32_t max_expected_tick_count;
} HIL_Application_Config_T;

/**
 * @brief Lightweight, statically allocatable Application message-codec context.
 *
 * @details The context contains immutable copied codec policy after successful
 * initialization. It does not retain caller storage, messages, encoded output,
 * decode storage, an active Test ID, upload/tick progress, outstanding variable
 * declarations, retained ticks, result-transfer progress, storage ownership,
 * execution state, endpoint role, protocol-version selection, Application
 * request identity, outstanding operations, or statistics. Per-call
 * input/output pointers are borrowed only for their documented synchronous
 * call.
 *
 * Fields are exposed so firmware and bindings may allocate the context without
 * heap allocation or a private-size query, but remain library-private. Callers
 * must initialize and inspect behavior only through the HIL_APPLICATION_ API.
 *
 * @warning One context has exactly one owning task, thread, or execution
 * context. Every operation for that context must be made by the same owner.
 * Distributing calls across tasks, callbacks, or interrupts is unsupported even
 * with external locking. The library adds no locks, atomics, callbacks, or RTOS
 * dependencies. Separate contexts may have separate owners.
 */
typedef struct
{
    /** Copied structural codec limits; library-private after initialization. */
    HIL_Application_Config_T config;

    /** Nonzero only after successful initialization; library-private. */
    uint8_t initialized;
} HIL_Application_Context_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TYPES_H */
