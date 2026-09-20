/**
 * @file application_result.h
 * @brief Firmware-to-Python fixed and variable Test Result types.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESULT_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESULT_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Execution condition associated with one fixed tick result.
 *
 * @warning Numeric values may become wire identifiers.
 */
typedef enum
{
    /**
     * Every configured fixed capture is valid.
     */
    HIL_APPLICATION_RESULT_CONDITION_OK = 0,
    /**
     * Every configured fixed capture is valid but one or more requested
     * variable communication captures failed or are incomplete.
     */
    HIL_APPLICATION_RESULT_CONDITION_PARTIAL = 1,
    /**
     * At least one configured fixed capture cannot be trusted for this tick.
     *
     * The complete set of fixed captured-value fields remains present for
     * structural consistency but is semantically invalid and must be ignored.
     * This condition does not replace an Application Error sent when the
     * problem is detected.
     */
    HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM = 2,
    /** Reserved sentinel. */
    HIL_APPLICATION_RESULT_CONDITION_RESERVED = 255
} HIL_Application_Result_Condition_T;

/** Result detail used when bounded capture capacity truncates variable data. */
#define HIL_APPLICATION_RESULT_PROBLEM_DETAIL_CAPTURE_OVERFLOW ( 1u )

/**
 * @brief One fixed firmware-to-Python Test Result body.
 *
 * @details The enclosing message carries the test ID. After a successfully
 * started test configured with expected_tick_count N, firmware produces exactly
 * one fixed Test Result for every tick 0 through N - 1. If execution stops or
 * fails early, every remaining tick still has a fixed result with condition
 * EXECUTION_PROBLEM. An Application Error may report the problem when detected,
 * but never replaces this complete fixed result set. Transport/session loss,
 * reset, or inability to communicate is the exception: integration cannot
 * guarantee completion and reports that recovery is required. The codec
 * documents but does not enforce these transaction rules.
 *
 * digital_inputs, analog_inputs, and pwm_inputs are inline fixed-size arrays.
 * Index i maps deterministically to logical input channel i; no channel IDs,
 * counts, sparse entries, duplicates, or omitted-channel structures exist.
 * The analogue array has exactly one value slot per physical analogue input;
 * each configured input contributes one sample in this fixed result at the test
 * tick rate. Multi-sample/higher-rate analogue capture is deferred.
 *
 * @par Fixed wire layout
 * The fixed payload is exactly 39 bytes: tick_number at offset 0 as uint32_t
 * little-endian, 10 Digital Input bytes at offset 4, 2 Analogue Input uint32_t
 * microvolt values at offset 14, 2 PWM Input records at offset 22, the one-byte
 * condition at offset 34, and problem_detail at offset 35 as uint32_t
 * little-endian. Each PWM record is a uint32_t little-endian nanosecond period
 * followed by a uint16_t little-endian permyriad duty cycle. With the 23-byte
 * common envelope, the complete message is 62 bytes.
 *
 * Structural validation requires tick_number to be less than the initialized
 * context's max_expected_tick_count, every Digital Input value to be 0 or 1,
 * PWM duty to be at most 10000, and PWM duty to be zero when period is zero.
 * condition must be OK, PARTIAL, or EXECUTION_PROBLEM. The codec deliberately
 * leaves Analogue Input values and problem_detail unconstrained. Comparing the
 * tick with an active Test Configuration, result ordering, enabled-channel
 * policy, and hardware feasibility are integration responsibilities. PARTIAL
 * is used for bounded variable-capture loss, including the defined capture
 * overflow detail below.
 *
 * Firmware encodes deterministic zero values for fixed capture channels that
 * are disabled or not configured, and Python ignores those elements. Their
 * presence does not cause PARTIAL or EXECUTION_PROBLEM. If any configured fixed
 * capture cannot be trusted, firmware uses EXECUTION_PROBLEM and the complete
 * set of fixed values is ignored. The initial protocol cannot express selective
 * validity among fixed digital, analogue, or PWM fields.
 *
 * A variable result with condition PARTIAL and problem_detail equal to
 * HIL_APPLICATION_RESULT_PROBLEM_DETAIL_CAPTURE_OVERFLOW reports that firmware
 * retained the bounded prefix, discarded data beyond its internal capture
 * capacity, and still completed the normal result stream. No additional Error,
 * acknowledgement, or result-finalization message is introduced. A more
 * serious failure that prevents tick completion uses EXECUTION_PROBLEM instead.
 *
 * Result messages have no Application Response or Application-level
 * stop-and-wait acknowledgement. Transport owns delivery acknowledgement and
 * retransmission. Future pipelining, interleaving, ranges, declaration-based
 * variable-result delivery, or out-of-order result delivery require a versioned
 * extension. The initial protocol has no result-finalization or result-summary
 * message.
 */
typedef struct
{
    /** Zero-based tick whose execution/capture produced this result. */
    uint32_t tick_number;
    /** Complete digital-input state; element i is DIGITAL_INPUT channel i. */
    HIL_Application_Digital_Input_Value_T
        digital_inputs[HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT];
    /** One tick-rate sample per analogue input; element i is channel i. */
    HIL_Application_Analog_Input_Value_T analog_inputs[HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT];
    /** Complete PWM-input state; element i is PWM_INPUT channel i. */
    HIL_Application_Pwm_Input_Value_T pwm_inputs[HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT];
    /** Recorded condition reported after execution; not an execution-time Error. */
    HIL_Application_Result_Condition_T condition;
    /**
     * Integration-defined diagnostic value when condition is not OK.
     *
     * Detailed semantic classification remains deferred. The current fixed
     * Test Result wire body encodes this value as a little-endian uint32_t.
     */
    uint32_t problem_detail;
} HIL_Application_Test_Result_T;

/**
 * @name Variable Test Result streaming control flags
 * @{
 */
/** Streaming control flag: complete tick result. */
#define HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK ( 0x00u )
/** Streaming control flag: subsequent result chunk with the same tick follows. */
#define HIL_APPLICATION_RESULT_FLAG_HAS_MORE_CHUNKS ( 0x01u )
/** @} */

/**
 * @brief One captured peripheral event or measurement in a variable result.
 *
 * @details Represents captured input state or incoming serial stream bytes at an
 * execution tick. The codec validates that peripheral_type and channel are valid
 * and that data conforms to the peripheral's captured layout.
 */
typedef struct
{
    /** Peripheral or signal family (e.g. DIGITAL_INPUT, ANALOG_INPUT, PWM_INPUT, UART, SPI, CAN).
     */
    HIL_Application_Peripheral_Type_T peripheral_type;
    /** Logical channel number within the peripheral family (bank 0 for digital). */
    uint8_t channel;
    /** Captured record payload bytes. */
    HIL_Application_Byte_Span_T data;
} HIL_Application_Captured_Record_T;

/**
 * @brief One variable-length firmware-to-Python Test Result body.
 *
 * @details Carries captured peripheral records and serial communication buffers
 * for one zero-based tick boundary.
 *
 * @par Wire layout
 * The encoded payload begins with a 12-byte header:
 * - tick_number at offset 0 as uint32_t little-endian (4 bytes).
 * - record_count at offset 4 as uint8_t (1 byte, 0..255).
 * - condition at offset 5 as uint8_t (1 byte; OK, PARTIAL, or EXECUTION_PROBLEM).
 * - flags at offset 6 as uint8_t (1 byte; COMPLETE_TICK or HAS_MORE_CHUNKS).
 * - reserved at offset 7 as uint8_t (1 byte, must be zero).
 * - problem_detail at offset 8 as uint32_t little-endian (4 bytes; 0 when condition is OK).
 *
 * When record_count is 0, the payload consists solely of the 12-byte header.
 * When record_count > 0, the header is followed by record_count 4-byte-aligned TLV records:
 * - peripheral_type at offset 0 as uint8_t (1 byte).
 * - channel at offset 1 as uint8_t (1 byte).
 * - payload_length at offset 2 as uint16_t little-endian (2 bytes, 1..255).
 * - data bytes at offset 4 (payload_length bytes).
 * - padding bytes (0 to 3 zero bytes to align the record to a 4-byte boundary).
 *
 * Structural validation requires tick_number < max_expected_tick_count,
 * problem_detail == 0 when condition is OK, no duplicate (peripheral_type, channel)
 * pairs, and valid captured payload extents.
 */
typedef struct
{
    /** Zero-based tick whose execution/capture produced this result. */
    uint32_t tick_number;
    /** Number of captured peripheral records at records pointer (0..255). */
    uint8_t record_count;
    /** Recorded execution condition (OK, PARTIAL, EXECUTION_PROBLEM). */
    HIL_Application_Result_Condition_T condition;
    /** Streaming control flags (HIL_APPLICATION_RESULT_FLAG_*). */
    uint8_t flags;
    /** Integration-defined diagnostic value when condition is not OK (0 if OK). */
    uint32_t problem_detail;
    /** Array of captured peripheral records. */
    const HIL_Application_Captured_Record_T* records;
} HIL_Application_Variable_Test_Result_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESULT_H */
