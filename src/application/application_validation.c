/**
 * @file application_validation.c
 * @brief Structural validation for typed Application message bodies.
 *
 * @details This file implements only rules owned by the stateless codec.
 * Stateful transaction ordering, hardware capability checks, variable-data
 * declaration/correlation rules, and Response/Error semantics remain deferred.
 */

#include "hil_rig_protocol/application/application_message.h"
#include "application_internal.h"
#include "application_size.h"
#include "application_validation.h"
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"
#include <string.h>

static HIL_Application_Status_T
HIL_APPLICATION_Byte_Span_validate( const HIL_Application_Byte_Span_T* span,
                                    size_t                             max_allowed_size )
{
    if ( span == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( ( size_t )span->size > max_allowed_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( span->size != 0u && span->data == NULL )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

static int HIL_APPLICATION_Boolean_Is_Valid( uint8_t value )
{
    return value <= 1u;
}

static int HIL_APPLICATION_Pwm_Value_Is_Valid( uint32_t period_nanoseconds,
                                               uint16_t duty_cycle_permyriad )
{
    return duty_cycle_permyriad <= 10000u
           && ( period_nanoseconds != 0u || duty_cycle_permyriad == 0u );
}

static int
HIL_APPLICATION_Config_Voltage_Is_Valid( HIL_Application_Peripheral_Config_Voltage_Level_T voltage )
{
    return voltage == HIL_APPLICATION_PERIPHERAL_CONFIG_3V3
           || voltage == HIL_APPLICATION_PERIPHERAL_CONFIG_5V
           || voltage == HIL_APPLICATION_PERIPHERAL_CONFIG_12V
           || voltage == HIL_APPLICATION_PERIPHERAL_CONFIG_24V;
}

static int HIL_APPLICATION_Bus_Role_Is_Valid( HIL_Application_Bus_Role_T role )
{
    return role == HIL_APPLICATION_BUS_ROLE_MASTER || role == HIL_APPLICATION_BUS_ROLE_SLAVE;
}

static HIL_Application_Status_T
HIL_APPLICATION_Digital_Input_Config_validate( const HIL_Application_Digital_Input_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->voltage_level == HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_Config_Voltage_Is_Valid( data->voltage_level )
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

static HIL_Application_Status_T HIL_APPLICATION_Digital_Output_Config_validate(
    const HIL_Application_Digital_Output_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled )
         || !HIL_APPLICATION_Boolean_Is_Valid( data->initial_high ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->voltage_level == HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID
                       && data->initial_high == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_Config_Voltage_Is_Valid( data->voltage_level )
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

static HIL_Application_Status_T
HIL_APPLICATION_Analog_Input_Config_validate( const HIL_Application_Analog_Input_Config_T* data )
{
    return HIL_APPLICATION_Boolean_Is_Valid( data->enabled )
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

static HIL_Application_Status_T
HIL_APPLICATION_Analog_Output_Config_validate( const HIL_Application_Analog_Output_Config_T* data )
{
    return HIL_APPLICATION_Boolean_Is_Valid( data->enabled )
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

static HIL_Application_Status_T
HIL_APPLICATION_Pwm_Input_Config_validate( const HIL_Application_Pwm_Input_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->voltage_level == HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_Config_Voltage_Is_Valid( data->voltage_level )
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

static HIL_Application_Status_T
HIL_APPLICATION_Pwm_Output_Config_validate( const HIL_Application_Pwm_Output_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->voltage_level == HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID
                       && data->initial_period_nanoseconds == 0u
                       && data->initial_duty_cycle_permyriad == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( !HIL_APPLICATION_Config_Voltage_Is_Valid( data->voltage_level )
         || data->initial_duty_cycle_permyriad > 10000u
         || ( data->initial_period_nanoseconds == 0u && data->initial_duty_cycle_permyriad != 0u ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Can_Config_validate( const HIL_Application_Context_T*    context,
                                     const HIL_Application_Can_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->bit_rate == 0u && data->capture_limit_bytes == 0u && data->filter_id == 0u
                       && data->filter_mask == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->bit_rate == 0u
         || data->capture_limit_bytes > ( uint32_t )context->config.max_variable_data_size
         || data->filter_id > 0x07ffu || data->filter_mask > 0x07ffu )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Spi_Config_validate( const HIL_Application_Context_T*    context,
                                     const HIL_Application_Spi_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->bit_rate == 0u && data->role == HIL_APPLICATION_BUS_ROLE_INVALID
                       && data->data_width == HIL_APPLICATION_SPI_DATA_WIDTH_INVALID
                       && data->bit_order == HIL_APPLICATION_SPI_BIT_ORDER_INVALID
                       && data->clock_polarity == HIL_APPLICATION_SPI_CLOCK_POLARITY_INVALID
                       && data->clock_phase == HIL_APPLICATION_SPI_CLOCK_PHASE_INVALID
                       && data->capture_limit_bytes == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->bit_rate == 0u || !HIL_APPLICATION_Bus_Role_Is_Valid( data->role )
         || ( data->data_width != HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS
              && data->data_width != HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS )
         || ( data->bit_order != HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST
              && data->bit_order != HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST )
         || ( data->clock_polarity != HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW
              && data->clock_polarity != HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH )
         || ( data->clock_phase != HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE
              && data->clock_phase != HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE )
         || data->capture_limit_bytes > ( uint32_t )context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Uart_Config_validate( const HIL_Application_Context_T*     context,
                                      const HIL_Application_Uart_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled )
         || !HIL_APPLICATION_Boolean_Is_Valid( data->rx_enabled )
         || !HIL_APPLICATION_Boolean_Is_Valid( data->tx_enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->baud_rate == 0u
                       && data->electrical_mode == HIL_APPLICATION_UART_ELECTRICAL_MODE_INVALID
                       && data->word_length == HIL_APPLICATION_UART_WORD_LENGTH_INVALID
                       && data->parity == HIL_APPLICATION_UART_PARITY_INVALID
                       && data->stop_bits == HIL_APPLICATION_UART_STOP_BITS_INVALID
                       && data->rx_enabled == 0u && data->tx_enabled == 0u
                       && data->capture_limit_bytes == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->baud_rate == 0u
         || ( data->electrical_mode != HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3
              && data->electrical_mode != HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V
              && data->electrical_mode != HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232 )
         || ( data->word_length != HIL_APPLICATION_UART_WORD_LENGTH_8_BITS
              && data->word_length != HIL_APPLICATION_UART_WORD_LENGTH_9_BITS )
         || ( data->parity != HIL_APPLICATION_UART_PARITY_NONE
              && data->parity != HIL_APPLICATION_UART_PARITY_EVEN
              && data->parity != HIL_APPLICATION_UART_PARITY_ODD )
         || ( data->stop_bits != HIL_APPLICATION_UART_STOP_BITS_1
              && data->stop_bits != HIL_APPLICATION_UART_STOP_BITS_2 )
         || ( data->rx_enabled == 0u && data->tx_enabled == 0u )
         || ( data->rx_enabled == 0u && data->capture_limit_bytes != 0u )
         || data->capture_limit_bytes > ( uint32_t )context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_I2c_Config_validate( const HIL_Application_Context_T*    context,
                                     const HIL_Application_I2c_Config_T* data )
{
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->bit_rate == 0u && data->role == HIL_APPLICATION_BUS_ROLE_INVALID
                       && data->own_address_7bit == 0u
                       && data->voltage_level == HIL_APPLICATION_I2C_VOLTAGE_INVALID
                       && data->pull_up == HIL_APPLICATION_I2C_PULL_UP_INVALID
                       && data->capture_limit_bytes == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->bit_rate == 0u || !HIL_APPLICATION_Bus_Role_Is_Valid( data->role )
         || ( data->voltage_level != HIL_APPLICATION_I2C_VOLTAGE_3V3
              && data->voltage_level != HIL_APPLICATION_I2C_VOLTAGE_5V )
         || ( data->pull_up != HIL_APPLICATION_I2C_PULL_UP_1K
              && data->pull_up != HIL_APPLICATION_I2C_PULL_UP_2K2
              && data->pull_up != HIL_APPLICATION_I2C_PULL_UP_4K7
              && data->pull_up != HIL_APPLICATION_I2C_PULL_UP_10K )
         || data->capture_limit_bytes > ( uint32_t )context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->role == HIL_APPLICATION_BUS_ROLE_MASTER && data->own_address_7bit != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->role == HIL_APPLICATION_BUS_ROLE_SLAVE
         && ( data->own_address_7bit == 0u || data->own_address_7bit > 0x7fu ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_System_Info_Request_validate( const HIL_Application_Context_T*             context,
                                              const HIL_Application_System_Info_Request_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->request_firmware_git_hash > 1u
         || data->query != HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC
         || data->application_protocol_major > UINT8_MAX
         || data->application_protocol_minor > UINT8_MAX )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_System_Info_Response_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_System_Info_Response_T* data )
{
    HIL_Application_Status_T status;

    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->application_protocol_major > UINT8_MAX
         || data->application_protocol_minor > UINT8_MAX )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    status = HIL_APPLICATION_Byte_Span_validate( &data->firmware_git_hash,
                                                 context->config.max_variable_data_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    return HIL_APPLICATION_Byte_Span_validate( &data->diagnostic_data,
                                               context->config.max_variable_data_size );
}

HIL_Application_Status_T
HIL_APPLICATION_Test_Configuration_validate( const HIL_Application_Context_T*            context,
                                             const HIL_Application_Test_Configuration_T* data )
{
    static const uint32_t    valid_periods_us[] = HIL_APPLICATION_VALID_TICK_PERIODS_US;
    HIL_Application_Status_T status;
    uint8_t                  tick_is_valid = 0u;

    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->expected_tick_count == 0u
         || data->expected_tick_count > context->config.max_expected_tick_count
         || data->flags != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    for ( size_t i = 0u; i < sizeof( valid_periods_us ) / sizeof( valid_periods_us[0] ); ++i )
    {
        if ( valid_periods_us[i] == data->tick_duration_us.microseconds )
        {
            tick_is_valid = 1u;
            break;
        }
    }
    if ( tick_is_valid == 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    status = HIL_APPLICATION_Byte_Span_validate( &data->extension_data,
                                                 context->config.max_variable_data_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status == HIL_APPLICATION_STATUS_INVALID_ARGUMENT
                   ? HIL_APPLICATION_STATUS_VALIDATION_FAILED
                   : status;
    }

#define HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( array_, count_, validator_ )                        \
    do                                                                                             \
    {                                                                                              \
        for ( size_t i_ = 0u; i_ < ( count_ ); ++i_ )                                              \
        {                                                                                          \
            status = validator_( &( array_ )[i_] );                                                \
            if ( status != HIL_APPLICATION_STATUS_OK )                                             \
            {                                                                                      \
                return status;                                                                     \
            }                                                                                      \
        }                                                                                          \
    } while ( 0 )

    HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( data->digital_in,
                                           HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT,
                                           HIL_APPLICATION_Digital_Input_Config_validate );
    HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( data->digital_out,
                                           HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT,
                                           HIL_APPLICATION_Digital_Output_Config_validate );
    HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( data->analog_in,
                                           HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT,
                                           HIL_APPLICATION_Analog_Input_Config_validate );
    HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( data->analog_out,
                                           HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT,
                                           HIL_APPLICATION_Analog_Output_Config_validate );
    HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( data->pwm_in, HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT,
                                           HIL_APPLICATION_Pwm_Input_Config_validate );
    HIL_APPLICATION_VALIDATE_CONFIG_ARRAY( data->pwm_out, HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT,
                                           HIL_APPLICATION_Pwm_Output_Config_validate );

#undef HIL_APPLICATION_VALIDATE_CONFIG_ARRAY

    for ( size_t i = 0u; i < HIL_APPLICATION_CAN_CHANNEL_COUNT; ++i )
    {
        status = HIL_APPLICATION_Can_Config_validate( context, &data->can[i] );
        if ( status != HIL_APPLICATION_STATUS_OK )
            return status;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_SPI_CHANNEL_COUNT; ++i )
    {
        status = HIL_APPLICATION_Spi_Config_validate( context, &data->spi[i] );
        if ( status != HIL_APPLICATION_STATUS_OK )
            return status;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_UART_CHANNEL_COUNT; ++i )
    {
        status = HIL_APPLICATION_Uart_Config_validate( context, &data->uart[i] );
        if ( status != HIL_APPLICATION_STATUS_OK )
            return status;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_I2C_CHANNEL_COUNT; ++i )
    {
        status = HIL_APPLICATION_I2c_Config_validate( context, &data->i2c[i] );
        if ( status != HIL_APPLICATION_STATUS_OK )
            return status;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Test_Instructions_validate( const HIL_Application_Context_T*          context,
                                            const HIL_Application_Test_Instruction_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( !HIL_APPLICATION_Boolean_Is_Valid( data->digital_outputs[i].high ) )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( !HIL_APPLICATION_Pwm_Value_Is_Valid( data->pwm_outputs[i].period_nanoseconds,
                                                  data->pwm_outputs[i].duty_cycle_permyriad ) )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }
    return HIL_APPLICATION_STATUS_OK;
}

/**
 * @brief Validate a 16-bit digital bank payload and ensure reserved bits are zero.
 *
 * @param[in] channel Logical bank index (must be 0).
 * @param[in] span    Payload byte span containing little-endian uint16_t mask.
 * @return Application status.
 */
static HIL_Application_Status_T
HIL_APPLICATION_Digital_Bank_Payload_validate( uint8_t                            channel,
                                               const HIL_Application_Byte_Span_T* span )
{
    if ( channel != 0u || span->size != 2u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    const uint16_t mask = HIL_APPLICATION_Read_U16_Le( span->data );
    return ( mask & HIL_APPLICATION_DIGITAL_BANK_RESERVED_MASK ) == 0u
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

/**
 * @brief Validate a 6-byte PWM payload (uint32 period_ns + uint16 duty_permyriad).
 *
 * @param[in] channel      Logical PWM channel index.
 * @param[in] max_channels Permitted channel bound.
 * @param[in] span         Payload byte span.
 * @return Application status.
 */
static HIL_Application_Status_T
HIL_APPLICATION_Pwm_Payload_validate( uint8_t channel, size_t max_channels,
                                      const HIL_Application_Byte_Span_T* span )
{
    if ( channel >= max_channels || span->size != 6u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    const uint32_t period_ns = HIL_APPLICATION_Read_U32_Le( &span->data[0] );
    const uint16_t duty      = HIL_APPLICATION_Read_U16_Le( &span->data[4] );
    return HIL_APPLICATION_Pwm_Value_Is_Valid( period_ns, duty )
               ? HIL_APPLICATION_STATUS_OK
               : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
}

/**
 * @brief Validate a CAN payload containing one or more 12-byte CAN frames.
 *
 * @param[in] channel Logical CAN channel index.
 * @param[in] span    Payload byte span.
 * @return Application status.
 */
static HIL_Application_Status_T
HIL_APPLICATION_Can_Payload_validate( uint8_t channel, const HIL_Application_Byte_Span_T* span )
{
    if ( channel >= HIL_APPLICATION_CAN_CHANNEL_COUNT
         || ( span->size % HIL_APPLICATION_CAN_FRAME_WIRE_SIZE ) != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    for ( size_t f = 0u; f < ( size_t )span->size; f += HIL_APPLICATION_CAN_FRAME_WIRE_SIZE )
    {
        const uint16_t can_id = HIL_APPLICATION_Read_U16_Le( &span->data[f] );
        const uint8_t  dlc    = span->data[f + 2u];
        const uint8_t  res    = span->data[f + 11u];
        if ( can_id > HIL_APPLICATION_CAN_MAX_STANDARD_ID || dlc > HIL_APPLICATION_CAN_MAX_DLC
             || res != 0u )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Update_Instruction_validate( const HIL_Application_Context_T*            context,
                                             const HIL_Application_Update_Instruction_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->flags > HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->operation_count == 0u || data->operations == NULL )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }

    /* Enforce no duplicate (peripheral_type, channel) pairs */
    for ( size_t i = 0u; i < ( size_t )data->operation_count; ++i )
    {
        for ( size_t j = i + 1u; j < ( size_t )data->operation_count; ++j )
        {
            if ( data->operations[i].peripheral_type == data->operations[j].peripheral_type
                 && data->operations[i].channel == data->operations[j].channel )
            {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
        }
    }

    for ( size_t i = 0u; i < ( size_t )data->operation_count; ++i )
    {
        const HIL_Application_Logical_Operation_T* op = &data->operations[i];

        if ( HIL_APPLICATION_Byte_Span_validate( &op->payload,
                                                 HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE )
                 != HIL_APPLICATION_STATUS_OK
             || op->payload.size == 0u )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }

        switch ( op->peripheral_type )
        {
            case HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT:
                if ( HIL_APPLICATION_Digital_Bank_Payload_validate( op->channel, &op->payload )
                     != HIL_APPLICATION_STATUS_OK )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT:
                if ( op->channel >= HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT
                     || op->payload.size != 4u )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT:
                if ( HIL_APPLICATION_Pwm_Payload_validate(
                         op->channel, HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT, &op->payload )
                     != HIL_APPLICATION_STATUS_OK )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_UART:
                if ( op->channel >= HIL_APPLICATION_UART_CHANNEL_COUNT )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_SPI: {
                if ( op->channel >= HIL_APPLICATION_SPI_CHANNEL_COUNT )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                const uint8_t num_packets = op->payload.data[0];
                if ( num_packets == 0u )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                if ( ( size_t )op->payload.size <= 1u + ( size_t )num_packets )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                const size_t expected_data_len =
                    ( size_t )op->payload.size - 1u - ( size_t )num_packets;
                size_t sum_packet_sizes = 0u;
                for ( size_t p = 0u; p < ( size_t )num_packets; ++p )
                {
                    const uint8_t pkt_len = op->payload.data[1u + p];
                    if ( pkt_len == 0u )
                    {
                        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                    }
                    sum_packet_sizes += pkt_len;
                }
                if ( sum_packet_sizes != expected_data_len )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            }
            case HIL_APPLICATION_PERIPHERAL_CAN:
                if ( HIL_APPLICATION_Can_Payload_validate( op->channel, &op->payload )
                     != HIL_APPLICATION_STATUS_OK )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            default:
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_validate(
    const HIL_Application_Context_T*                   context,
    const HIL_Application_Variable_Instruction_Data_T* data )
{
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    ( void )data;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Execution_Control_validate( const HIL_Application_Context_T*           context,
                                            const HIL_Application_Execution_Control_T* data )
{
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->command != HIL_APPLICATION_CONTROL_START
         && data->command != HIL_APPLICATION_CONTROL_ABORT )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    /* Execution Control flags are reserved and must remain zero. */
    if ( data->flags != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Global_Control_validate( const HIL_Application_Context_T*        context,
                                         const HIL_Application_Global_Control_T* data )
{
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->command != HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    /* Global Control flags are reserved and must remain zero. */
    if ( data->flags != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Test_Result_validate( const HIL_Application_Context_T*     context,
                                      const HIL_Application_Test_Result_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( !HIL_APPLICATION_Boolean_Is_Valid( data->digital_inputs[i].high ) )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }
    for ( size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( !HIL_APPLICATION_Pwm_Value_Is_Valid( data->pwm_inputs[i].period_nanoseconds,
                                                  data->pwm_inputs[i].duty_cycle_permyriad ) )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }
    if ( data->condition != HIL_APPLICATION_RESULT_CONDITION_OK
         && data->condition != HIL_APPLICATION_RESULT_CONDITION_PARTIAL
         && data->condition != HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Variable_Test_Result_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_Variable_Test_Result_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->flags > HIL_APPLICATION_RESULT_FLAG_HAS_MORE_CHUNKS )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->condition != HIL_APPLICATION_RESULT_CONDITION_OK
         && data->condition != HIL_APPLICATION_RESULT_CONDITION_PARTIAL
         && data->condition != HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->condition == HIL_APPLICATION_RESULT_CONDITION_OK && data->problem_detail != 0u )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->record_count != 0u && data->records == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    /* Enforce no duplicate (peripheral_type, channel) pairs */
    for ( size_t i = 0u; i < ( size_t )data->record_count; ++i )
    {
        for ( size_t j = i + 1u; j < ( size_t )data->record_count; ++j )
        {
            if ( data->records[i].peripheral_type == data->records[j].peripheral_type
                 && data->records[i].channel == data->records[j].channel )
            {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
        }
    }

    for ( size_t i = 0u; i < ( size_t )data->record_count; ++i )
    {
        const HIL_Application_Captured_Record_T* rec = &data->records[i];

        if ( HIL_APPLICATION_Byte_Span_validate( &rec->data,
                                                 HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE )
                 != HIL_APPLICATION_STATUS_OK
             || rec->data.size == 0u )
        {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }

        switch ( rec->peripheral_type )
        {
            case HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT:
                if ( HIL_APPLICATION_Digital_Bank_Payload_validate( rec->channel, &rec->data )
                     != HIL_APPLICATION_STATUS_OK )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT:
                if ( rec->channel >= HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT
                     || rec->data.size != 4u )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_PWM_INPUT:
                if ( HIL_APPLICATION_Pwm_Payload_validate(
                         rec->channel, HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT, &rec->data )
                     != HIL_APPLICATION_STATUS_OK )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_UART:
                if ( rec->channel >= HIL_APPLICATION_UART_CHANNEL_COUNT )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_SPI:
                if ( rec->channel >= HIL_APPLICATION_SPI_CHANNEL_COUNT )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            case HIL_APPLICATION_PERIPHERAL_CAN:
                if ( HIL_APPLICATION_Can_Payload_validate( rec->channel, &rec->data )
                     != HIL_APPLICATION_STATUS_OK )
                {
                    return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
                }
                break;
            default:
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
    }

    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Variable_Result_Data_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_Variable_Result_Data_T* data )
{
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    ( void )data;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Response_validate( const HIL_Application_Context_T*  context,
                                   const HIL_Application_Response_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->scope != HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION
         && data->scope != HIL_APPLICATION_RESPONSE_SCOPE_TICK
         && data->scope != HIL_APPLICATION_RESPONSE_SCOPE_COMPLETE_TEST
         && data->scope != HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL
         && data->scope != HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->outcome != HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED
         && data->outcome != HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED
         && data->outcome != HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED
         && data->outcome != HIL_APPLICATION_RESPONSE_OUTCOME_FAILED )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->reason < HIL_APPLICATION_RESPONSE_REASON_NONE
         || data->reason > HIL_APPLICATION_RESPONSE_REASON_INTERNAL_FAILURE )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->control_command != HIL_APPLICATION_CONTROL_INVALID
         && data->control_command != HIL_APPLICATION_CONTROL_START
         && data->control_command != HIL_APPLICATION_CONTROL_ABORT )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->global_control_command != HIL_APPLICATION_GLOBAL_CONTROL_INVALID
         && data->global_control_command != HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Error_validate( const HIL_Application_Context_T* context,
                                                         const HIL_Application_Error_T*   data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->category < HIL_APPLICATION_ERROR_CATEGORY_HARDWARE
         || data->category > HIL_APPLICATION_ERROR_CATEGORY_INTERNAL
         || !HIL_APPLICATION_Boolean_Is_Valid( data->recoverable )
         || !HIL_APPLICATION_Boolean_Is_Valid( data->has_tick_number ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->has_tick_number == 0u && data->tick_number != 0u )
    {
        return HIL_APPLICATION_STATUS_INCONSISTENT_TICK;
    }
    if ( data->has_tick_number == 1u
         && data->tick_number >= context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_INCONSISTENT_TICK;
    }
    return HIL_APPLICATION_Byte_Span_validate( &data->diagnostic_data,
                                               context->config.max_variable_data_size );
}
