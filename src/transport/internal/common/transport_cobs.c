#include "transport_cobs.h"

#include "../third_party/cobs/cobs.h"

#include <limits.h>

static int HIL_TRANSPORT_COBS_Ranges_Overlap( const void* first, size_t first_size,
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

HIL_Transport_Status_T HIL_TRANSPORT_COBS_Max_Encoded_Size( size_t  input_size,
                                                            size_t* encoded_size )
{
    size_t block_count;

    if ( encoded_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;

    if ( input_size == 0u )
    {
        *encoded_size = 1u;
        return HIL_TRANSPORT_STATUS_OK;
    }

    if ( input_size > ( SIZE_MAX - 253u ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }
    block_count = ( input_size + 253u ) / 254u;
    if ( input_size > ( SIZE_MAX - block_count ) )
    {
        return HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION;
    }

    *encoded_size = input_size + block_count;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_COBS_Encode( const uint8_t* input, size_t input_size,
                                                  uint8_t* output, size_t output_size,
                                                  size_t* encoded_size )
{
    cobs_encode_result result;

    if ( encoded_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *encoded_size = 0u;

    if ( ( input == NULL ) || ( output == NULL ) || ( input_size == 0u ) || ( output_size == 0u )
         || HIL_TRANSPORT_COBS_Ranges_Overlap( input, input_size, output, output_size ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    result = HIL_TRANSPORT_THIRD_PARTY_COBS_Encode( output, output_size, input, input_size );
    if ( result.status == COBS_ENCODE_OUT_BUFFER_OVERFLOW )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }
    if ( result.status != COBS_ENCODE_OK )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    *encoded_size = result.out_len;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_COBS_Decode( const uint8_t* input, size_t input_size,
                                                  uint8_t* output, size_t output_size,
                                                  size_t* decoded_size )
{
    cobs_decode_result result;

    if ( decoded_size == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *decoded_size = 0u;

    if ( ( input == NULL ) || ( output == NULL ) || ( input_size == 0u ) || ( output_size == 0u )
         || HIL_TRANSPORT_COBS_Ranges_Overlap( input, input_size, output, output_size ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    result = HIL_TRANSPORT_THIRD_PARTY_COBS_Decode( output, output_size, input, input_size );
    if ( ( result.status & COBS_DECODE_OUT_BUFFER_OVERFLOW ) != 0 )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }
    if ( result.status != COBS_DECODE_OK )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    *decoded_size = result.out_len;
    return HIL_TRANSPORT_STATUS_OK;
}
