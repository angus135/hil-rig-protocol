/**
 * @file application_instruction.h
 * @brief Python-to-firmware fixed and variable instruction bodies.
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
 * Every variable_data declaration has a nonzero byte_length and requires one
 * HIL_Application_Variable_Instruction_Data_T message with matching test ID,
 * tick, peripheral, channel, and byte count. Channels with no variable data are
 * omitted, and each (peripheral, channel) pair may appear at most once in the
 * declaration array. Duplicate matching variable messages are invalid. The
 * fixed tick is complete only after all declarations are satisfied and
 * integration accepts responsibility for retaining it. Retention
 * may use NAND, RAM, or an accepted storage-manager queue; it is not required to
 * be a completed NAND write. The codec can validate the fixed values and
 * declaration structure, but only integration with the active Test
 * Configuration can verify tick order and tick_number < expected_tick_count.
 * Under the initial stop-and-wait transaction contract, the fixed message and
 * all of its associated variable messages collectively form the only tick
 * awaiting semantic acceptance. Python must not submit tick T + 1 until it
 * receives Tick T ACCEPTED. Rejection or failure of the fixed tick or any
 * associated variable data invalidates the upload and requires restart from
 * Test Configuration. This ordering is Application integration policy, not
 * codec, Transport, or execution-manager state.
 */
typedef struct
{
    /** Zero-based tick identity; integration requires 0 <= value < configured N. */
    uint32_t tick_number;
    /** Complete digital-output state; element i is DIGITAL_OUTPUT channel i. */
    HIL_Application_Digital_Output_Value_T
        digital_outputs[HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT];
    /** Complete analogue-output state; element i is ANALOG_OUTPUT channel i. */
    HIL_Application_Analog_Output_Value_T
        analog_outputs[HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT];
    /** Complete PWM-output state; element i is PWM_OUTPUT channel i. */
    HIL_Application_Pwm_Output_Value_T pwm_outputs[HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT];
    // /** Unique, nonzero UART/SPI/I2C/CAN transfers for this tick. */
    // const HIL_Application_Data_Declaration_T* variable_data;
    // /** Number of declarations at variable_data. */
    // uint8_t variable_data_count;
} HIL_Application_Test_Instruction_T;

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
    /** The remaining number of packets */
    uint32_t remaining;
    /** UART, SPI, I2C, or CAN logical channel. */
    HIL_Application_Channel_Id_T channel;
    /** Complete declared transfer bytes; size is the declared byte length. */
    HIL_Application_Byte_Span_T data;
} HIL_Application_Variable_Instruction_Data_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_INSTRUCTION_H */
