/**
 * @file application_test_config.h
 * @brief Test-wide configuration message body and fixed channel records.
 *
 * @details Test Configuration describes protocol-level requirements only. Array
 * index is the logical channel identity, so fixed records never carry a channel
 * or peripheral identifier on the wire. The codec does not expose or validate
 * MCU, HAL, GPIO, timer, DMA, filter-bank, prescaler, or driver details.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TEST_CONFIG_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TEST_CONFIG_H

#include <stdint.h>

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Protocol voltage selection used by Digital and PWM configuration records. */
typedef enum
{
    HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID  = 0,
    HIL_APPLICATION_PERIPHERAL_CONFIG_3V3              = 1,
    HIL_APPLICATION_PERIPHERAL_CONFIG_5V               = 2,
    HIL_APPLICATION_PERIPHERAL_CONFIG_12V              = 3,
    HIL_APPLICATION_PERIPHERAL_CONFIG_24V              = 4,
    HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED = 255
} HIL_Application_Peripheral_Config_Voltage_Level_T;

/** Master/slave role shared by SPI and I2C configuration records. */
typedef enum
{
    HIL_APPLICATION_BUS_ROLE_INVALID  = 0,
    HIL_APPLICATION_BUS_ROLE_MASTER   = 1,
    HIL_APPLICATION_BUS_ROLE_SLAVE    = 2,
    HIL_APPLICATION_BUS_ROLE_RESERVED = 255
} HIL_Application_Bus_Role_T;

/** SPI data width. */
typedef enum
{
    HIL_APPLICATION_SPI_DATA_WIDTH_INVALID  = 0,
    HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS   = 1,
    HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS  = 2,
    HIL_APPLICATION_SPI_DATA_WIDTH_RESERVED = 255
} HIL_Application_Spi_Data_Width_T;

/** SPI bit transmission order. */
typedef enum
{
    HIL_APPLICATION_SPI_BIT_ORDER_INVALID   = 0,
    HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST = 1,
    HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST = 2,
    HIL_APPLICATION_SPI_BIT_ORDER_RESERVED  = 255
} HIL_Application_Spi_Bit_Order_T;

/** SPI clock polarity. */
typedef enum
{
    HIL_APPLICATION_SPI_CLOCK_POLARITY_INVALID   = 0,
    HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW  = 1,
    HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH = 2,
    HIL_APPLICATION_SPI_CLOCK_POLARITY_RESERVED  = 255
} HIL_Application_Spi_Clock_Polarity_T;

/** SPI clock phase. */
typedef enum
{
    HIL_APPLICATION_SPI_CLOCK_PHASE_INVALID     = 0,
    HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE  = 1,
    HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE = 2,
    HIL_APPLICATION_SPI_CLOCK_PHASE_RESERVED    = 255
} HIL_Application_Spi_Clock_Phase_T;

/** UART electrical interface mode. */
typedef enum
{
    HIL_APPLICATION_UART_ELECTRICAL_MODE_INVALID  = 0,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3  = 1,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V   = 2,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232    = 3,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_RESERVED = 255
} HIL_Application_Uart_Electrical_Mode_T;

/** UART word length. */
typedef enum
{
    HIL_APPLICATION_UART_WORD_LENGTH_INVALID  = 0,
    HIL_APPLICATION_UART_WORD_LENGTH_8_BITS   = 1,
    HIL_APPLICATION_UART_WORD_LENGTH_9_BITS   = 2,
    HIL_APPLICATION_UART_WORD_LENGTH_RESERVED = 255
} HIL_Application_Uart_Word_Length_T;

/** UART parity selection. */
typedef enum
{
    HIL_APPLICATION_UART_PARITY_INVALID  = 0,
    HIL_APPLICATION_UART_PARITY_NONE     = 1,
    HIL_APPLICATION_UART_PARITY_EVEN     = 2,
    HIL_APPLICATION_UART_PARITY_ODD      = 3,
    HIL_APPLICATION_UART_PARITY_RESERVED = 255
} HIL_Application_Uart_Parity_T;

/** UART stop-bit selection. */
typedef enum
{
    HIL_APPLICATION_UART_STOP_BITS_INVALID  = 0,
    HIL_APPLICATION_UART_STOP_BITS_1        = 1,
    HIL_APPLICATION_UART_STOP_BITS_2        = 2,
    HIL_APPLICATION_UART_STOP_BITS_RESERVED = 255
} HIL_Application_Uart_Stop_Bits_T;

/** I2C voltage selection. */
typedef enum
{
    HIL_APPLICATION_I2C_VOLTAGE_INVALID  = 0,
    HIL_APPLICATION_I2C_VOLTAGE_3V3      = 1,
    HIL_APPLICATION_I2C_VOLTAGE_5V       = 2,
    HIL_APPLICATION_I2C_VOLTAGE_RESERVED = 255
} HIL_Application_I2c_Voltage_Level_T;

/** I2C pull-up resistance selection. */
typedef enum
{
    HIL_APPLICATION_I2C_PULL_UP_INVALID  = 0,
    HIL_APPLICATION_I2C_PULL_UP_1K       = 1,
    HIL_APPLICATION_I2C_PULL_UP_2K2      = 2,
    HIL_APPLICATION_I2C_PULL_UP_4K7      = 3,
    HIL_APPLICATION_I2C_PULL_UP_10K      = 4,
    HIL_APPLICATION_I2C_PULL_UP_RESERVED = 255
} HIL_Application_I2c_Pull_Up_T;

/**
 * @brief Fixed Digital Input configuration.
 *
 * @details When disabled, voltage_level must be the zero INVALID enum value.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** Enabled input voltage: 3.3 V, 5 V, 12 V, or 24 V; zero when disabled. */
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
} HIL_Application_Digital_Input_Config_T;

/**
 * @brief Fixed Digital Output configuration.
 *
 * @details When disabled, voltage_level and initial_high must both be zero.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** Enabled output voltage: 3.3 V, 5 V, 12 V, or 24 V; zero when disabled. */
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
    /** Initial output state: exactly 0 for low or 1 for high; zero when disabled. */
    uint8_t initial_high;
} HIL_Application_Digital_Output_Config_T;

/** Fixed Analogue Input configuration; enabled is the only protocol-selectable field. */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
} HIL_Application_Analog_Input_Config_T;

/** Fixed Analogue Output configuration; enabled is the only protocol-selectable field. */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
} HIL_Application_Analog_Output_Config_T;

/**
 * @brief Fixed PWM Input configuration.
 *
 * @details When disabled, voltage_level must be the zero INVALID enum value.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** Enabled input voltage: 3.3 V, 5 V, 12 V, or 24 V; zero when disabled. */
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
} HIL_Application_Pwm_Input_Config_T;

/**
 * @brief Fixed PWM Output configuration.
 *
 * @details When disabled, voltage_level, initial_period_nanoseconds and
 * initial_duty_cycle_permyriad must all be zero.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** Enabled output voltage: 3.3 V, 5 V, 12 V, or 24 V; zero when disabled. */
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
    /** Initial PWM period in nanoseconds; zero period requires zero duty cycle. */
    uint32_t initial_period_nanoseconds;
    /** Initial duty cycle in permyriad: 0..10000 represents 0..100 percent. */
    uint16_t initial_duty_cycle_permyriad;
} HIL_Application_Pwm_Output_Config_T;

/**
 * @brief Fixed standard-CAN configuration.
 *
 * @details CAN-FD and implementation-specific filters are not exposed. When
 * disabled, bit_rate, termination_enabled and capture_limit_bytes must all be zero.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** Standard-CAN nominal bit rate in bits per second; nonzero when enabled. */
    uint32_t bit_rate;
    /** Bus termination request: exactly 0 (disabled) or 1 (enabled); zero when record disabled. */
    uint8_t termination_enabled;
    /** Maximum captured receive bytes; bounded by context max_variable_data_size; zero when
     * disabled. */
    uint32_t capture_limit_bytes;
} HIL_Application_Can_Config_T;

/**
 * @brief Fixed SPI configuration.
 *
 * @details When disabled, every field after enabled must use its zero value.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** SPI bit rate in bits per second; nonzero when enabled. */
    uint32_t bit_rate;
    /** Master or slave role; INVALID/zero when disabled. */
    HIL_Application_Bus_Role_T role;
    /** Protocol data width selection, 8 or 16 bits; INVALID/zero when disabled. */
    HIL_Application_Spi_Data_Width_T data_width;
    /** MSB-first or LSB-first transmission order; INVALID/zero when disabled. */
    HIL_Application_Spi_Bit_Order_T bit_order;
    /** Clock idle polarity selection; INVALID/zero when disabled. */
    HIL_Application_Spi_Clock_Polarity_T clock_polarity;
    /** First-edge or second-edge sampling phase; INVALID/zero when disabled. */
    HIL_Application_Spi_Clock_Phase_T clock_phase;
    /** Maximum captured receive bytes; bounded by context max_variable_data_size; zero when
     * disabled. */
    uint32_t capture_limit_bytes;
} HIL_Application_Spi_Config_T;

/**
 * @brief Fixed UART configuration.
 *
 * @details At least one of rx_enabled and tx_enabled must be one when enabled.
 * capture_limit_bytes must be zero when RX is disabled. When the record is
 * disabled, every field after enabled must use its zero value.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** UART baud rate in symbols per second; nonzero when enabled. */
    uint32_t baud_rate;
    /** TTL 3.3 V, TTL 5 V, or RS-232 electrical mode; INVALID/zero when disabled. */
    HIL_Application_Uart_Electrical_Mode_T electrical_mode;
    /** UART word length, 8 or 9 bits; INVALID/zero when disabled. */
    HIL_Application_Uart_Word_Length_T word_length;
    /** UART parity: none, even, or odd; INVALID/zero when disabled. */
    HIL_Application_Uart_Parity_T parity;
    /** UART stop-bit count, 1 or 2; INVALID/zero when disabled. */
    HIL_Application_Uart_Stop_Bits_T stop_bits;
    /** Receive direction flag, exactly 0 or 1; zero when the record is disabled. */
    uint8_t rx_enabled;
    /** Transmit direction flag, exactly 0 or 1; zero when the record is disabled. */
    uint8_t tx_enabled;
    /** Maximum captured RX bytes; zero when RX is disabled and bounded by context policy. */
    uint32_t capture_limit_bytes;
} HIL_Application_Uart_Config_T;

/**
 * @brief Fixed I2C configuration.
 *
 * @details An enabled master uses own_address_7bit == 0. An enabled slave uses
 * a nonzero valid 7-bit address. When disabled, every field after enabled must
 * use its zero value.
 */
typedef struct
{
    /** Exactly 0 (disabled) or 1 (enabled). */
    uint8_t enabled;
    /** I2C bit rate in bits per second; nonzero when enabled. */
    uint32_t bit_rate;
    /** Master or slave role; INVALID/zero when disabled. */
    HIL_Application_Bus_Role_T role;
    /** Own 7-bit address: zero for master, valid nonzero 7-bit address for slave. */
    uint16_t own_address_7bit;
    /** Protocol I2C voltage selection, 3.3 V or 5 V; INVALID/zero when disabled. */
    HIL_Application_I2c_Voltage_Level_T voltage_level;
    /** Pull-up selection of 1 kOhm, 2.2 kOhm, 4.7 kOhm or 10 kOhm; zero when disabled. */
    HIL_Application_I2c_Pull_Up_T pull_up;
    /** Maximum captured receive bytes; bounded by context max_variable_data_size; zero when
     * disabled. */
    uint32_t capture_limit_bytes;
} HIL_Application_I2c_Config_T;

/**
 * @brief Python-to-firmware Test Configuration body.
 *
 * @details Every fixed array uses implicit channel identity: element i configures
 * logical channel i. Every record begins with enabled. A disabled record is
 * canonical only when all remaining fields are zero, including enum INVALID
 * value zero. The codec performs structural protocol validation only; firmware
 * later validates hardware availability, exact supported rates, electrical
 * feasibility, driver conflicts, storage capacity, and workflow state.
 *
 * The wire order is global fields, Digital Input, Digital Output, Analogue Input,
 * Analogue Output, PWM Input, PWM Output, CAN, SPI, UART, I2C, then a one-byte
 * extension length and extension bytes. The fixed payload is 197 bytes.
 */
typedef struct
{
    /** Duration represented by one instruction tick, in microseconds. */
    HIL_Application_Tick_Duration_T tick_duration_us;
    /** Nonzero upload length N, structurally bounded by context configuration. */
    uint32_t expected_tick_count;
    /** Reserved test-wide option bits; must be zero. */
    uint32_t flags;

    HIL_Application_Digital_Input_Config_T digital_in[HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT];
    HIL_Application_Digital_Output_Config_T
                                          digital_out[HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT];
    HIL_Application_Analog_Input_Config_T analog_in[HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT];
    HIL_Application_Analog_Output_Config_T analog_out[HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT];
    HIL_Application_Pwm_Input_Config_T     pwm_in[HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT];
    HIL_Application_Pwm_Output_Config_T    pwm_out[HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT];
    HIL_Application_Can_Config_T           can[HIL_APPLICATION_CAN_CHANNEL_COUNT];
    HIL_Application_Spi_Config_T           spi[HIL_APPLICATION_SPI_CHANNEL_COUNT];
    HIL_Application_Uart_Config_T          uart[HIL_APPLICATION_UART_CHANNEL_COUNT];
    HIL_Application_I2c_Config_T           i2c[HIL_APPLICATION_I2C_CHANNEL_COUNT];

    /**
     * Reserved extension bytes. Zero length may use NULL; nonzero length requires data.
     * The wire length is at most 255 bytes and the configured max_variable_data_size may be lower.
     */
    HIL_Application_Byte_Span_T extension_data;
} HIL_Application_Test_Configuration_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_TEST_CONFIG_H */
