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

#define HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE 5
#define HIL_APPLICATION_SYSTEM_INFO_RESPONSE_FIXED_ENCODE_SIZE 10
#define HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE 6

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_size( const HIL_Application_Peripheral_Config_T* data,
                                        uint32_t*                                  size );

/**
 * @brief Determine the encoded size of a system information request.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          System information request data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of a system information response.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          System information response data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of a test configuration.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Test configuration data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of test instructions.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Test instruction data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of variable instruction data.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Variable instruction data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T                    test_id,
    const HIL_Application_Variable_Instruction_Data_T* data, uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of execution control data.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Execution control data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of global control data.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Global control data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Global_Control_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of a test result.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Test result data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Result_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of variable result data.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Variable result data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Result_Data_T* data,
    uint32_t* encoded_size );

/**
 * @brief Determine the encoded size of a response.
 *
 * @param[in]  context       Application context.
 * @param[in]  sub_type      Message subtype.
 * @param[in]  test_id       Test ID.
 * @param[in]  data          Response data.
 * @param[out] encoded_size  Size of the encoded message in bytes.
 *
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Response_size( const HIL_Application_Context_T*         context,
                               const HIL_Application_Message_Subtype_T* sub_type,
                               const HIL_Application_Test_Id_T          test_id,
                               const HIL_Application_Response_T* data, uint32_t* encoded_size );

#ifdef __cplusplus
}
#endif