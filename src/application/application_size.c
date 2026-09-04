/**
 * @file application_size.c
 * @brief Message-specific Application payload-size calculations.
 *
 * @details Sizes in this file exclude the common 23-byte envelope. The public
 * façade adds that envelope with checked arithmetic. Families intentionally
 * deferred from this foundation return NOT_IMPLEMENTED rather than deriving a
 * size from native C representation.
 */

#include "application_size.h"
#include "application_internal.h"
#include "application_encoding.h"
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    // System Information Request payload: one flag byte plus one query byte.
    *encoded_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    *encoded_size = HIL_APPLICATION_SYSTEM_INFO_RESPONSE_FIXED_ENCODE_SIZE
                    + data->firmware_git_hash.size + data->diagnostic_data.size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_size( const HIL_Application_Peripheral_Config_T* data,
                                        size_t*                                    size )
{
    /**
     * Current fixed configuration record widths are architecture-independent:
     * the type tag is one byte, a channel ID is three bytes (one-byte
     * peripheral plus little-endian uint16_t channel), and voltage-level enums
     * are one byte. PWM additionally carries a little-endian uint32_t period
     * and uint16_t duty cycle. Communication configuration sizing is deferred.
     */
    size_t size_local = HIL_APPLICATION_WIRE_ENUM_SIZE;
    switch ( data->type )
    {
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += HIL_APPLICATION_WIRE_ENUM_SIZE;
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += HIL_APPLICATION_WIRE_ENUM_SIZE;
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += HIL_APPLICATION_WIRE_U32_SIZE;
            size_local += HIL_APPLICATION_WIRE_U16_SIZE;
            size_local += HIL_APPLICATION_WIRE_ENUM_SIZE;
            break;
        /* Communication configuration wire sizing is deliberately deferred. */
        case HIL_APPLICATION_PERIPHERAL_CONFIG_RESERVED:
            return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
        default:
            return HIL_APPLICATION_STATUS_INTERNAL_ERROR;
    }
    *size = size_local;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T                    test_id,
    const HIL_Application_Variable_Instruction_Data_T* data, size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Execution_Control_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Global_Control_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Test_Result_size( const HIL_Application_Context_T*         context,
                                  const HIL_Application_Message_Subtype_T* sub_type,
                                  const HIL_Application_Test_Id_T          test_id,
                                  const HIL_Application_Test_Result_T* data, size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Result_Data_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Response_size( const HIL_Application_Context_T*         context,
                               const HIL_Application_Message_Subtype_T* sub_type,
                               const HIL_Application_Test_Id_T          test_id,
                               const HIL_Application_Response_T* data, size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}
