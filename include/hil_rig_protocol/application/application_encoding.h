

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
 * @brief Encode a byte span into a payload.
 *
 * @param[in]  data    Byte span to encode.
 * @param[out] payload Destination payload buffer.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Byte_Span_encode( const HIL_Application_Byte_Span_T* data,
                                                           uint8_t* payload );

/**
 * @brief Encode a system information request.
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
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode a system information response.
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
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode a test configuration.
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
HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode test instruction data.
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
HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode variable instruction data.
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
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T                    test_id,
    const HIL_Application_Variable_Instruction_Data_T* data, uint32_t max_payload_size,
    uint8_t* payload, size_t* used_size );

/**
 * @brief Encode execution control data.
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
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode global control data.
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
HIL_Application_Status_T HIL_APPLICATION_Global_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode test result data.
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
HIL_Application_Status_T HIL_APPLICATION_Test_Result_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode variable result data.
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
HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Result_Data_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode a response.
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
HIL_Application_Status_T HIL_APPLICATION_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Response_T* data,
    uint32_t max_payload_size, uint8_t* payload, size_t* used_size );

#ifdef __cplusplus
}
#endif
