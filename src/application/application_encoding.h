/**
 * @file application_encoding.h
 * @brief Private declarations for Application payload encoders.
 *
 * @details The public façade validates the common typed envelope, encodes the
 * 23-byte common header, and then calls one selected payload encoder with only
 * the remaining payload capacity. These helpers encode explicit fixed-width
 * fields and report exactly how many payload bytes they wrote. They never
 * encode the common envelope or a payload-end marker.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_ENCODING_INTERNAL_H
#define HIL_RIG_PROTOCOL_APPLICATION_ENCODING_INTERNAL_H

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
 * @brief Encode one length-delimited byte span.
 * @details Wire form is one unsigned length byte followed by exactly data->size bytes.
 * @param[in]  data             Span to encode; data->data may be NULL only when size is zero.
 * @param[out] payload          Destination payload bytes.
 * @param[in]  payload_capacity Available destination capacity.
 * @param[out] used_size        Bytes written on success; zero on failure where documented.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Byte_Span_encode( const HIL_Application_Byte_Span_T* data,
                                                           uint8_t* payload,
                                                           size_t   payload_capacity,
                                                           size_t*  used_size );

/**
 * @brief Encode the two-byte System Information Request payload.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed request body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Request_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Request_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the existing System Information Response payload.
 * @details Canonical compiled repository protocol version fields are written,
 * followed by diagnostic-data and firmware-Git-hash byte spans.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed response body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_System_Info_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_System_Info_Response_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the complete Test Configuration payload.
 * @details Encodes the 197-byte fixed portion, all fixed I/O and communication
 * arrays, and the one-byte-length-prefixed extension using explicit wire widths.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Test Configuration body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Configuration_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Configuration_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the exact 50-byte fixed Test Instruction payload.
 * @details Variable instruction declarations/data remain deliberately deferred.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Test Instruction body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Instructions_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Instruction_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Preserve the Variable Instruction Data encoder entry point.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed variable instruction body.
 * @param[in]  max_payload_size Available payload capacity.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Set to zero by the current stub when non-NULL.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Variable instruction encoding is deferred.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T                    test_id,
    const HIL_Application_Variable_Instruction_Data_T* data, size_t max_payload_size,
    uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the five-byte Execution Control payload.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Execution Control body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Execution_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Execution_Control_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the five-byte Global Control payload.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Global Control body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Global_Control_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Global_Control_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the exact 39-byte fixed Test Result payload.
 * @details Variable result declarations/data remain deliberately deferred.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype selected by the public envelope.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Test Result body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Test_Result_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Test_Result_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Preserve the Variable Result Data encoder entry point.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed variable result body.
 * @param[in]  max_payload_size Available payload capacity.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload-size output reserved for future implementation.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Variable result encoding is deferred.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Result_Data_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Variable_Result_Data_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the existing fixed Application Response body representation.
 * @details Public Response validation remains deliberately NOT_IMPLEMENTED.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Response body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on body-encode success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Response_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Response_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

/**
 * @brief Encode the existing Application Error body representation.
 * @details Public Error validation/sizing remains deliberately unfinished.
 * @param[in]  context          Application context.
 * @param[in]  sub_type         Message subtype.
 * @param[in]  test_id          Envelope Test ID value.
 * @param[in]  data             Typed Error body.
 * @param[in]  max_payload_size Available payload capacity after the common header.
 * @param[out] payload          Destination payload buffer.
 * @param[out] used_size        Payload bytes written on body-encode success.
 * @return Application status.
 */
HIL_Application_Status_T HIL_APPLICATION_Error_encode(
    const HIL_Application_Context_T* context, const HIL_Application_Message_Subtype_T* sub_type,
    const HIL_Application_Test_Id_T test_id, const HIL_Application_Error_T* data,
    size_t max_payload_size, uint8_t* payload, size_t* used_size );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_ENCODING_INTERNAL_H */
