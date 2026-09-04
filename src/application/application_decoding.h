/**
 * @file application_decoding.h
 * @brief Private declarations for Application payload decoders.
 *
 * @details The public façade has already parsed and bounded the 23-byte common
 * envelope before calling these helpers. max_payload_size is therefore the
 * exact declared payload extent for normal top-level decoding, not the complete
 * encoded-message size. A helper must never read beyond that extent and must
 * report exactly the number of payload bytes it consumed.
 *
 * Missing bytes inside a declared payload are malformed encoded input and use
 * HIL_APPLICATION_STATUS_MALFORMED_MESSAGE. BUFFER_TOO_SMALL is reserved for
 * insufficient caller-provided decoded_data storage.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_DECODING_INTERNAL_H
#define HIL_RIG_PROTOCOL_APPLICATION_DECODING_INTERNAL_H

#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_message.h"
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
 * @brief Validate the exact payload width for a currently fixed-size message family.
 *
 * @details This structural check is shared by Decode_Storage_Size() and the
 * fixed body decoders so malformed undersized or oversized payloads cannot be
 * classified differently by the two public decode paths.
 *
 * @param[in] type         Parsed Application message type.
 * @param[in] payload_size Declared payload extent in bytes.
 * @return OK for an exact fixed width, MALFORMED_MESSAGE for a width mismatch,
 *         or NOT_IMPLEMENTED when the selected family is not fixed-size here.
 */
HIL_Application_Status_T
HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_Application_Message_Type_T type,
                                          size_t                         payload_size );

/**
 * @brief Decode the fixed System Information Request payload.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Unused for this fixed body.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Zero for this fixed body.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Request_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode a System Information Response payload and its byte spans.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Caller storage for decoded byte-span contents.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Bytes used in decoded_data on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_System_Info_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the current fixed Test Configuration payload and extension span.
 * @details Tick duration is decoded directly as little-endian microseconds.
 * Detailed configuration semantics are validated separately and remain partial.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Caller storage for extension-data bytes.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Extension-data bytes stored on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Configuration_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the current 50-byte fixed Test Instruction payload.
 * @details Variable instruction declarations/data are deliberately not part of
 * this fixed decoder yet.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Parsed message subtype.
 * @param[in]  test_id          Parsed Test ID value.
 * @param[out] data             Typed body destination.
 * @param[in]  payload          First byte of the declared payload.
 * @param[in]  max_payload_size Declared payload extent in bytes.
 * @param[out] payload_size     Number of payload bytes consumed on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Instruction_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size );

/**
 * @brief Preserve the Variable Instruction Data decoder entry point.
 * @details Runtime decoding remains deliberately NOT_IMPLEMENTED.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               Declared payload bytes.
 * @param[in]  max_payload_size      Declared payload extent.
 * @param[out] payload_size          Payload bytes consumed if implemented.
 * @param[out] decoded_data          Caller decoded-data storage.
 * @param[in]  max_decoded_data_size Available decoded-data capacity.
 * @param[out] used_decoded_size     Decoded-data bytes used if implemented.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Variable instruction decoding is deferred.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Instruction_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the five-byte Execution Control payload.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Unused for this fixed body.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Zero for this fixed body.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Execution_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the five-byte Global Control payload.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Unused for this fixed body.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Zero for this fixed body.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Global_Control_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Global_Control_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the current 39-byte fixed Test Result payload.
 * @details Variable result declarations/data are deliberately not part of this
 * fixed decoder yet.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Parsed message subtype.
 * @param[in]  test_id          Parsed Test ID value.
 * @param[out] data             Typed body destination.
 * @param[in]  payload          First byte of the declared payload.
 * @param[in]  max_payload_size Declared payload extent in bytes.
 * @param[out] payload_size     Number of payload bytes consumed on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size );

/**
 * @brief Preserve the Variable Result Data decoder entry point.
 * @details Runtime decoding remains deliberately NOT_IMPLEMENTED.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               Declared payload bytes.
 * @param[in]  max_payload_size      Declared payload extent.
 * @param[out] payload_size          Payload bytes consumed if implemented.
 * @param[out] decoded_data          Caller decoded-data storage.
 * @param[in]  max_decoded_data_size Available decoded-data capacity.
 * @param[out] used_decoded_size     Decoded-data bytes used if implemented.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Variable result decoding is deferred.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Result_Data_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the existing fixed Application Response body representation.
 * @details Public typed Response validation remains deliberately NOT_IMPLEMENTED,
 * so successful body parsing does not make the family fully supported.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent.
 * @param[out] payload_size          Payload bytes consumed on body-decode success.
 * @param[out] decoded_data          Unused by the current fixed body.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Zero for the current fixed body.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Response_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Response_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the existing Application Error body and diagnostic byte span.
 * @details Public Error validation/storage sizing remains deliberately
 * unfinished, so this body decoder is not a statement that the family is fully
 * supported.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent.
 * @param[out] payload_size          Payload bytes consumed on body-decode success.
 * @param[out] decoded_data          Caller storage for diagnostic bytes.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Diagnostic bytes stored on success.
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

#endif /* HIL_RIG_PROTOCOL_APPLICATION_DECODING_INTERNAL_H */
