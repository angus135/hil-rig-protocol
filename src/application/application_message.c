/**
 * @file application_message.c
 * @brief Common 23-byte Application envelope encoding, parsing, and validation.
 *
 * @details The envelope is serialized field-by-field with explicit one-byte
 * type/subtype values and a little-endian uint16_t payload length. Native C
 * enum size, structure layout, size_t representation, and host byte order are
 * deliberately excluded from the wire format.
 */

#include "application_internal.h"

#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/version.h"

#include <string.h>

_Static_assert( HIL_RIG_PROTOCOL_VERSION_MAJOR <= UINT8_MAX,
                "HIL-RIG protocol major version must fit the Application envelope" );
_Static_assert( HIL_RIG_PROTOCOL_VERSION_MINOR <= UINT8_MAX,
                "HIL-RIG protocol minor version must fit the Application envelope" );
_Static_assert( HIL_RIG_PROTOCOL_VERSION_PATCH <= UINT16_MAX,
                "HIL-RIG protocol patch version must fit System Information Response" );

static int HIL_APPLICATION_Message_Type_Is_Defined( HIL_Application_Message_Type_T type )
{
    switch ( type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return 1;
        case HIL_APPLICATION_MESSAGE_TYPE_INVALID:
        case HIL_APPLICATION_MESSAGE_TYPE_RESERVED:
        default:
            return 0;
    }
}

static int HIL_APPLICATION_Subtype_Is_Representable( HIL_Application_Message_Subtype_T subtype )
{
    return subtype == HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
           || subtype == HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
}

static int HIL_APPLICATION_Test_Id_Is_Zero( const HIL_Application_Test_Id_T* test_id )
{
    size_t i;
    for ( i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        if ( test_id->bytes[i] != 0u )
        {
            return 0;
        }
    }
    return 1;
}

HIL_Application_Status_T
HIL_APPLICATION_Validate_Common_Message_Fields( const HIL_Application_Message_T* message )
{
    uint8_t required_test_id = 0u;

    if ( message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_APPLICATION_Message_Type_Is_Defined( message->type ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }
    if ( !HIL_APPLICATION_Subtype_Is_Representable( message->subtype ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_SUBTYPE;
    }
    if ( message->has_test_id > 1u )
    {
        return HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID;
    }

    if ( message->type == HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST
         || message->type == HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE )
    {
        if ( message->subtype != HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC )
        {
            return HIL_APPLICATION_STATUS_INVALID_SUBTYPE;
        }
    }
    else if ( message->subtype != HIL_APPLICATION_MESSAGE_SUBTYPE_NONE )
    {
        return HIL_APPLICATION_STATUS_INVALID_SUBTYPE;
    }

    switch ( message->type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            required_test_id = 0u;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            required_test_id = 1u;
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            required_test_id = ( uint8_t )( message->body.response.scope
                                                    == HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL
                                                ? 0u
                                                : 1u );
            break;
        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            return HIL_APPLICATION_STATUS_OK;
        default:
            return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }

    if ( message->has_test_id != required_test_id )
    {
        return HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Header_Encoding( const HIL_Application_Message_T* message,
                                                          uint8_t* dest, size_t dest_capacity )
{
    HIL_Application_Status_T status;
    size_t                   offset = 0u;

    if ( message == NULL || dest == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( dest_capacity < HIL_APPLICATION_HEADER_SIZE_BYTES )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    status = HIL_APPLICATION_Validate_Common_Message_Fields( message );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }

    /* Envelope version comes from the repository version, not Application configuration. */
    dest[offset++] = ( uint8_t )HIL_RIG_PROTOCOL_VERSION_MAJOR;
    dest[offset++] = ( uint8_t )HIL_RIG_PROTOCOL_VERSION_MINOR;
    dest[offset++] = message->has_test_id;
    if ( message->has_test_id == 1u )
    {
        memcpy( &dest[offset], message->test_id.bytes, HIL_APPLICATION_TEST_ID_SIZE );
    }
    else
    {
        memset( &dest[offset], 0, HIL_APPLICATION_TEST_ID_SIZE );
    }
    offset += HIL_APPLICATION_TEST_ID_SIZE;
    uint8_t wire_type    = 0u;
    uint8_t wire_subtype = 0u;
    if ( !HIL_APPLICATION_Enum_To_U8( ( int )message->type, &wire_type )
         || !HIL_APPLICATION_Enum_To_U8( ( int )message->subtype, &wire_subtype ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    dest[offset++] = wire_type;
    dest[offset++] = wire_subtype;
    /* The façade patches payload length after successful body encoding. */
    HIL_APPLICATION_Write_U16_Le( &dest[offset], 0u );
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Header_Decoding( HIL_Application_Envelope_T* envelope,
                                                          const uint8_t* encoded_message,
                                                          size_t         encoded_message_size )
{
    size_t  offset = 0u;
    uint8_t wire_type;
    uint8_t wire_subtype;

    if ( envelope == NULL || encoded_message == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    memset( envelope, 0, sizeof( *envelope ) );
    envelope->type = HIL_APPLICATION_MESSAGE_TYPE_INVALID;
    /* Prove the complete fixed envelope is present before reading any envelope field. */
    if ( encoded_message_size < HIL_APPLICATION_HEADER_SIZE_BYTES )
    {
        return HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE;
    }
    if ( encoded_message[offset++] != ( uint8_t )HIL_RIG_PROTOCOL_VERSION_MAJOR
         || encoded_message[offset++] != ( uint8_t )HIL_RIG_PROTOCOL_VERSION_MINOR )
    {
        return HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE;
    }

    envelope->has_test_id = encoded_message[offset++];
    if ( envelope->has_test_id > 1u )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    memcpy( envelope->test_id.bytes, &encoded_message[offset], HIL_APPLICATION_TEST_ID_SIZE );
    offset += HIL_APPLICATION_TEST_ID_SIZE;
    if ( envelope->has_test_id == 0u && !HIL_APPLICATION_Test_Id_Is_Zero( &envelope->test_id ) )
    {
        return HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID;
    }

    wire_type      = encoded_message[offset++];
    envelope->type = ( HIL_Application_Message_Type_T )wire_type;
    if ( !HIL_APPLICATION_Message_Type_Is_Defined( envelope->type ) )
    {
        envelope->type = HIL_APPLICATION_MESSAGE_TYPE_INVALID;
        return HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE;
    }

    wire_subtype      = encoded_message[offset++];
    envelope->subtype = ( HIL_Application_Message_Subtype_T )wire_subtype;
    if ( !HIL_APPLICATION_Subtype_Is_Representable( envelope->subtype ) )
    {
        envelope->type = HIL_APPLICATION_MESSAGE_TYPE_INVALID;
        return HIL_APPLICATION_STATUS_INVALID_SUBTYPE;
    }

    /* Payload length is always a two-byte little-endian wire value. */
    envelope->payload_length = HIL_APPLICATION_Read_U16_Le( &encoded_message[offset] );
    return HIL_APPLICATION_STATUS_OK;
}
