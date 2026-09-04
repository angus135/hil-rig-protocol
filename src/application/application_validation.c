/**
 * @file application_validation.c
 * @brief Structural validation for typed Application message bodies.
 *
 * @details This file implements only rules owned by the stateless codec.
 * Stateful transaction ordering, hardware capability checks, detailed fixed-I/O
 * semantics, and unfinished variable/Response/Error semantics remain deferred.
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

HIL_Application_Status_T
HIL_APPLICATION_peripheral_config_validate( const HIL_Application_Peripheral_Config_T* peripheral )
{
    switch ( peripheral->type )
    {
        /** Invalid sentinel. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        /** Digital input/output electrical configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            if ( peripheral->value.digital.channel.peripheral
                     != HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT
                 && peripheral->value.digital.channel.peripheral
                        != HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT )
            {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
            break;
        /** Analogue input/output range configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            if ( peripheral->value.analog.channel.peripheral
                     != HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT
                 && peripheral->value.analog.channel.peripheral
                        != HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT )
            {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
            break;
        /** PWM generation or capture configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            if ( peripheral->value.pwm.channel.peripheral != HIL_APPLICATION_PERIPHERAL_PWM_INPUT
                 && peripheral->value.pwm.channel.peripheral
                        != HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT )
            {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
            break;
        /** UART/SPI/I2C/CAN communication and capture configuration. */
        // case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:
        //     if ( peripheral->value.communication.channel.peripheral
        //              != HIL_APPLICATION_PERIPHERAL_UART
        //          && peripheral->value.communication.channel.peripheral
        //                 != HIL_APPLICATION_PERIPHERAL_CAN
        //          && peripheral->value.communication.channel.peripheral
        //                 != HIL_APPLICATION_PERIPHERAL_SPI
        //          && peripheral->value.communication.channel.peripheral
        //                 != HIL_APPLICATION_PERIPHERAL_I2C )
        //     {
        //         return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        //     }
        //     if ( peripheral->value.communication.channel.channel
        //          > HIL_APPLICATION_UART_CHANNEL_COUNT )
        //     {
        //         return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        //     }
        //     if ( peripheral->value.communication.capture_limit_bytes
        //          > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE )
        //     {
        //         return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        //     }
        //     break;
        /** Reserved sentinel. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
        default:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
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
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    /* This is a structural policy bound; the stateless codec does not retain uploaded ticks. */
    if ( data->expected_tick_count > context->config.max_expected_tick_count )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    /* Tick duration is carried directly on the wire in microseconds. */
    uint8_t               check              = 0;
    static const uint32_t valid_periods_us[] = HIL_APPLICATION_VALID_TICK_PERIODS_US;
    for ( size_t i = 0u; i < sizeof( valid_periods_us ) / sizeof( valid_periods_us[0] ); ++i )
    {
        if ( valid_periods_us[i] == data->tick_duration_us.microseconds )
        {
            check = 1;
        }
    }
    if ( check == 0 )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( HIL_APPLICATION_Byte_Span_validate( &data->extension_data,
                                             HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE )
         != HIL_APPLICATION_STATUS_OK )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Test_Instructions_validate( const HIL_Application_Context_T*          context,
                                            const HIL_Application_Test_Instruction_T* data )
{
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->tick_number > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    /* Variable declaration count/channel semantics are deliberately deferred. */
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
    if ( context->initialized == 0 )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->tick_number > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    if ( data->condition != HIL_APPLICATION_RESULT_CONDITION_OK
         && data->condition != HIL_APPLICATION_RESULT_CONDITION_PARTIAL
         && data->condition != HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    /* Variable declaration count/channel semantics are deliberately deferred. */
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
