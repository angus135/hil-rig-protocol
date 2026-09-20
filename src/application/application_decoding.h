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
 * classified differently by the two public decode paths. Test Instruction and
 * Test Result widths come from the shared private 50-byte and 39-byte constants.
 *
 * @param[in] type         Parsed Application message type.
 * @param[in] payload_size Declared payload extent in bytes.
 * @return OK for an exact fixed width, MALFORMED_MESSAGE for a width mismatch,
 *         or INVALID_ARGUMENT when the selected family is variable-size here.
 */
HIL_Application_Status_T
HIL_APPLICATION_Fixed_Body_Validate_Size( HIL_Application_Message_Type_T type,
                                          size_t                         payload_size );

/**
 * @brief Scan a bounded System Information Response payload without allocating.
 *
 * @details The scanner validates six numeric fields and both length-prefixed
 * spans, requires exact payload consumption, applies the configured per-span
 * limit, and reports the combined decoded span storage requirement.
 */
HIL_Application_Status_T
HIL_APPLICATION_System_Info_Response_Scan( const HIL_Application_Context_T* context,
                                           const uint8_t* payload, size_t payload_size,
                                           size_t* decoded_storage_size );

/** Scan one Error body, proving its exact declared diagnostic extent before policy checks. */
HIL_Application_Status_T HIL_APPLICATION_Error_Scan( const HIL_Application_Context_T* context,
                                                     const uint8_t* payload, size_t payload_size,
                                                     size_t* decoded_storage_size );

/**
 * @brief Scan an Update Instruction payload without allocating.
 *
 * @details Validates the 8-byte header, reserved fields, and the 4-byte-aligned
 * TLV record sequence for all operations, and computes the required decode storage.
 *
 * @param[in]  context              Application context.
 * @param[in]  payload              First byte of the declared payload.
 * @param[in]  payload_size         Declared payload extent in bytes.
 * @param[out] decoded_storage_size Required decoded storage in bytes.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Update_Instruction_Scan( const HIL_Application_Context_T* context,
                                         const uint8_t* payload, size_t payload_size,
                                         size_t* decoded_storage_size );

/**
 * @brief Scan a Variable Test Result payload without allocating.
 *
 * @details Validates the 12-byte header, condition, flags, reserved fields, and
 * the 4-byte-aligned TLV record sequence for all captured records, and computes
 * the required decode storage.
 *
 * @param[in]  context              Application context.
 * @param[in]  payload              First byte of the declared payload.
 * @param[in]  payload_size         Declared payload extent in bytes.
 * @param[out] decoded_storage_size Required decoded storage in bytes.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Variable_Test_Result_Scan( const HIL_Application_Context_T* context,
                                           const uint8_t* payload, size_t payload_size,
                                           size_t* decoded_storage_size );

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
 * @brief Decode the complete Test Configuration payload and extension span.
 * @details Decodes all fixed configuration arrays from explicit wire fields.
 * Structural configuration semantics are validated by the existing façade after decoding.
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

/** Validate one encoded Test Configuration without copying extension bytes. */
HIL_Application_Status_T
HIL_APPLICATION_Test_Configuration_Encoded_Validate( const HIL_Application_Context_T* context,
                                                     const uint8_t* payload, size_t payload_size,
                                                     size_t* decoded_storage_size );

/**
 * @brief Decode the exact 50-byte fixed Test Instruction payload.
 * @details The exact width is checked before any field is read and payload_size
 * is published only after all 50 bytes have been consumed.
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
 * @brief Decode an Update Instruction payload and its logical operations.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Caller storage for operations array and payload bytes.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Bytes used in decoded_data on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Update_Instruction_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Update_Instruction_T* data,
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

/** Decode the four-byte Finalize Test Upload payload. */
HIL_Application_Status_T HIL_APPLICATION_Finalize_Test_Upload_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Finalize_Test_Upload_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the exact 39-byte fixed Test Result payload.
 * @details The exact width is checked before any field is read and payload_size
 * is published only after all 39 bytes have been consumed.
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
 * @brief Decode a Variable Test Result payload and its captured records.
 * @param[in]  context               Application context.
 * @param[in]  sub_type              Parsed message subtype.
 * @param[in]  test_id               Parsed Test ID value.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent in bytes.
 * @param[out] payload_size          Number of payload bytes consumed on success.
 * @param[out] decoded_data          Caller storage for records array and data bytes.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Bytes used in decoded_data on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Test_Result_decode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, HIL_Application_Variable_Test_Result_T* data,
    const uint8_t* payload, size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
    size_t max_decoded_data_size, size_t* used_decoded_size );

/**
 * @brief Decode the fixed Application Response body representation.
 * @details The body is fixed-width and uses no decode storage.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent.
 * @param[out] payload_size          Payload bytes consumed on body-decode success.
 * @param[out] used_decoded_size     Zero for the current fixed body.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Response_decode( HIL_Application_Response_T* data,
                                                          const uint8_t*              payload,
                                                          size_t  max_payload_size,
                                                          size_t* payload_size,
                                                          size_t* used_decoded_size );

/**
 * @brief Decode an Application Error body and diagnostic byte span.
 * @param[in]  context               Application context.
 * @param[out] data                  Typed body destination.
 * @param[in]  payload               First byte of the declared payload.
 * @param[in]  max_payload_size      Declared payload extent.
 * @param[out] payload_size          Payload bytes consumed on body-decode success.
 * @param[out] decoded_data          Caller storage for diagnostic bytes.
 * @param[in]  max_decoded_data_size Available decoded_data capacity.
 * @param[out] used_decoded_size     Diagnostic bytes stored on success.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Error_decode( const HIL_Application_Context_T* context,
                              HIL_Application_Error_T* data, const uint8_t* payload,
                              size_t max_payload_size, size_t* payload_size, uint8_t* decoded_data,
                              size_t max_decoded_data_size, size_t* used_decoded_size );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_DECODING_INTERNAL_H */
