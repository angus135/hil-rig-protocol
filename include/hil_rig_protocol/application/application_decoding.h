

#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief decode a system information request.
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
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Request_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a system information response.
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
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a test configuration.
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
HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Configuration_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size,
    HIL_Application_Peripheral_Config_T* decoded_peripherals, size_t max_decoded_peripherals_num,
    uint8_t* decoded_data, size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode test instruction data.
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
HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Instruction_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size,
    HIL_Application_Data_Declaration_T* decoded_variable_data, size_t max_decoded_variable_data_num,
    size_t* used_devoded_variable_num );

/**
 * @brief decode variable instruction data.
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
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Instruction_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode execution control data.
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
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Execution_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode global control data.
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
HIL_Application_Status_T HIL_APPLICATION_Global_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Global_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode test result data.
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
HIL_Application_Status_T HIL_APPLICATION_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_siz,
    HIL_Application_Data_Declaration_T* decoded_variable_data, size_t max_decoded_variable_data_num,
    size_t* used_decoded_variable_nume );

/**
 * @brief decode variable result data.
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
HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Result_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a response.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[in]  data             Response data.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload          Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

HIL_Application_Status_T HIL_APPLICATION_Error_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Error_T* data, const uint8_t* payload,
    size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

#ifdef __cplusplus
}
#endif
