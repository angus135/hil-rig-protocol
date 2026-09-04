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
     * Every configured fixed capture is valid. Under the future variable-data
     * declaration design, every declaration would identify valid variable
     * result data associated with this result.
     */
    HIL_APPLICATION_RESULT_CONDITION_OK = 0,
    /**
     * Reserved for future variable-data semantics in which every configured
     * fixed capture is valid but one or more requested variable communication
     * captures failed or are incomplete. The current implementation does not
     * encode result declarations or variable result-data messages.
     */
    HIL_APPLICATION_RESULT_CONDITION_PARTIAL = 1,
    /**
     * At least one configured fixed capture cannot be trusted for this tick.
     *
     * The complete set of fixed captured-value fields remains present for
     * structural consistency but is semantically invalid and must be ignored.
     * A future declaration design may still identify valid variable result data.
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
 * presence does not cause PARTIAL or EXECUTION_PROBLEM. If any configured fixed
 * capture cannot be trusted, firmware uses EXECUTION_PROBLEM and the complete
 * set of fixed values is ignored. The initial protocol cannot express selective
 * validity among fixed digital, analogue, or PWM fields.
 *
 * @par Future variable-data declaration design
 * A future version may add declarations that associate nonzero data.size values
 * with variable result-data messages and define their uniqueness, ordering,
 * completeness, and PARTIAL-result semantics. Those declarations are not
 * represented by HIL_Application_Test_Result_T and are not encoded or validated
 * by the current implementation. The commented declaration members below are
 * retained only as design notes and must not be treated as part of the current
 * public wire contract.
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
    /** Future design only: variable-data declarations are not currently encoded. */
    // const HIL_Application_Data_Declaration_T* variable_data;
    // /** Number of result declarations at variable_data. */
    // uint32_t variable_data_count;
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
 * @brief Future variable result-data body for one tick/channel.
 *
 * @details This alias is retained for future variable-message design. The
 * current Application implementation does not encode or decode variable
 * result-data messages, and Test Result bodies do not contain declarations that
 * reference them. Future correlation and storage ownership are expected to
 * follow the corresponding variable instruction-data design.
 */
typedef HIL_Application_Peripheral_Data_T HIL_Application_Variable_Result_Data_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESULT_H */
