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
#include "hil_rig_protocol/application/application_types.h"
#include "hil_rig_protocol/application/application_status.h"
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

#define HIL_APPLICATION_VALID_TICK_PERIODS_NS                                                      \
    {                                                                                              \
        10000000, 1000000, 100000, 10000                                                           \
    }  // 100 Hz 1kHz 10kHz 100kHz

#define HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE                                                    \
    255U  // Largest allowable # of bytes (bytespan size = uint8_t)
#define HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE                                            \
    HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE  // Byte Span Limit
// CAN {2}, I2C {2}, SPI {2}, UART {2} = .
#define HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK 8U  // variable_data_count = uint8_t
// DIO {20}, AIO {12}, PWMIO {4} + HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK =
#define HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT 24U
// Test Instruction
/**
    What is written to the wire:

    ________________________________________________
    |                                              |
    |               Has Test ID {1}                |
    |______________________________________________|
    |                                              |
    |                Test ID {16}                  |
    |______________________________________________|
    |                     |                        |
    |   Message Type {4}  |  Message Sub-Type {4}  |
    |_____________________|________________________|
    |                                              |
    |           Payload Size (Bytes) {4}           |
    |______________________________________________|
    |                                              |
    |                Payload {X}                   |
    |______________________________________________|
    |                                              |
    |            Payload end flag {1}              |
    |______________________________________________|

    i.e. 30 bytes plus payload bytes

    The largest payload at the moment (4/09/2026) is configuration:

    _______________________________________________________
    |                         |                            |
    |    tick duration {4}    |  expected tick count {4}   |
    |_________________________|____________________________|
    |                         |                            |
    |        flags {4}        |   digital out [10] {10}    |
    |_________________________|____________________________|
    |                         |                            |
    |  digital in [10] {10}   |    analog out [6] {10}     |
    |_________________________|____________________________|
    |                         |                            |
    |    analog in [2] {10}   |       pwm out [2] {16}     |
    |_________________________|____________________________|
    |                         |                            |
    |    pwm in [2] {16}      |     extension data {X}     |
    |_________________________|____________________________|

    extension data has a max size of 255 (defined by HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE)

    meaning the max payload message size is: 611
    and the max message size is: 641

*/
#define HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE                                                  \
    641U  // as calculated by HIL_APPLICATION_Test_Instructions_encode
#define HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT 1000000U  // Arbitrarily selected

// Header defines
#define HIL_APPLICATION_MESSAGE_HAS_ID_SIZE_BYTES 1
#define HIL_APPLICATION_MESSAGE_TYPE_SIZE_BYTES 4
#define HIL_APPLICATION_MESSAGE_SUB_TYPE_SIZE_BYTES 4
#define HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES 4
#define HIL_APPLICATION_HEADER_SIZE_BYTES                                                          \
    ( HIL_APPLICATION_MESSAGE_HAS_ID_SIZE_BYTES + HIL_APPLICATION_TEST_ID_SIZE                     \
      + HIL_APPLICATION_MESSAGE_TYPE_SIZE_BYTES + HIL_APPLICATION_MESSAGE_SUB_TYPE_SIZE_BYTES      \
      + HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES )

/**
 * @brief Semantic family of one complete Application message.
 *
 * @details Numeric assignments are explicit because they are intended to
 * become stable wire identifiers. Changing an assigned value after publication
 * may break firmware/Python compatibility and requires protocol-version review.
 * There is deliberately no Application sequence field: Transport supplies
 * reliable ordered delivery, while test ID/tick/scope correlate semantics.
 * Python integration serializes response-requiring operations so only one is
 * outstanding. The shared codec remains direction-neutral and does not store
 * an endpoint role, request identity, or outstanding operation.
 */
typedef enum
{
    /** Invalid sentinel; also used to clear failed decode output. */
    HIL_APPLICATION_MESSAGE_TYPE_INVALID = 0,
    /** Python-to-firmware request for minimal version/diagnostic information. */
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST = 1,
    /** Firmware-to-Python response with version/diagnostic information. */
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE = 2,
    /** Python-to-firmware test-wide configuration. */
    HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION = 16,
    /** Python-to-firmware fixed instruction for one tick. */
    HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION = 17,
    /** Python-to-firmware variable channel bytes for one tick. */
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA = 18,
    /** Python-to-firmware test-scoped execution/abort request. */
    HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL = 19,
    /** Python-to-firmware test-independent Application recovery request. */
    HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL = 20,
    /** Firmware-to-Python fixed result in an ordered N-tick result set. */
    HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT = 32,
    /** Firmware-to-Python variable bytes declared by a preceding fixed result. */
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA = 33,
    /** Firmware-to-Python acceptance/rejection/completion outcome. */
    HIL_APPLICATION_MESSAGE_TYPE_RESPONSE = 48,
    /** Firmware-to-Python broader Application fault report. */
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

HIL_Application_Status_T HIL_APPLICATION_Header_Encoding( const HIL_Application_Message_T* message,
                                                          const HIL_Application_Context_T* context,
                                                          uint8_t*                         dest );

HIL_Application_Status_T
HIL_APPLICATION_Header_decoding( const HIL_Application_Context_T* old_context,
                                 HIL_Application_Message_T* new_message, const uint8_t* encoded_message,
                                 size_t* payload_size );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_MESSAGE_H */
