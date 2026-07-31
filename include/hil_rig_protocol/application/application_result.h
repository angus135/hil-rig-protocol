/**
 * @file application_result.h
 * @brief Fixed test-result and variable result-data message bodies.
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
    /** No execution problem is reported for this tick. */
    HIL_APPLICATION_RESULT_CONDITION_OK = 0,
    /** Some capture data is valid but the result is incomplete. */
    HIL_APPLICATION_RESULT_CONDITION_PARTIAL = 1,
    /**
     * Reporting-stage summary that execution/capture failed for this tick.
     *
     * This does not replace the Application Error sent when the execution
     * problem is detected.
     */
    HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM = 2,
    /** Reserved sentinel. */
    HIL_APPLICATION_RESULT_CONDITION_RESERVED = 255
} HIL_Application_Result_Condition_T;

/**
 * @brief One fixed HIL-RIG-to-host Test Result body.
 *
 * @details The enclosing message carries the test ID. Firmware may make normal
 * Test Results available after execution completes. Execution-time problems use
 * Application Error instead of ordinary results. The codec documents but does
 * not enforce this transaction prerequisite.
 *
 * digital_inputs, analog_inputs, and pwm_inputs are inline fixed-size arrays.
 * Index i maps deterministically to logical input channel i; no channel IDs,
 * counts, sparse entries, duplicates, or omitted-channel semantics exist.
 * Variable declarations remain pointer/count data and identify complete result
 * messages that follow. Transport fragmentation remains invisible.
 *
 * For a test with N ticks, host integration considers result transfer complete
 * only after decoding every fixed result for ticks 0 through N - 1 and every
 * variable result declared by those fixed messages. The codec does not track
 * that progress.
 */
typedef struct
{
    /** Zero-based tick whose execution/capture produced this result. */
    uint32_t tick_number;
    /** Complete digital-input state; element i is DIGITAL_INPUT channel i. */
    HIL_Application_Digital_Input_Value_T
        digital_inputs[HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT];
    /** Complete analogue-input state; element i is ANALOG_INPUT channel i. */
    HIL_Application_Analog_Input_Value_T analog_inputs[HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT];
    /** Complete PWM-input state; element i is PWM_INPUT channel i. */
    HIL_Application_Pwm_Input_Value_T pwm_inputs[HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT];
    /** Declared UART/SPI/I2C/CAN result transfers. */
    const HIL_Application_Data_Declaration_T* variable_data;
    /** Number of result declarations at variable_data. */
    size_t variable_data_count;
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
 * instruction data.
 */
typedef HIL_Application_Peripheral_Data_T HIL_Application_Variable_Result_Data_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESULT_H */
