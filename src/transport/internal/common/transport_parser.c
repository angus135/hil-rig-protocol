#include "transport_parser.h"

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Init( HIL_Transport_Parser_T* parser,
                                                  uint8_t*                scratch_buffer,
                                                  size_t                  scratch_buffer_size )
{
    /*
     * TODO: Validate parser and the scratch pointer/size pair, retain the opaque
     * body buffer, and clear length, unread-body, and discard state atomically.
     * Do not decode profile fields or allocate memory.
     */
    ( void )parser;
    ( void )scratch_buffer;
    ( void )scratch_buffer_size;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Parser_Result_T HIL_TRANSPORT_Parser_Push_Byte( HIL_Transport_Parser_T* parser,
                                                              uint8_t                 byte )
{
    /*
     * TODO: Ignore empty delimiters; protect an unread body; append within
     * capacity; publish a nonempty body only at its delimiter; and, after
     * overflow, discard every byte until the next delimiter before accepting a
     * new body. Do not inspect frame or profile metadata.
     */
    ( void )parser;
    ( void )byte;
    return HIL_TRANSPORT_PARSER_RESULT_ERROR;
}

HIL_Transport_Parser_Result_T HIL_TRANSPORT_Parser_Push_Bytes( HIL_Transport_Parser_T* parser,
                                                               const uint8_t*          data,
                                                               size_t                  data_size,
                                                               size_t* bytes_consumed )
{
    /*
     * TODO: Clear and require bytes_consumed, validate the borrowed chunk, apply
     * Push_Byte in order, and report the exact accepted prefix before returning
     * a ready/discard/error result. Never drop or replay an unreported suffix.
     */
    ( void )parser;
    ( void )data;
    ( void )data_size;
    if ( bytes_consumed != NULL )
    {
        *bytes_consumed = 0u;
    }
    return HIL_TRANSPORT_PARSER_RESULT_ERROR;
}

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Read_Body( HIL_Transport_Parser_T* parser,
                                                       uint8_t* out_buffer, size_t out_buffer_size,
                                                       size_t* body_size )
{
    /*
     * TODO: Clear and require body_size, validate the output query combination,
     * require one unread body, report required capacity without consuming on a
     * small buffer, and consume only after the full opaque body is copied. Do
     * not decode, classify, or retain the caller output pointer.
     */
    ( void )parser;
    ( void )out_buffer;
    ( void )out_buffer_size;
    if ( body_size != NULL )
    {
        *body_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

void HIL_TRANSPORT_Parser_Reset( HIL_Transport_Parser_T* parser )
{
    /*
     * TODO: Clear accumulated length, unread-body state, and discard state while
     * preserving the caller-workspace scratch pointer and size.
     */
    ( void )parser;
}
