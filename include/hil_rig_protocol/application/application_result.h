/**
 * @file application_result.h
 * @brief Firmware-to-Python fixed and variable result message bodies.
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
     * Every configured fixed capture is valid and every declaration identifies
     * valid variable result data that follows this result.
     */
    HIL_APPLICATION_RESULT_CONDITION_OK = 0,
    /**
     * Every configured fixed capture is valid, but one or more requested
     * variable communication captures failed or are incomplete. Declarations
     * identify only valid variable result data that follows this result.
     */
    HIL_APPLICATION_RESULT_CONDITION_PARTIAL = 1,
    /**
     * At least one configured fixed capture cannot be trusted for this tick.
     *
     * The complete set of fixed captured-value fields remains present for
     * structural consistency but is semantically invalid and must be ignored.
     * Declarations may still identify valid variable result data that follows.
     * This condition does not replace an Application Error sent when the
     * problem is detected.
     */
    HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM = 2,
    /** Reserved sentinel. */
    HIL_APPLICATION_RESULT_CONDITION_RESERVED = 255
} HIL_Application_Result_Condition_T;

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
 * Firmware encodes deterministic zero values for fixed capture channels that
 * are disabled or not configured, and Python ignores those elements. Their
 * presence does not cause PARTIAL or EXECUTION_PROBLEM. OK means every
 * configured fixed capture is valid and every declaration identifies valid
 * variable data. PARTIAL means every configured fixed capture remains valid,
 * but one or more requested variable communication captures failed or are
 * incomplete; declarations identify only the valid variable messages that
 * follow. If any configured fixed capture cannot be trusted, firmware uses
 * EXECUTION_PROBLEM. Python then ignores the complete set of fixed values;
 * declarations may still identify valid variable data. The initial protocol
 * cannot express selective validity among fixed digital, analogue, or PWM
 * fields.
 *
 * Each declaration has nonzero byte_length and a unique (peripheral, channel)
 * pair within the fixed result. It identifies exactly one complete variable
 * result message; duplicate matching messages are invalid. For tick T,
 * firmware sends this fixed result first and then sends every declared variable
 * result in declaration order. It completes all declarations before sending
 * the fixed result for tick T + 1. Fixed results are sent in increasing order
 * from tick 0 through N - 1, and variable data never precedes its declaring
 * fixed result. Transport fragmentation remains invisible.
 *
 * For a test with N ticks, host integration considers result transfer complete
 * only after decoding every fixed result for ticks 0 through N - 1 and every
 * variable result declared by those fixed messages. The codec does not track
 * that progress. Result messages have no Application Response or
 * Application-level stop-and-wait acknowledgement. Transport owns delivery
 * acknowledgement and retransmission. Early execution failure and an optional
 * Application Error do not replace or reorder the required result set. Future
 * pipelining, interleaving, ranges, or out-of-order result delivery require a
 * versioned extension. The initial protocol has no result-finalization or
 * result-summary message.
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
    /** Unique, nonzero UART/SPI/I2C/CAN result transfers. */
    // const HIL_Application_Data_Declaration_T* variable_data;
    // /** Number of result declarations at variable_data. */
    // uint32_t variable_data_count;
    /** Recorded condition reported after execution; not an execution-time Error. */
    HIL_Application_Result_Condition_T condition;
    /**
     * Integration-defined diagnostic value when condition is not OK.
     *
     * Detailed classification and final wire width remain TODO.
     */
    uint32_t problem_detail;
} HIL_Application_Test_Result_T;

/**
 * @brief Complete variable result-data body for one tick/channel.
 *
 * @details Correlation and caller decode-storage ownership match variable
 * instruction data. Firmware sends each message after the fixed result that
 * declares it, in declaration order, before sending the next fixed result.
 */
typedef HIL_Application_Peripheral_Data_T HIL_Application_Variable_Result_Data_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESULT_H */
