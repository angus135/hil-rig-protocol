/**
 * @file application_internal.h
 * @brief Private fixed-width wire helpers and common-envelope representation.
 *
 * @details The constants in this header define encoded widths explicitly. They
 * must not be replaced with sizeof(enum), sizeof(struct), native byte order, or
 * size_t wire fields. The bounded header parser is shared by normal decoding
 * and decode-storage sizing so the common envelope has one implementation.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_INTERNAL_H
#define HIL_RIG_PROTOCOL_APPLICATION_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/application/application_status.h"

/** @name Fixed wire widths */
/** @{ */
#define HIL_APPLICATION_WIRE_U8_SIZE 1u
#define HIL_APPLICATION_WIRE_U16_SIZE 2u
#define HIL_APPLICATION_WIRE_U32_SIZE 4u
#define HIL_APPLICATION_WIRE_U64_SIZE 8u
#define HIL_APPLICATION_WIRE_ENUM_SIZE 1u
#define HIL_APPLICATION_BYTE_SPAN_LENGTH_SIZE 1u
/** @} */

/** @name Application Response body offsets and fixed wire width */
/** @{ */
#define HIL_APPLICATION_RESPONSE_SCOPE_OFFSET 0u
#define HIL_APPLICATION_RESPONSE_OUTCOME_OFFSET 1u
#define HIL_APPLICATION_RESPONSE_REASON_OFFSET 2u
#define HIL_APPLICATION_RESPONSE_TICK_NUMBER_OFFSET 3u
#define HIL_APPLICATION_RESPONSE_CONTROL_COMMAND_OFFSET 7u
#define HIL_APPLICATION_RESPONSE_GLOBAL_CONTROL_COMMAND_OFFSET 8u
#define HIL_APPLICATION_RESPONSE_DETAIL_OFFSET 9u
#define HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE 13u
/** @} */

/** @name Application Error body offsets and fixed wire width */
/** @{ */
#define HIL_APPLICATION_ERROR_CATEGORY_OFFSET 0u
#define HIL_APPLICATION_ERROR_RECOVERABLE_OFFSET 1u
#define HIL_APPLICATION_ERROR_HAS_TICK_NUMBER_OFFSET 2u
#define HIL_APPLICATION_ERROR_TICK_NUMBER_OFFSET 3u
#define HIL_APPLICATION_ERROR_DETAIL_OFFSET 7u
#define HIL_APPLICATION_ERROR_DIAGNOSTIC_LENGTH_OFFSET 11u
#define HIL_APPLICATION_ERROR_DIAGNOSTIC_DATA_OFFSET 12u
#define HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE 12u
/** @} */

/** @name Update Instruction payload header offsets and wire widths */
/** @{ */
#define HIL_APPLICATION_UPDATE_INSTRUCTION_TICK_OFFSET 0u
#define HIL_APPLICATION_UPDATE_INSTRUCTION_COUNT_OFFSET 4u
#define HIL_APPLICATION_UPDATE_INSTRUCTION_FLAGS_OFFSET 5u
#define HIL_APPLICATION_UPDATE_INSTRUCTION_RESERVED_OFFSET 6u
#define HIL_APPLICATION_UPDATE_INSTRUCTION_HEADER_SIZE 8u
/** @} */

/** @name Variable Test Result payload header offsets and wire widths */
/** @{ */
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_TICK_OFFSET 0u
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_COUNT_OFFSET 4u
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_CONDITION_OFFSET 5u
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_FLAGS_OFFSET 6u
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_RESERVED_OFFSET 7u
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_PROBLEM_DETAIL_OFFSET 8u
#define HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE 12u
/** @} */

/** @name Operation and captured record TLV wire widths */
/** @{ */
#define HIL_APPLICATION_RECORD_HEADER_SIZE 4u
#define HIL_APPLICATION_OPERATION_RECORD_HEADER_SIZE HIL_APPLICATION_RECORD_HEADER_SIZE
#define HIL_APPLICATION_CAPTURED_RECORD_HEADER_SIZE HIL_APPLICATION_RECORD_HEADER_SIZE
/** @} */

/** Fixed Test Instruction payload width derived from the published wire fields. */
#define HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE                                        \
    ( HIL_APPLICATION_WIRE_U32_SIZE                                                                \
      + HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U8_SIZE                \
      + HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U32_SIZE                \
      + HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT                                                   \
            * ( HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_U16_SIZE ) )

/** Fixed Test Result payload width derived from the published wire fields. */
#define HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE                                             \
    ( HIL_APPLICATION_WIRE_U32_SIZE                                                                \
      + HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U8_SIZE                 \
      + HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT * HIL_APPLICATION_WIRE_U32_SIZE                 \
      + HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT                                                    \
            * ( HIL_APPLICATION_WIRE_U32_SIZE + HIL_APPLICATION_WIRE_U16_SIZE )                    \
      + HIL_APPLICATION_WIRE_ENUM_SIZE + HIL_APPLICATION_WIRE_U32_SIZE )

#if defined( __cplusplus )
static_assert( HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE == 50u,
               "Test Instruction fixed payload wire width changed" );
static_assert( HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE == 39u,
               "Test Result fixed payload wire width changed" );
static_assert( HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE == 13u,
               "Application Response fixed payload wire width changed" );
static_assert( HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE == 12u,
               "Application Error fixed payload wire width changed" );
static_assert( HIL_APPLICATION_UPDATE_INSTRUCTION_HEADER_SIZE == 8u,
               "Update Instruction payload header wire width changed" );
static_assert( HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE == 12u,
               "Variable Test Result payload header wire width changed" );
static_assert( HIL_APPLICATION_RECORD_HEADER_SIZE == 4u,
               "Record header wire width changed" );
#else
_Static_assert( HIL_APPLICATION_TEST_INSTRUCTION_FIXED_PAYLOAD_SIZE == 50u,
                "Test Instruction fixed payload wire width changed" );
_Static_assert( HIL_APPLICATION_TEST_RESULT_FIXED_PAYLOAD_SIZE == 39u,
                "Test Result fixed payload wire width changed" );
_Static_assert( HIL_APPLICATION_RESPONSE_FIXED_PAYLOAD_SIZE == 13u,
                "Application Response fixed payload wire width changed" );
_Static_assert( HIL_APPLICATION_ERROR_FIXED_PAYLOAD_SIZE == 12u,
                "Application Error fixed payload wire width changed" );
_Static_assert( HIL_APPLICATION_UPDATE_INSTRUCTION_HEADER_SIZE == 8u,
                "Update Instruction payload header wire width changed" );
_Static_assert( HIL_APPLICATION_VARIABLE_TEST_RESULT_HEADER_SIZE == 12u,
                "Variable Test Result payload header wire width changed" );
_Static_assert( HIL_APPLICATION_RECORD_HEADER_SIZE == 4u,
                "Record header wire width changed" );
#endif

/**
 * @brief Lightweight result of bounded common-envelope parsing.
 *
 * @details This is not a packed wire structure. HIL_APPLICATION_Header_Decoding()
 * fills it field-by-field after proving each encoded field is present. Keeping
 * this representation small avoids constructing the full public body union when
 * only message-family classification is required.
 */
typedef struct
{
    uint8_t                           protocol_major;
    uint8_t                           protocol_minor;
    uint8_t                           has_test_id;
    HIL_Application_Test_Id_T         test_id;
    HIL_Application_Message_Type_T    type;
    HIL_Application_Message_Subtype_T subtype;
    uint16_t                          payload_length;
} HIL_Application_Envelope_T;

/** Maximum temporary decode storage required by a supported body. */
#define HIL_APPLICATION_MAX_DECODE_STORAGE_SIZE ( 2u * HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE )

/** Checked size_t addition. Returns zero without writing result on overflow/error. */
static inline int HIL_APPLICATION_Checked_Add_Size( size_t lhs, size_t rhs, size_t* result )
{
    if ( result == NULL || lhs > SIZE_MAX - rhs )
    {
        return 0;
    }
    *result = lhs + rhs;
    return 1;
}

/** Checked size_t multiplication. Returns zero without writing result on overflow/error. */
static inline int HIL_APPLICATION_Checked_Mul_Size( size_t lhs, size_t rhs, size_t* result )
{
    if ( result == NULL || ( rhs != 0u && lhs > SIZE_MAX / rhs ) )
    {
        return 0;
    }
    *result = lhs * rhs;
    return 1;
}

/** Checked conversion from local size_t length to the uint16_t payload-length wire field. */
static inline int HIL_APPLICATION_Size_To_U16( size_t value, uint16_t* result )
{
    if ( result == NULL || value > ( size_t )UINT16_MAX )
    {
        return 0;
    }
    *result = ( uint16_t )value;
    return 1;
}

/** Checked conversion from a public enum's numeric value to its explicit one-byte wire value. */
static inline int HIL_APPLICATION_Enum_To_U8( int value, uint8_t* result )
{
    if ( result == NULL || value < 0 || value > ( int )UINT8_MAX )
    {
        return 0;
    }
    *result = ( uint8_t )value;
    return 1;
}

/** Write one little-endian uint16_t without relying on host byte order. */
static inline void HIL_APPLICATION_Write_U16_Le( uint8_t* dst, uint16_t value )
{
    dst[0] = ( uint8_t )( value & 0xffu );
    dst[1] = ( uint8_t )( ( value >> 8 ) & 0xffu );
}

/** Read one little-endian uint16_t without relying on host byte order. */
static inline uint16_t HIL_APPLICATION_Read_U16_Le( const uint8_t* src )
{
    return ( uint16_t )( ( uint16_t )src[0] | ( uint16_t )( ( uint16_t )src[1] << 8 ) );
}

/** Encode only the fixed 23-byte common envelope; payload length is patched later by the façade. */
HIL_Application_Status_T HIL_APPLICATION_Header_Encoding( const HIL_Application_Message_T* message,
                                                          uint8_t* dest, size_t dest_capacity );

/** Bounded parse of the fixed 23-byte common envelope from an actual encoded input extent. */
HIL_Application_Status_T HIL_APPLICATION_Header_Decoding( HIL_Application_Envelope_T* envelope,
                                                          const uint8_t* encoded_message,
                                                          size_t         encoded_message_size );

/** Verify that a decoded discovery body repeats the parsed envelope version. */
HIL_Application_Status_T
HIL_APPLICATION_Validate_Discovery_Envelope_Body( const HIL_Application_Envelope_T* envelope,
                                                  const HIL_Application_Message_T*  message );

/** Validate common typed envelope fields before family-specific validation/encoding. */
HIL_Application_Status_T
HIL_APPLICATION_Validate_Common_Message_Fields( const HIL_Application_Message_T* message );

#endif
