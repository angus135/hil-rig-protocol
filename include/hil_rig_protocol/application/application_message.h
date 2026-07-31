/**
 * @file application_message.h
 * @brief Common tagged Application message envelope and typed body union.
 *
 * @details Callers construct and receive this C-compatible representation
 * rather than reinterpreting raw body bytes. It is an API structure, not a
 * packed wire header. A future codec explicitly serializes approved fields.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_MESSAGE_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_MESSAGE_H

#include <stdint.h>

#include "hil_rig_protocol/application/application_control.h"
#include "hil_rig_protocol/application/application_error.h"
#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Semantic family of one complete Application message.
 *
 * @details Numeric assignments are explicit because they are intended to
 * become stable wire identifiers. Changing an assigned value after publication
 * may break firmware/Python compatibility and requires protocol-version review.
 * There is deliberately no Application sequence field: Transport supplies
 * reliable ordered delivery, while test ID/tick/scope correlate semantics.
 */
typedef enum
{
    /** Invalid sentinel; also used to clear failed decode output. */
    HIL_APPLICATION_MESSAGE_TYPE_INVALID = 0,
    /** Host request for minimal version/diagnostic information. */
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST = 1,
    /** HIL-RIG response with minimal version/diagnostic information. */
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE = 2,
    /** Host-to-HIL-RIG test-wide configuration. */
    HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION = 16,
    /** Host-to-HIL-RIG fixed instruction for one tick. */
    HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION = 17,
    /** Host-to-HIL-RIG variable channel bytes for one tick. */
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA = 18,
    /** Host-to-HIL-RIG test-scoped execution/abort request. */
    HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL = 19,
    /** Host-to-HIL-RIG test-independent Application recovery request. */
    HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL = 20,
    /** HIL-RIG-to-host fixed captured result available after execution. */
    HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT = 32,
    /** HIL-RIG-to-host variable result bytes available after execution. */
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA = 33,
    /** Application acceptance/rejection/completion outcome. */
    HIL_APPLICATION_MESSAGE_TYPE_RESPONSE = 48,
    /** Broader Application fault report. */
    HIL_APPLICATION_MESSAGE_TYPE_ERROR = 49,
    /** Reserved sentinel; never a valid message type. */
    HIL_APPLICATION_MESSAGE_TYPE_RESERVED = 255
} HIL_Application_Message_Type_T;

/**
 * @brief Optional semantic subtype inside a message family.
 *
 * @details Initial test-specific families use NONE. System Information uses
 * BASIC. More subtypes require documented compatibility behavior.
 */
typedef enum
{
    /** No subtype; required for initial test-specific families. */
    HIL_APPLICATION_MESSAGE_SUBTYPE_NONE = 0,
    /** Initial basic System Information record. */
    HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC = 1,
    /** Reserved sentinel; never valid in a message. */
    HIL_APPLICATION_MESSAGE_SUBTYPE_RESERVED = 255
} HIL_Application_Message_Subtype_T;

/**
 * @brief Tagged typed representation of one complete Application message.
 *
 * @details type selects exactly one body union member. subtype must be BASIC
 * for both System Information types and NONE for all other currently defined
 * types.
 *
 * has_test_id is zero for System Information and Global Control, one for Test
 * Configuration, Test Instruction, Variable Instruction Data, Execution
 * Control, Test Result, and Variable Result Data, optional for Error, and
 * scope-dependent for Response. A Global Control Response has no Test ID; all
 * test-scoped Responses require one. test_id bytes are ignored when
 * has_test_id is zero; no byte pattern is reserved for absence.
 *
 * Nested pointer fields are borrowed only during synchronous typed validation
 * or encoding. A successful decoder copies variable arrays/bytes into the
 * supplied decode storage and sets nested pointers into that storage. The
 * caller owns both typed structure and storage, and the context retains neither.
 */
typedef struct
{
    /** Message family selecting the active body member. */
    HIL_Application_Message_Type_T type;
    /** Family subtype governed by type-specific rules. */
    HIL_Application_Message_Subtype_T subtype;
    /** Nonzero when test_id is present and must be serialized. */
    uint8_t has_test_id;
    /** Opaque correlation identifier when has_test_id is nonzero. */
    HIL_Application_Test_Id_T test_id;
    /** Typed message body selected by type. */
    union
    {
        /** Body for SYSTEM_INFO_REQUEST. */
        HIL_Application_System_Info_Request_T system_info_request;
        /** Body for SYSTEM_INFO_RESPONSE. */
        HIL_Application_System_Info_Response_T system_info_response;
        /** Body for TEST_CONFIGURATION. */
        HIL_Application_Test_Configuration_T test_configuration;
        /** Body for TEST_INSTRUCTION. */
        HIL_Application_Test_Instruction_T test_instruction;
        /** Body for VARIABLE_INSTRUCTION_DATA. */
        HIL_Application_Variable_Instruction_Data_T variable_instruction_data;
        /** Body for EXECUTION_CONTROL. */
        HIL_Application_Execution_Control_T execution_control;
        /** Body for GLOBAL_CONTROL. */
        HIL_Application_Global_Control_T global_control;
        /** Body for TEST_RESULT. */
        HIL_Application_Test_Result_T test_result;
        /** Body for VARIABLE_RESULT_DATA. */
        HIL_Application_Variable_Result_Data_T variable_result_data;
        /** Body for RESPONSE. */
        HIL_Application_Response_T response;
        /** Body for ERROR. */
        HIL_Application_Error_T error;
    } body;
} HIL_Application_Message_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_MESSAGE_H */
