/**
 * @file application_validation.c
 * @brief Structural validation for typed Application message bodies.
 *
 * @details This file implements only rules owned by the stateless codec.
 * Stateful transaction ordering, hardware capability checks, variable-data
 * declaration/correlation rules, and Response/Error semantics remain deferred.
 */

#include "hil_rig_protocol/application/application_message.h"
#include "application_size.h"
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"
#include "hil_rig_protocol/version.h"

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
    if ( !HIL_APPLICATION_Boolean_Is_Valid( data->enabled )
         || !HIL_APPLICATION_Boolean_Is_Valid( data->termination_enabled ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->enabled == 0u )
    {
        return data->bit_rate == 0u && data->termination_enabled == 0u
                       && data->capture_limit_bytes == 0u
                   ? HIL_APPLICATION_STATUS_OK
                   : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->bit_rate == 0u
         || data->capture_limit_bytes > ( uint32_t )context->config.max_variable_data_size )
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
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->request_firmware_git_hash > 1u
         || data->query != HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC )
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

    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    /* Typed diagnostics must match the repository protocol version encoded by this codec. */
    if ( data->application_protocol_major != HIL_RIG_PROTOCOL_VERSION_MAJOR
         || data->application_protocol_minor != HIL_RIG_PROTOCOL_VERSION_MINOR
         || data->application_protocol_patch != HIL_RIG_PROTOCOL_VERSION_PATCH )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    status = HIL_APPLICATION_Byte_Span_validate( &data->firmware_git_hash,
                                                 HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    return HIL_APPLICATION_Byte_Span_validate( &data->diagnostic_data,
                                               HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE );
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
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    ( void )data;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}
