
#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/application/application_size.h"
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



HIL_Application_Status_T HIL_APPLICATION_peripheral_config_validate(const HIL_Application_Peripheral_Config_T* peripheral)
{
    switch (peripheral->type) {
        /** Invalid sentinel. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
        /** Digital input/output electrical configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            if (peripheral->value.digital.channel.peripheral != HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT && peripheral->value.digital.channel.peripheral != HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT ) {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
            }
            break;
        /** Analogue input/output range configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            if (peripheral->value.analog.channel.peripheral != HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT && peripheral->value.analog.channel.peripheral != HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT ) {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
            }
            break;
        /** PWM generation or capture configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            if (peripheral->value.pwm.channel.peripheral != HIL_APPLICATION_PERIPHERAL_PWM_INPUT && peripheral->value.pwm.channel.peripheral != HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT ) {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
            }
            break;
        /** UART/SPI/I2C/CAN communication and capture configuration. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:
            if (peripheral->value.communication.channel.peripheral != HIL_APPLICATION_PERIPHERAL_UART && peripheral->value.communication.channel.peripheral != HIL_APPLICATION_PERIPHERAL_CAN && peripheral->value.communication.channel.peripheral != HIL_APPLICATION_PERIPHERAL_SPI && peripheral->value.communication.channel.peripheral != HIL_APPLICATION_PERIPHERAL_I2C) {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
            }
            if (peripheral->value.communication.channel.channel > HIL_APPLICATION_UART_CHANNEL_COUNT) {
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
            }
            if (peripheral->value.communication.capture_limit_bytes > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE){
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED; 
            }
            break;
        /** Reserved sentinel. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED:
            return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE; 
        default: 
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED; 
    }
    return HIL_APPLICATION_STATUS_OK;
}
/**
 * @brief validate a system information request.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             System information request data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_validate(
    const HIL_Application_Context_T* context, const HIL_Application_System_Info_Request_T* data){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }

        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate a system information response.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             System information response data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_validate(
    const HIL_Application_Context_T* context, const HIL_Application_System_Info_Response_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate a test configuration.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Test configuration data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Test_Configuration_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        if (data->peripheral_count > HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT){
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->expected_tick_count > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT){
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        uint8_t check = 0;
        uint32_t periods[] = HIL_APPLICATION_VALID_TICK_PERIODS_NS;
        for (uint32_t i=0; i<sizeof(periods); i++){
            if (periods[i] == data->tick_duration.nanoseconds){
                check = 1;
            }
        }
        if (check == 0){
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        for (uint8_t i=0; i<data->peripheral_count;i++){
            if (HIL_APPLICATION_peripheral_config_validate(&(data->peripherals[i])) != HIL_APPLICATION_STATUS_OK){
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
        }
        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate test instruction data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Test instruction data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Test_Instruction_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        if (data->tick_number > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->variable_data_count > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        for (uint8_t i=0; i<data->variable_data_count;i++){
            if (data->variable_data[i].channel.peripheral == HIL_APPLICATION_PERIPHERAL_INVALID){
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
            if (data->variable_data[i].channel.channel > HIL_APPLICATION_UART_CHANNEL_COUNT){
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
        }
        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate variable instruction data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Variable instruction data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_validate(
    const HIL_Application_Context_T* context,
    const HIL_Application_Variable_Instruction_Data_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        (void)data;
        return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
    }

/**
 * @brief validate execution control data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Execution control data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Execution_Control_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        if (data->command == HIL_APPLICATION_CONTROL_INVALID){
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate global control data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Global control data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Global_Control_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Global_Control_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        if (data->command == HIL_APPLICATION_GLOBAL_CONTROL_INVALID){
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate test result data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Test result data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Result_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Test_Result_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        if (data->tick_number > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->variable_data_count > HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        for (uint8_t i=0; i<data->variable_data_count;i++){
            if (data->variable_data[i].channel.peripheral == HIL_APPLICATION_PERIPHERAL_INVALID){
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
            if (data->variable_data[i].channel.channel > HIL_APPLICATION_UART_CHANNEL_COUNT){
                return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
            }
        }
        return HIL_APPLICATION_STATUS_OK;
    }

/**
 * @brief validate variable result data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Variable result data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload           Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Variable_Result_Data_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        (void) data;
        return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
    }

/**
 * @brief validate variable result data.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Variable result data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload           Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Response_validate(
    const HIL_Application_Context_T* context, const HIL_Application_Response_T* data ){
        if (context->initialized == 0){
            return HIL_APPLICATION_STATUS_UNINITIALIZED;
        }
        if (data->tick_number > HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->scope == HIL_APPLICATION_RESPONSE_SCOPE_INVALID) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->outcome == HIL_APPLICATION_RESPONSE_OUTCOME_INVALID) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->control_command == HIL_APPLICATION_CONTROL_INVALID) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        if (data->global_control_command == HIL_APPLICATION_GLOBAL_CONTROL_INVALID) {
            return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        }
        return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
    }

