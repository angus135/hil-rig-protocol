/**
 * @file application_test_config_internal.h
 * @brief Private Test Configuration wire sizes and offsets.
 *
 * @details These constants describe encoded bytes only. Sizing, encoding,
 * decoding, and bounded storage scanning share this single definition.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_TEST_CONFIG_INTERNAL_H
#define HIL_RIG_PROTOCOL_APPLICATION_TEST_CONFIG_INTERNAL_H

#include "application_internal.h"
#include "hil_rig_protocol/application/application_types.h"

#define HIL_APPLICATION_TEST_CONFIG_GLOBAL_FIELDS_SIZE ( 3u * HIL_APPLICATION_WIRE_U32_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_RECORD_SIZE 2u
#define HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_RECORD_SIZE 3u
#define HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_RECORD_SIZE 1u
#define HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_RECORD_SIZE 1u
#define HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_RECORD_SIZE 2u
#define HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_RECORD_SIZE 8u
#define HIL_APPLICATION_TEST_CONFIG_CAN_RECORD_SIZE 10u
#define HIL_APPLICATION_TEST_CONFIG_SPI_RECORD_SIZE 14u
#define HIL_APPLICATION_TEST_CONFIG_UART_RECORD_SIZE 15u
#define HIL_APPLICATION_TEST_CONFIG_I2C_RECORD_SIZE 14u

#define HIL_APPLICATION_TEST_CONFIG_GLOBAL_FIELDS_OFFSET 0u
#define HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_OFFSET                                           \
    HIL_APPLICATION_TEST_CONFIG_GLOBAL_FIELDS_SIZE
#define HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_OFFSET                                          \
    ( HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_OFFSET                                             \
      + HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT                                                \
            * HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_OFFSET                                            \
    ( HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_OFFSET                                            \
      + HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT                                               \
            * HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_OFFSET                                           \
    ( HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_OFFSET                                              \
      + HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT                                                 \
            * HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_OFFSET                                               \
    ( HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_OFFSET                                             \
      + HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT                                                \
            * HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_OFFSET                                              \
    ( HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_OFFSET                                                 \
      + HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT                                                    \
            * HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_CAN_OFFSET                                                     \
    ( HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_OFFSET                                                \
      + HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT                                                   \
            * HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_SPI_OFFSET                                                     \
    ( HIL_APPLICATION_TEST_CONFIG_CAN_OFFSET                                                       \
      + HIL_APPLICATION_CAN_CHANNEL_COUNT * HIL_APPLICATION_TEST_CONFIG_CAN_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_UART_OFFSET                                                    \
    ( HIL_APPLICATION_TEST_CONFIG_SPI_OFFSET                                                       \
      + HIL_APPLICATION_SPI_CHANNEL_COUNT * HIL_APPLICATION_TEST_CONFIG_SPI_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_I2C_OFFSET                                                     \
    ( HIL_APPLICATION_TEST_CONFIG_UART_OFFSET                                                      \
      + HIL_APPLICATION_UART_CHANNEL_COUNT * HIL_APPLICATION_TEST_CONFIG_UART_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET                                        \
    ( HIL_APPLICATION_TEST_CONFIG_I2C_OFFSET                                                       \
      + HIL_APPLICATION_I2C_CHANNEL_COUNT * HIL_APPLICATION_TEST_CONFIG_I2C_RECORD_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_EXTENSION_DATA_OFFSET                                          \
    ( HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET + HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE )
#define HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE                                             \
    HIL_APPLICATION_TEST_CONFIG_EXTENSION_DATA_OFFSET

_Static_assert( HIL_APPLICATION_TEST_CONFIG_DIGITAL_INPUT_OFFSET == 12u,
                "Test Configuration DI offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_DIGITAL_OUTPUT_OFFSET == 32u,
                "Test Configuration DO offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_ANALOG_INPUT_OFFSET == 62u,
                "Test Configuration AI offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_ANALOG_OUTPUT_OFFSET == 64u,
                "Test Configuration AO offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_PWM_INPUT_OFFSET == 70u,
                "Test Configuration PWM input offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_PWM_OUTPUT_OFFSET == 74u,
                "Test Configuration PWM output offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_CAN_OFFSET == 90u, "Test Configuration CAN offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_SPI_OFFSET == 110u, "Test Configuration SPI offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_UART_OFFSET == 138u, "Test Configuration UART offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_I2C_OFFSET == 168u, "Test Configuration I2C offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_EXTENSION_LENGTH_OFFSET == 196u,
                "Test Configuration extension length offset" );
_Static_assert( HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE == 197u,
                "Test Configuration fixed payload size" );

#endif /* HIL_RIG_PROTOCOL_APPLICATION_TEST_CONFIG_INTERNAL_H */
