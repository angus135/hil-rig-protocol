

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
 * @brief decode a varaible instruction.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Request_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a varaible instruction.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a varaible instruction.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
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
 * @brief decode a test instruction
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 * @param[out] decoded_variable_data          A buffer of data structs for varialbe data.
 * @param[in] max_decoded_variable_data_num       Number of data structs available within
 * decoded_data.
 * @param[out] used_decoded_variable_nume          Number of datastructs used in decoded_data.
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
 * @brief decode a varaible instruction.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Instruction_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode an execution control.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Execution_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a global control.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Global_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Global_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode a result.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 * @param[out] decoded_variable_data          A buffer of data structs for varialbe data.
 * @param[in] max_decoded_variable_data_num       Number of data structs available within
 * decoded_data.
 * @param[out] used_decoded_variable_nume          Number of datastructs used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size,
    HIL_Application_Data_Declaration_T* decoded_variable_data, size_t max_decoded_variable_data_num,
    size_t* used_decoded_variable_nume );

/**
 * @brief decode a varaible result.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
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
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief decode an error.
 *
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Test ID.
 * @param[out]  data             Response data (message we are converting to).
 * @param[in] payload          The encoded data being recieved from the wire.
 * @param[in]  max_payload_size Maximum available payload size in bytes.
 * @param[out] payload_size          The actual number of bytes decoded from the payload.
 * @param[out] decoded_data          The buffer for varialbe data (bytespans).
 * @param[in] max_decoded_data_size       Number of bytes available within decoded_data.
 * @param[out] used_decoded_size          Number of bytes used in decoded_data.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Error_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Error_T* data, const uint8_t* payload,
    size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

#ifdef __cplusplus
}
#endif
