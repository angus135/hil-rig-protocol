#include "transport_frame_codec_mvp.h"

#include "../common/transport_cobs.h"
#include "../common/transport_crc.h"

#include <limits.h>
#include <string.h>

#define HIL_TRANSPORT_MVP_HEADER_SIZE ( 14u )
#define HIL_TRANSPORT_MVP_CRC_SIZE ( 4u )

static int HIL_TRANSPORT_MVP_Ranges_Overlap( const void* first, size_t first_size,
                                             const void* second, size_t second_size )
{
    const uintptr_t first_start  = ( uintptr_t )first;
    const uintptr_t second_start = ( uintptr_t )second;
    uintptr_t       first_end;
    uintptr_t       second_end;

    if ( ( first_size == 0u ) || ( second_size == 0u ) )
    {
        return 0;
    }
    if ( ( first_start > ( UINTPTR_MAX - first_size ) )
         || ( second_start > ( UINTPTR_MAX - second_size ) ) )
    {
        return 1;
    }
    first_end  = first_start + first_size;
    second_end = second_start + second_size;
    return ( first_start < second_end ) && ( second_start < first_end );
}

static void HIL_TRANSPORT_MVP_Write_U16_LE( uint8_t* output, uint16_t value )
{
    output[0] = ( uint8_t )( value & UINT16_C( 0x00FF ) );
    output[1] = ( uint8_t )( value >> 8u );
}

static void HIL_TRANSPORT_MVP_Write_U32_LE( uint8_t* output, uint32_t value )
{
    output[0] = ( uint8_t )( value & UINT32_C( 0x000000FF ) );
    output[1] = ( uint8_t )( ( value >> 8u ) & UINT32_C( 0x000000FF ) );
    output[2] = ( uint8_t )( ( value >> 16u ) & UINT32_C( 0x000000FF ) );
    output[3] = ( uint8_t )( value >> 24u );
}

static void HIL_TRANSPORT_MVP_Write_U64_LE( uint8_t* output, uint64_t value )
{
    uint8_t index;

    for ( index = 0u; index < 8u; index++ )
    {
        output[index] = ( uint8_t )( value & UINT64_C( 0xFF ) );
        value >>= 8u;
    }
}

static uint16_t HIL_TRANSPORT_MVP_Read_U16_LE( const uint8_t* input )
{
    return ( uint16_t )( ( uint16_t )input[0] | ( ( uint16_t )input[1] << 8u ) );
}

static uint32_t HIL_TRANSPORT_MVP_Read_U32_LE( const uint8_t* input )
{
    return ( uint32_t )input[0] | ( ( uint32_t )input[1] << 8u ) | ( ( uint32_t )input[2] << 16u )
           | ( ( uint32_t )input[3] << 24u );
}

static uint64_t HIL_TRANSPORT_MVP_Read_U64_LE( const uint8_t* input )
{
    uint64_t value = 0u;
    uint8_t  index;

    for ( index = 0u; index < 8u; index++ )
    {
        value |= ( ( uint64_t )input[index] << ( index * 8u ) );
    }
    return value;
}

static int HIL_TRANSPORT_MVP_Frame_Type_Valid( HIL_Transport_Mvp_Frame_Type_T type )
{
    return ( type >= HIL_TRANSPORT_MVP_FRAME_INITIATE )
           && ( type <= HIL_TRANSPORT_MVP_FRAME_RESET );
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Validate_Frame( const HIL_Transport_Mvp_Frame_T* frame,
                                  size_t maximum_application_message_size )
{
    if ( ( frame == NULL ) || !HIL_TRANSPORT_MVP_Frame_Type_Valid( frame->type )
         || ( frame->session_identifier == HIL_TRANSPORT_SESSION_SEED_INVALID )
         || ( ( frame->payload == NULL ) && ( frame->payload_size != 0u ) ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ( frame->type == HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
    {
        if ( frame->acknowledgement_sequence != 0u )
        {
            return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
        }
        if ( frame->payload_size == 0u )
        {
            return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
        }
        if ( frame->payload_size > maximum_application_message_size )
        {
            return HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE;
        }
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( frame->payload_size != 0u )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    switch ( frame->type )
    {
        case HIL_TRANSPORT_MVP_FRAME_INITIATE:
            return ( frame->acknowledgement_sequence == 0u )
                       ? HIL_TRANSPORT_STATUS_OK
                       : HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
        case HIL_TRANSPORT_MVP_FRAME_RESPONSE:
        case HIL_TRANSPORT_MVP_FRAME_CONFIRM:
            return HIL_TRANSPORT_STATUS_OK;
        case HIL_TRANSPORT_MVP_FRAME_ACK:
            return ( frame->sequence == 0u ) ? HIL_TRANSPORT_STATUS_OK
                                             : HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
        case HIL_TRANSPORT_MVP_FRAME_RESET:
            return ( ( frame->sequence == 0u ) && ( frame->acknowledgement_sequence == 0u ) )
                       ? HIL_TRANSPORT_STATUS_OK
                       : HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
        default:
            return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
}

static HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Exact_Cobs_Size( const uint8_t* raw, size_t raw_size, size_t* encoded_size )
{
    size_t  index;
    size_t  size        = 1u;
    uint8_t nonzero_run = 0u;

    if ( ( raw == NULL ) || ( raw_size == 0u ) || ( encoded_size == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;

    for ( index = 0u; index < raw_size; index++ )
    {
        if ( size == SIZE_MAX )
        {
            return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
        }
        size++;

        if ( raw[index] == 0u )
        {
            nonzero_run = 0u;
        }
        else
        {
            nonzero_run++;
            if ( ( nonzero_run == 254u ) && ( ( index + 1u ) < raw_size ) )
            {
                if ( size == SIZE_MAX )
                {
                    return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
                }
                size++;
                nonzero_run = 0u;
            }
        }
    }

    *encoded_size = size;
    return HIL_TRANSPORT_STATUS_OK;
}

size_t HIL_TRANSPORT_MVP_Max_Encoded_Size( size_t maximum_application_message_size )
{
    size_t raw_size;
    size_t cobs_size;

    if ( ( maximum_application_message_size == 0u )
         || ( maximum_application_message_size > ( SIZE_MAX - HIL_TRANSPORT_MVP_RAW_OVERHEAD ) ) )
    {
        return 0u;
    }
    raw_size = maximum_application_message_size + HIL_TRANSPORT_MVP_RAW_OVERHEAD;
    if ( HIL_TRANSPORT_COBS_Max_Encoded_Size( raw_size, &cobs_size ) != HIL_TRANSPORT_STATUS_OK )
    {
        return 0u;
    }
    if ( cobs_size == SIZE_MAX )
    {
        return 0u;
    }
    return cobs_size + 1u;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Encode_Frame( const HIL_Transport_Mvp_Frame_T* frame,
                                                       size_t   maximum_application_message_size,
                                                       uint8_t* raw_scratch,
                                                       size_t raw_scratch_size, uint8_t* out_buffer,
                                                       size_t out_buffer_size, size_t* output_size )
{
    HIL_Transport_Status_T status;
    size_t                 raw_size;
    size_t                 cobs_size;
    size_t                 produced_size;
    uint32_t               crc;

    if ( output_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0u;

    status = HIL_TRANSPORT_MVP_Validate_Frame( frame, maximum_application_message_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( raw_scratch == NULL ) || ( out_buffer == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( frame->payload_size > ( SIZE_MAX - HIL_TRANSPORT_MVP_RAW_OVERHEAD ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }
    raw_size = HIL_TRANSPORT_MVP_RAW_OVERHEAD + frame->payload_size;
    if ( raw_scratch_size < raw_size )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }
    if ( HIL_TRANSPORT_MVP_Ranges_Overlap( raw_scratch, raw_size, out_buffer, out_buffer_size )
         || HIL_TRANSPORT_MVP_Ranges_Overlap( frame->payload, frame->payload_size, raw_scratch,
                                              raw_size )
         || HIL_TRANSPORT_MVP_Ranges_Overlap( frame->payload, frame->payload_size, out_buffer,
                                              out_buffer_size ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    raw_scratch[0] = HIL_TRANSPORT_MVP_PROTOCOL_VERSION;
    raw_scratch[1] = ( uint8_t )frame->type;
    HIL_TRANSPORT_MVP_Write_U64_LE( raw_scratch + 2u, frame->session_identifier );
    HIL_TRANSPORT_MVP_Write_U16_LE( raw_scratch + 10u, frame->sequence );
    HIL_TRANSPORT_MVP_Write_U16_LE( raw_scratch + 12u, frame->acknowledgement_sequence );
    if ( frame->payload_size != 0u )
    {
        memcpy( raw_scratch + HIL_TRANSPORT_MVP_HEADER_SIZE, frame->payload, frame->payload_size );
    }
    crc = HIL_TRANSPORT_CRC32_Compute( raw_scratch,
                                       HIL_TRANSPORT_MVP_HEADER_SIZE + frame->payload_size );
    HIL_TRANSPORT_MVP_Write_U32_LE(
        raw_scratch + HIL_TRANSPORT_MVP_HEADER_SIZE + frame->payload_size, crc );

    status = HIL_TRANSPORT_MVP_Exact_Cobs_Size( raw_scratch, raw_size, &cobs_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( ( cobs_size == SIZE_MAX ) || ( out_buffer_size < ( cobs_size + 1u ) ) )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    status =
        HIL_TRANSPORT_COBS_Encode( raw_scratch, raw_size, out_buffer, cobs_size, &produced_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( produced_size != cobs_size )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    out_buffer[produced_size] = 0u;
    *output_size              = produced_size + 1u;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Decode_Frame_View(
    const uint8_t* encoded_body, size_t encoded_body_size, uint8_t* raw_scratch,
    size_t raw_scratch_size, size_t maximum_application_message_size,
    HIL_Transport_Mvp_Frame_T* frame, HIL_Transport_Mvp_Decode_Result_T* decode_result )
{
    HIL_Transport_Status_T         status;
    HIL_Transport_Mvp_Frame_T      candidate = { 0 };
    HIL_Transport_Mvp_Frame_Type_T frame_type;
    size_t                         maximum_raw_size;
    size_t                         raw_size = 0u;
    size_t                         payload_size;
    uint32_t                       expected_crc;
    uint32_t                       actual_crc;

    if ( frame != NULL )
    {
        *frame = ( HIL_Transport_Mvp_Frame_T ){ 0 };
    }
    if ( decode_result != NULL )
    {
        *decode_result = HIL_TRANSPORT_MVP_DECODE_MALFORMED;
    }
    if ( ( encoded_body == NULL ) || ( encoded_body_size == 0u ) || ( raw_scratch == NULL )
         || ( raw_scratch_size == 0u ) || ( frame == NULL ) || ( decode_result == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( maximum_application_message_size > ( SIZE_MAX - HIL_TRANSPORT_MVP_RAW_OVERHEAD ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }
    maximum_raw_size = maximum_application_message_size + HIL_TRANSPORT_MVP_RAW_OVERHEAD;
    if ( HIL_TRANSPORT_MVP_Ranges_Overlap( encoded_body, encoded_body_size, raw_scratch,
                                           raw_scratch_size ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( raw_scratch_size < maximum_raw_size )
    {
        /* The internal workspace cannot hold every valid configured frame. */
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    status = HIL_TRANSPORT_COBS_Decode( encoded_body, encoded_body_size, raw_scratch,
                                        raw_scratch_size, &raw_size );
    if ( status == HIL_TRANSPORT_STATUS_INTERNAL_ERROR )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( status == HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL )
    {
        /*
         * Scratch was proven sufficient above. Overflow now means the wire body
         * expands beyond the largest valid configured frame, so it is malformed
         * input rather than caller capacity that can be enlarged and retried.
         */
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( raw_size < HIL_TRANSPORT_MVP_RAW_OVERHEAD )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    expected_crc =
        HIL_TRANSPORT_MVP_Read_U32_LE( raw_scratch + raw_size - HIL_TRANSPORT_MVP_CRC_SIZE );
    actual_crc = HIL_TRANSPORT_CRC32_Compute( raw_scratch, raw_size - HIL_TRANSPORT_MVP_CRC_SIZE );
    if ( expected_crc != actual_crc )
    {
        *decode_result = HIL_TRANSPORT_MVP_DECODE_INTEGRITY_INVALID;
        return HIL_TRANSPORT_STATUS_OK;
    }
    if ( raw_scratch[0] != HIL_TRANSPORT_MVP_PROTOCOL_VERSION )
    {
        *decode_result = HIL_TRANSPORT_MVP_DECODE_SESSION_INCOMPATIBLE;
        return HIL_TRANSPORT_STATUS_OK;
    }

    frame_type = ( HIL_Transport_Mvp_Frame_Type_T )raw_scratch[1];
    if ( !HIL_TRANSPORT_MVP_Frame_Type_Valid( frame_type ) )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    payload_size                       = raw_size - HIL_TRANSPORT_MVP_RAW_OVERHEAD;
    candidate.type                     = frame_type;
    candidate.session_identifier       = HIL_TRANSPORT_MVP_Read_U64_LE( raw_scratch + 2u );
    candidate.sequence                 = HIL_TRANSPORT_MVP_Read_U16_LE( raw_scratch + 10u );
    candidate.acknowledgement_sequence = HIL_TRANSPORT_MVP_Read_U16_LE( raw_scratch + 12u );
    candidate.payload =
        ( payload_size == 0u ) ? NULL : ( raw_scratch + HIL_TRANSPORT_MVP_HEADER_SIZE );
    candidate.payload_size = payload_size;

    status = HIL_TRANSPORT_MVP_Validate_Frame( &candidate, maximum_application_message_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        if ( status == HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE )
        {
            return status;
        }
        return HIL_TRANSPORT_STATUS_OK;
    }

    *frame         = candidate;
    *decode_result = HIL_TRANSPORT_MVP_DECODE_VALID;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Decode_Frame( const uint8_t* encoded_body, size_t encoded_body_size,
                                uint8_t* raw_scratch, size_t raw_scratch_size,
                                HIL_Transport_Mvp_Frame_T* frame, uint8_t* message_buffer,
                                size_t message_buffer_size, size_t* message_size,
                                HIL_Transport_Mvp_Decode_Result_T* decode_result )
{
    HIL_Transport_Mvp_Frame_T candidate;
    HIL_Transport_Status_T    status;

    if ( frame != NULL )
    {
        *frame = ( HIL_Transport_Mvp_Frame_T ){ 0 };
    }
    if ( message_size != NULL )
    {
        *message_size = 0u;
    }
    if ( decode_result != NULL )
    {
        *decode_result = HIL_TRANSPORT_MVP_DECODE_MALFORMED;
    }
    if ( ( encoded_body == NULL ) || ( encoded_body_size == 0u ) || ( raw_scratch == NULL )
         || ( raw_scratch_size == 0u ) || ( frame == NULL ) || ( message_size == NULL )
         || ( decode_result == NULL )
         || ( ( message_buffer == NULL ) && ( message_buffer_size != 0u ) ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( message_buffer_size > ( SIZE_MAX - HIL_TRANSPORT_MVP_RAW_OVERHEAD ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }
    if ( HIL_TRANSPORT_MVP_Ranges_Overlap( encoded_body, encoded_body_size, message_buffer,
                                           message_buffer_size )
         || HIL_TRANSPORT_MVP_Ranges_Overlap( raw_scratch, raw_scratch_size, message_buffer,
                                              message_buffer_size ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    status = HIL_TRANSPORT_MVP_Decode_Frame_View( encoded_body, encoded_body_size, raw_scratch,
                                                  raw_scratch_size, message_buffer_size, &candidate,
                                                  decode_result );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    if ( *decode_result != HIL_TRANSPORT_MVP_DECODE_VALID )
    {
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( candidate.payload_size != 0u )
    {
        memcpy( message_buffer, candidate.payload, candidate.payload_size );
        candidate.payload = message_buffer;
    }
    *frame        = candidate;
    *message_size = candidate.payload_size;
    return HIL_TRANSPORT_STATUS_OK;
}
