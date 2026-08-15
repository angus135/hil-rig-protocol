#include "transport_parser.h"

#include <string.h>

static int HIL_TRANSPORT_Parser_State_Is_Valid( const HIL_Transport_Parser_T* parser )
{
    return ( parser != NULL ) && ( parser->scratch_buffer != NULL )
           && ( parser->scratch_buffer_size != 0u )
           && ( parser->accumulated_size <= parser->scratch_buffer_size )
           && ( parser->body_ready <= 1u ) && ( parser->discarding <= 1u )
           && !( ( parser->body_ready != 0u ) && ( parser->discarding != 0u ) )
           && !( ( parser->body_ready != 0u ) && ( parser->accumulated_size == 0u ) )
           && !( ( parser->discarding != 0u ) && ( parser->accumulated_size != 0u ) );
}

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Init( HIL_Transport_Parser_T* parser,
                                                  uint8_t*                scratch_buffer,
                                                  size_t                  scratch_buffer_size )
{
    if ( ( parser == NULL ) || ( scratch_buffer == NULL ) || ( scratch_buffer_size == 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    *parser                     = ( HIL_Transport_Parser_T ){ 0 };
    parser->scratch_buffer      = scratch_buffer;
    parser->scratch_buffer_size = scratch_buffer_size;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Parser_Result_T HIL_TRANSPORT_Parser_Push_Byte( HIL_Transport_Parser_T* parser,
                                                              uint8_t                 byte )
{
    if ( !HIL_TRANSPORT_Parser_State_Is_Valid( parser ) )
    {
        return HIL_TRANSPORT_PARSER_RESULT_ERROR;
    }
    if ( parser->body_ready != 0u )
    {
        return HIL_TRANSPORT_PARSER_RESULT_BODY_READY;
    }
    if ( parser->discarding != 0u )
    {
        if ( byte == 0u )
        {
            parser->discarding = 0u;
            return HIL_TRANSPORT_PARSER_RESULT_DISCARDED_BODY;
        }
        return HIL_TRANSPORT_PARSER_RESULT_DISCARDING;
    }
    if ( byte == 0u )
    {
        if ( parser->accumulated_size == 0u )
        {
            return HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA;
        }
        parser->body_ready = 1u;
        return HIL_TRANSPORT_PARSER_RESULT_BODY_READY;
    }
    if ( parser->accumulated_size == parser->scratch_buffer_size )
    {
        parser->accumulated_size = 0u;
        parser->discarding       = 1u;
        return HIL_TRANSPORT_PARSER_RESULT_DISCARDING;
    }

    parser->scratch_buffer[parser->accumulated_size++] = byte;
    return HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA;
}

HIL_Transport_Parser_Result_T HIL_TRANSPORT_Parser_Push_Bytes( HIL_Transport_Parser_T* parser,
                                                               const uint8_t*          data,
                                                               size_t                  data_size,
                                                               size_t* bytes_consumed )
{
    HIL_Transport_Parser_Result_T result = HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA;
    size_t                        index;

    if ( bytes_consumed != NULL )
    {
        *bytes_consumed = 0u;
    }
    if ( ( bytes_consumed == NULL ) || ( parser == NULL )
         || ( ( data == NULL ) && ( data_size != 0u ) )
         || !HIL_TRANSPORT_Parser_State_Is_Valid( parser ) )
    {
        return HIL_TRANSPORT_PARSER_RESULT_ERROR;
    }
    if ( parser->body_ready != 0u )
    {
        return HIL_TRANSPORT_PARSER_RESULT_BODY_READY;
    }

    for ( index = 0u; index < data_size; index++ )
    {
        result = HIL_TRANSPORT_Parser_Push_Byte( parser, data[index] );
        if ( result == HIL_TRANSPORT_PARSER_RESULT_ERROR )
        {
            return result;
        }
        *bytes_consumed = index + 1u;

        if ( result == HIL_TRANSPORT_PARSER_RESULT_BODY_READY )
        {
            return result;
        }
        if ( result == HIL_TRANSPORT_PARSER_RESULT_DISCARDED_BODY )
        {
            return result;
        }
    }

    return result;
}

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Read_Body( HIL_Transport_Parser_T* parser,
                                                       uint8_t* out_buffer, size_t out_buffer_size,
                                                       size_t* body_size )
{
    const uint8_t*         body;
    HIL_Transport_Status_T status;

    if ( body_size != NULL )
    {
        *body_size = 0u;
    }
    if ( ( body_size == NULL ) || ( parser == NULL ) || ( parser->scratch_buffer == NULL )
         || ( ( out_buffer == NULL ) && ( out_buffer_size != 0u ) ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    status = HIL_TRANSPORT_Parser_Peek_Body( parser, &body, body_size );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }

    if ( ( out_buffer == NULL ) || ( out_buffer_size < *body_size ) )
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    /* Overlap, including exact scratch aliasing, is permitted by this private operation. */
    memmove( out_buffer, body, *body_size );
    return HIL_TRANSPORT_Parser_Consume_Body( parser );
}

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Peek_Body( const HIL_Transport_Parser_T* parser,
                                                       const uint8_t** body, size_t* body_size )
{
    if ( body != NULL )
    {
        *body = NULL;
    }
    if ( body_size != NULL )
    {
        *body_size = 0u;
    }
    if ( ( parser == NULL ) || ( body == NULL ) || ( body_size == NULL ) )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_Parser_State_Is_Valid( parser ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( parser->body_ready == 0u )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    *body      = parser->scratch_buffer;
    *body_size = parser->accumulated_size;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Consume_Body( HIL_Transport_Parser_T* parser )
{
    if ( parser == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( !HIL_TRANSPORT_Parser_State_Is_Valid( parser ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( parser->body_ready == 0u )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    parser->body_ready       = 0u;
    parser->accumulated_size = 0u;
    return HIL_TRANSPORT_STATUS_OK;
}

void HIL_TRANSPORT_Parser_Reset( HIL_Transport_Parser_T* parser )
{
    if ( parser == NULL )
    {
        return;
    }
    parser->accumulated_size = 0u;
    parser->body_ready       = 0u;
    parser->discarding       = 0u;
}
