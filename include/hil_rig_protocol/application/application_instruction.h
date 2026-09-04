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
 * @par Future variable-data declaration design
 * A future version may add variable_data declarations that associate nonzero
 * data.size values with variable instruction-data messages for the same test
 * tick and logical communication channel. Such declarations, their uniqueness
 * rules, completion semantics, ordering, and integration acceptance policy are
 * not represented by HIL_Application_Test_Instruction_T and are not encoded or
 * validated by the current implementation. The commented declaration members
 * below are retained only as design notes and must not be treated as part of the
 * current public wire contract.
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
    // /** Future design only: variable-data declarations are not currently encoded. */
    // const HIL_Application_Data_Declaration_T* variable_data;
    // /** Number of declarations at variable_data. */
    // uint8_t variable_data_count;
} HIL_Application_Test_Instruction_T;

/**
 * @brief Future variable communication bytes associated with one test tick.
 *
 * @details This structure is retained for future variable-message design. The
 * current Application implementation does not encode or decode variable
 * instruction-data messages, and Test Instruction bodies do not contain
 * declarations that reference them. Future correlation is expected to use the
 * enclosing test ID together with tick_number, channel, and data.size.
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
