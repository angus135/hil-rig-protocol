
#include "hil_rig_protocol/application/application_size.h"
#include "hil_rig_protocol/application/application_encoding.h"
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

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    uint32_t* encoded_size )
{
    // The message will be the application header and the HIL_Application_System_Info_Request_T
    *encoded_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    uint32_t* encoded_size )
{
    *encoded_size = HIL_APPLICATION_SYSTEM_INFO_RESPONSE_FIXED_ENCODE_SIZE
                    + data->firmware_git_hash.size + data->diagnostic_data.size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T
HIL_APPLICATION_Peripheral_Config_size( const HIL_Application_Peripheral_Config_T* data,
                                        uint32_t*                                  size )
{
    /**
    Payload:
    _______________________________________________________
    |                         |                            |
    |         type {4}        |         value {Z}          |
    |_________________________|____________________________|
    Value is a Union of 4 different types:
    Digital :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |       output mV {4}        |
    |_________________________|____________________________|
    |                         |                            |
    |      input mV {4}       |   initial output high {1}  |
    |_________________________|____________________________|
    |                         |
    |     capture en {1}      |
    |_________________________|
    Analog :
    _______________________________________________________
    |                         |                            |
    |      channel {6}        |       minimum mV {4}       |
    |_________________________|____________________________|
    |                         |
    |      maximum mV {4}     |
    |_________________________|
    PWM :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |        period nS {4}       |
    |_________________________|____________________________|
    |                         |                            |
    |initial duty cycle pm {2}|     capture enabled {1}    |
    |_________________________|____________________________|
    Communication :
    _______________________________________________________
    |                         |                            |
    |       channel {6}       |      bit rate bps {4}      |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   capture limit bytes {4}  |
    |_________________________|____________________________|

    */
    uint32_t size_local = 0;
    switch ( data->type )
    {
        case HIL_APPLICATION_PERIPHERAL_CONFIG_INVALID:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += sizeof( data->value.digital.output_millivolts );
            size_local += sizeof( data->value.digital.input_threshold_millivolts );
            size_local += sizeof( data->value.digital.initial_output_high );
            size_local += sizeof( data->value.digital.capture_enabled );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += sizeof( data->value.analog.minimum_microvolts );
            size_local += sizeof( data->value.analog.maximum_microvolts );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_PWM:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += sizeof( data->value.pwm.period_nanoseconds );
            size_local += sizeof( data->value.pwm.initial_duty_cycle_permyriad );
            size_local += sizeof( data->value.pwm.capture_enabled );
            break;
        case HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION:
            size_local += HIL_APPLICATION_CHANNEL_ID_ENCODE_SIZE;
            size_local += sizeof( data->value.communication.bit_rate );
            size_local += sizeof( data->value.communication.flags );
            size_local += sizeof( data->value.communication.capture_limit_bytes );
            break;
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
    uint32_t* encoded_size )
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
    uint32_t* encoded_size )
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
    const HIL_Application_Variable_Instruction_Data_T* data, uint32_t* encoded_size )
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
    uint32_t* encoded_size )
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
    uint32_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Result_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    uint32_t* encoded_size )
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
    uint32_t* encoded_size )
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
                               const HIL_Application_Response_T* data, uint32_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;
    ( void )data;
    ( void )encoded_size;
    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}