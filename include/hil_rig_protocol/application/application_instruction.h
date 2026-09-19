/**
 * @file application_instruction.h
 * @brief Python-to-firmware fixed Test Instruction, variable Update Instruction, and related types.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_INSTRUCTION_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_INSTRUCTION_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief One fixed Python-to-firmware Test Instruction body.
 *
 * @details Each message describes exactly one zero-based tick. The enclosing
 * message carries the test ID. digital_outputs, analog_outputs, and pwm_outputs
 * are inline fixed-size arrays containing the complete state for every physical
 * HIL-RIG output channel. Index i maps deterministically to logical channel i.
 * There are no channel IDs, element counts, sparse entries, duplicates, omitted
 * channels, or implicit "retain the previous value" semantics.
 *
 * @par Fixed wire layout
 * The fixed payload is exactly 50 bytes: tick_number at offset 0 as uint32_t
 * little-endian, 10 Digital Output bytes at offset 4, 6 Analogue Output
 * uint32_t microvolt values at offset 14, then 2 PWM Output records at offset
 * 38. Each PWM record is a uint32_t little-endian nanosecond period followed by
 * a uint16_t little-endian permyriad duty cycle. With the 23-byte common
 * envelope, the complete message is 73 bytes.
 *
 * Structural validation requires tick_number to be less than the initialized
 * context's max_expected_tick_count, every Digital Output value to be 0 or 1,
 * PWM duty to be at most 10000, and PWM duty to be zero when period is zero.
 * The codec imposes no Analogue Output range or hardware-specific PWM limits.
 * Comparing tick_number with an active Test Configuration's actual
 * expected_tick_count, tick ordering, enabled-channel policy, and hardware
 * feasibility are integration responsibilities.
 *
 */
typedef struct
{
    /** Zero-based tick identity; codec requires value < context max_expected_tick_count. */
    uint32_t tick_number;
    /** Complete digital-output state; element i is DIGITAL_OUTPUT channel i. */
    HIL_Application_Digital_Output_Value_T
        digital_outputs[HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT];
    /** Complete analogue-output state; element i is ANALOG_OUTPUT channel i. */
    HIL_Application_Analog_Output_Value_T
        analog_outputs[HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT];
    /** Complete PWM-output state; element i is PWM_OUTPUT channel i. */
    HIL_Application_Pwm_Output_Value_T pwm_outputs[HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT];
} HIL_Application_Test_Instruction_T;

/**
 * @name Update Instruction streaming control flags
 * @{
 */
/** Streaming control flag: final or only message for this tick. */
#define HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK ( 0x00u )
/** Streaming control flag: subsequent chunk message with the same tick follows. */
#define HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS ( 0x01u )
/** @} */

/**
 * @brief One logical peripheral operation for an update-based instruction.
 *
 * @details Represents a discrete output change or communication transfer at an
 * execution tick. The codec validates that peripheral_type and channel are valid
 * and that payload conforms to the peripheral's logical data layout.
 */
typedef struct
{
    /** Peripheral or signal family (e.g. DIGITAL_OUTPUT, ANALOG_OUTPUT, PWM_OUTPUT, UART, SPI,
     * CAN). */
    HIL_Application_Peripheral_Type_T peripheral_type;
    /** Logical channel number within the peripheral family (bank 0 for digital). */
    uint8_t channel;
    /** Logical operation payload bytes. */
    HIL_Application_Byte_Span_T payload;
} HIL_Application_Logical_Operation_T;

/**
 * @brief One variable-length Python-to-firmware Update Instruction body.
 *
 * @details Carries sparse logical peripheral operations and streaming serial data
 * for one zero-based tick boundary.
 *
 * @par Wire layout
 * The encoded payload begins with an 8-byte header:
 * - tick_number at offset 0 as uint32_t little-endian (4 bytes).
 * - operation_count at offset 4 as uint8_t (1 byte, 1..255).
 * - flags at offset 5 as uint8_t (1 byte; COMPLETE_TICK or HAS_MORE_CHUNKS).
 * - reserved at offset 6 as uint16_t little-endian (2 bytes, must be zero).
 *
 * The header is followed by operation_count 4-byte-aligned TLV records:
 * - peripheral_type at offset 0 as uint8_t (1 byte).
 * - channel at offset 1 as uint8_t (1 byte).
 * - payload_length at offset 2 as uint16_t little-endian (2 bytes, 1..255).
 * - payload bytes at offset 4 (payload_length bytes).
 * - padding bytes (0 to 3 zero bytes to align the record to a 4-byte boundary).
 *
 * Structural validation requires tick_number < max_expected_tick_count,
 * operation_count in 1..255, operations != NULL, no duplicate (peripheral_type, channel)
 * pairs, and valid payload layouts for DIGITAL_OUTPUT, ANALOG_OUTPUT, PWM_OUTPUT,
 * UART, SPI, and CAN.
 */
typedef struct
{
    /** Zero-based tick identity; codec requires value < context max_expected_tick_count. */
    uint32_t tick_number;
    /** Number of operations at operations pointer (1..255). */
    uint8_t operation_count;
    /** Streaming control flags (HIL_APPLICATION_INSTRUCTION_FLAG_*). */
    uint8_t flags;
    /** Array of logical operations packed in this message. */
    const HIL_Application_Logical_Operation_T* operations;
} HIL_Application_Update_Instruction_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_INSTRUCTION_H */
