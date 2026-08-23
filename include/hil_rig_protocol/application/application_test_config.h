/**
 * @file application_test_config.h
 * @brief Test-wide configuration message body and peripheral records.
 *
 * @details Test Configuration is the first test-specific Python-to-firmware
 * message. It describes protocol-level requirements without exposing MCU
 * registers, port addresses, driver handles, timers, or scheduling behavior.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TEST_CONFIG_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TEST_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Peripheral configuration record category.
 *
 * @warning Values may become wire identifiers and require compatibility review
 * if changed after publication.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID = 0,
    /** Digital input/output electrical configuration. */
    HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL = 1,
    /** Analogue input/output range configuration. */
    HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG = 2,
    /** PWM generation or capture configuration. */
    HIL_APPLICATION_PERIPHERAL_CONFIG_PWM = 3,
    /** UART/SPI/I2C/CAN communication and capture configuration. */
    HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION = 4,
    /** Reserved sentinel. */
    HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED = 255
} HIL_Application_Peripheral_Config_Type_T;

/**
 * @brief Digital channel voltage and initial-state configuration.
 *
 * @details Direction is carried by channel.peripheral and must be
 * DIGITAL_INPUT or DIGITAL_OUTPUT. Voltage support and safe limits require
 * firmware semantic validation.
 */
typedef struct
{
    /** Protocol-level digital input or output channel. */
    HIL_Application_Channel_Id_T channel;
    /** Nominal output level in millivolts; zero for input-only channels. */
    uint32_t output_millivolts;
    /** Input-high threshold in millivolts; zero for output-only channels. */
    uint32_t input_threshold_millivolts;
    /** Initial output state; zero for input channels. */
    uint8_t initial_output_high;
    /** Nonzero requests capture of input state for result reporting. */
    uint8_t capture_enabled;
} HIL_Application_Digital_Config_T;

/**
 * @brief Analogue channel range configuration.
 *
 * @details The initial protocol does not select an independent sample rate or
 * sample count. Each fixed Test Result has one analogue input element per
 * physical channel; for each configured ANALOG_INPUT it is one sample for that
 * tick, produced at the configured test tick rate. Hardware-specific gain,
 * ADC/DAC selection, calibration, and achievable timing remain firmware
 * responsibilities. Higher-rate or multi-sample capture requires a future
 * versioned extension.
 */
typedef struct
{
    /** Protocol-level ANALOG_INPUT or ANALOG_OUTPUT channel. */
    HIL_Application_Channel_Id_T channel;
    /** Minimum requested/supported signal in microvolts. */
    int32_t minimum_microvolts;
    /** Maximum requested/supported signal in microvolts. */
    int32_t maximum_microvolts;
} HIL_Application_Analog_Config_T;

/** Protocol-level PWM generation or capture configuration. */
typedef struct
{
    /** Protocol-level PWM_INPUT or PWM_OUTPUT channel. */
    HIL_Application_Channel_Id_T channel;
    /** Nominal period in nanoseconds; zero is invalid for PWM output. */
    uint32_t period_nanoseconds;
    /** Initial duty cycle in 1/10000 units for PWM output. */
    uint16_t initial_duty_cycle_permyriad;
    /** Nonzero requests period/duty capture in fixed result messages. */
    uint8_t capture_enabled;
} HIL_Application_Pwm_Config_T;

/**
 * @brief Protocol-level serial/bus channel configuration.
 *
 * @details The flags field reserves family-specific protocol options whose
 * exact bit assignments remain TODO. It must be zero in the initial protocol
 * and must not contain MCU register values. A nonzero value is structurally
 * unsupported.
 */
typedef struct
{
    /** UART, SPI, I2C, or CAN logical channel. */
    HIL_Application_Channel_Id_T channel;
    /** Requested communication rate in bits per second. */
    uint32_t bit_rate;
    /** Reserved protocol option bits; must be zero in the initial protocol. */
    uint32_t flags;
    /** Maximum captured bytes per tick; zero disables result capture. */
    size_t capture_limit_bytes;
} HIL_Application_Communication_Config_T;

/**
 * @brief Tagged peripheral configuration record.
 *
 * @details type selects exactly one union member. Within one Test
 * Configuration, each (peripheral, channel) pair may appear at most once;
 * duplicate records are structurally invalid rather than first-wins or
 * last-wins. The codec validates tag/member and uniqueness rules, but firmware
 * decides channel support and electrical feasibility.
 */
typedef struct
{
    /** Tag selecting the valid union member. */
    HIL_Application_Peripheral_Config_Type_T type;
    /** Type-specific protocol configuration. */
    union
    {
        /** Valid when type is DIGITAL. */
        HIL_Application_Digital_Config_T digital;
        /** Valid when type is ANALOG. */
        HIL_Application_Analog_Config_T analog;
        /** Valid when type is PWM. */
        HIL_Application_Pwm_Config_T pwm;
        /** Valid when type is COMMUNICATION. */
        HIL_Application_Communication_Config_T communication;
    } value;
} HIL_Application_Peripheral_Config_T;

/**
 * @brief Python-to-firmware Test Configuration body.
 *
 * @details The enclosing message must carry a test ID and be the first message
 * for that test. expected_tick_count must be nonzero and is authoritative: a
 * test with N ticks has exactly ticks 0 through N - 1, uploaded in increasing
 * stop-and-wait order. Integration automatically performs whole-test validation
 * after all N fixed instructions and their declared variable data have been
 * accepted; no finalize command is required. Successful whole-test validation
 * produces a Complete Test Response whose ACCEPTED outcome makes the identified
 * test available for a subsequent START request. There is no ARM or
 * FINALIZE_TEST command. The codec can bound expected_tick_count structurally
 * but cannot compare later messages against an active configuration.
 *
 * A configuration-scoped ACCEPTED Response creates the active upload
 * transaction. A negative Response creates no transaction; host integration
 * starts a new upload from Test Configuration with a fresh random Test ID.
 *
 * peripherals points to peripheral_count records and is borrowed only during
 * validation/encoding; after decoding it points into caller-owned decode
 * storage.
 *
 * flags must be zero and extension_data must be empty in the initial protocol.
 * They reserve versioned test-wide settings approved later. A nonzero flag or
 * nonempty extension produces HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE and
 * must not be used to smuggle hardware mappings.
 */
typedef struct
{
    /** Unit-explicit duration represented by one instruction tick. */
    HIL_Application_Tick_Duration_T tick_duration;
    /** Nonzero upload length N, defining valid ticks 0 through N - 1. */
    uint32_t expected_tick_count;
    /** Reserved test-wide option bits; must be zero in the initial protocol. */
    uint32_t flags;
    /** Number of readable records at peripherals. */
    size_t peripheral_count;
    /** Peripheral/channel configuration records. */
    const HIL_Application_Peripheral_Config_T* peripherals;
    /** Reserved versioned settings bytes; must be empty in the initial protocol. */
    HIL_Application_Byte_Span_T extension_data;
} HIL_Application_Test_Configuration_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TEST_CONFIG_H */
