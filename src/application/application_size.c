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
#include "application_test_config_internal.h"
#include "application_encoding.h"
#include "application_validation.h"
#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"
#include "hil_rig_protocol/version.h"

HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    size_t* encoded_size )
{
    ( void )sub_type;
    ( void )test_id;
    if ( context == NULL || data == NULL || encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( data->application_protocol_major != HIL_RIG_PROTOCOL_VERSION_MAJOR
         || data->application_protocol_minor != HIL_RIG_PROTOCOL_VERSION_MINOR
         || data->application_protocol_patch != HIL_RIG_PROTOCOL_VERSION_PATCH )
    {
        return HIL_APPLICATION_STATUS_VERSION_MISMATCH;
    }
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_System_Info_Request_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    /* Request flag, query and an exact protocol version triplet. */
    *encoded_size = HIL_APPLICATION_SYSTEM_INFO_REQUEST_FIXED_ENCODE_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    size_t* encoded_size )
{
    ( void )sub_type;
    ( void )test_id;
    if ( context == NULL || data == NULL || encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( data->application_protocol_major != HIL_RIG_PROTOCOL_VERSION_MAJOR
         || data->application_protocol_minor != HIL_RIG_PROTOCOL_VERSION_MINOR
         || data->application_protocol_patch != HIL_RIG_PROTOCOL_VERSION_PATCH )
    {
        return HIL_APPLICATION_STATUS_VERSION_MISMATCH;
    }
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_System_Info_Response_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_SYSTEM_INFO_RESPONSE_FIXED_ENCODE_SIZE,
                                            ( size_t )data->firmware_git_hash.size
                                                + ( size_t )data->diagnostic_data.size,
                                            encoded_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    size_t* encoded_size )
{
    size_t total_size = 0u;
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( data == NULL || encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_TEST_CONFIG_FIXED_PAYLOAD_SIZE,
                                            ( size_t )data->extension_data.size, &total_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    *encoded_size = total_size;
    return HIL_APPLICATION_STATUS_OK;
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
    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

static HIL_Application_Status_T
HIL_APPLICATION_Aligned_Record_size( const HIL_Application_Byte_Span_T* span,
                                     size_t*                           running_total )
{
    if ( span->size != 0u && span->data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    const size_t pad              = HIL_APPLICATION_Align4_Padding( ( size_t )span->size );
    size_t       record_wire_size = 0u;
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_RECORD_HEADER_SIZE,
                                            ( size_t )span->size, &record_wire_size )
         || !HIL_APPLICATION_Checked_Add_Size( record_wire_size, pad, &record_wire_size )
         || !HIL_APPLICATION_Checked_Add_Size( *running_total, record_wire_size, running_total ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Update_Instruction_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Update_Instruction_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;
    if ( data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( data->operation_count != 0u && data->operations == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    size_t running_total = HIL_APPLICATION_UPDATE_INSTRUCTION_HEADER_SIZE;
    for ( size_t i = 0u; i < ( size_t )data->operation_count; ++i )
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Aligned_Record_size( &data->operations[i].payload, &running_total );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    *encoded_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
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
    ( void )sub_type;
    ( void )test_id;
    if ( data == NULL || encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Execution_Control_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    *encoded_size = HIL_APPLICATION_CONTROL_FIXED_ENCODE_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Global_Control_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    size_t* encoded_size )
{
    ( void )sub_type;
    ( void )test_id;
    if ( data == NULL || encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Global_Control_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    *encoded_size = HIL_APPLICATION_CONTROL_FIXED_ENCODE_SIZE;
    return HIL_APPLICATION_STATUS_OK;
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
    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Variable_Test_Result_size(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Test_Result_T* data,
    size_t* encoded_size )
{
    ( void )context;
    ( void )sub_type;
    ( void )test_id;

    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;
    if ( data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( data->record_count != 0u && data->records == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }

    size_t running_total = HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE;
    for ( size_t i = 0u; i < ( size_t )data->record_count; ++i )
    {
        const HIL_Application_Status_T status =
            HIL_APPLICATION_Aligned_Record_size( &data->records[i].data, &running_total );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    *encoded_size = running_total;
    return HIL_APPLICATION_STATUS_OK;
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

HIL_Application_Status_T HIL_APPLICATION_Response_size( const HIL_Application_Context_T*  context,
                                                        const HIL_Application_Response_T* data,
                                                        size_t* encoded_size )
{
    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;
    {
        const HIL_Application_Status_T status = HIL_APPLICATION_Response_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    *encoded_size = HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Error_size( const HIL_Application_Context_T* context,
                                                     const HIL_Application_Error_T*   data,
                                                     size_t*                          encoded_size )
{
    size_t total_size = 0u;
    if ( encoded_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;
    {
        const HIL_Application_Status_T status = HIL_APPLICATION_Error_validate( context, data );
        if ( status != HIL_APPLICATION_STATUS_OK )
        {
            return status;
        }
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE,
                                            data->diagnostic_data.size, &total_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    *encoded_size = total_size;
    return HIL_APPLICATION_STATUS_OK;
}
