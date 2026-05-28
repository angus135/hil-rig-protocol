#include "hil_rig_protocol/transport/transport_parser.h"

#include "hil_rig_protocol/transport/transport_frame.h"

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Init(
    HIL_Transport_Parser_T *parser,
    uint8_t *scratch_buffer,
    size_t scratch_buffer_len
)
{
    if ((parser == (HIL_Transport_Parser_T *)0) ||
        (scratch_buffer == (uint8_t *)0) ||
        (scratch_buffer_len == 0u))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    parser->scratch_buffer = scratch_buffer;
    parser->scratch_buffer_len = scratch_buffer_len;
    parser->bytes_used = 0u;
    parser->frame_ready = 0u;
    parser->last_status = HIL_TRANSPORT_STATUS_OK;

    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Parse_Result_T HIL_TRANSPORT_Parser_Push_Byte(
    HIL_Transport_Parser_T *parser,
    uint8_t byte
)
{
    if ((parser == (HIL_Transport_Parser_T *)0) ||
        (parser->scratch_buffer == (uint8_t *)0) ||
        (parser->scratch_buffer_len == 0u))
    {
        return HIL_TRANSPORT_PARSE_RESULT_ERROR;
    }

    if (byte == 0u)
    {
        parser->frame_ready = 1u;
        parser->last_status = HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;

        /*
         * TODO: Treat this as a delimiter, then COBS-decode and validate the
         * accumulated encoded frame before reporting readiness.
         */
        return HIL_TRANSPORT_PARSE_RESULT_FRAME_READY;
    }

    if (parser->bytes_used >= parser->scratch_buffer_len)
    {
        parser->bytes_used = 0u;
        parser->frame_ready = 0u;
        parser->last_status = HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;

        /*
         * TODO: Define overflow handling and resynchronisation behaviour for
         * malformed or overlong encoded frames.
         */
        return HIL_TRANSPORT_PARSE_RESULT_FRAME_REJECTED;
    }

    parser->scratch_buffer[parser->bytes_used] = byte;
    parser->bytes_used++;
    parser->last_status = HIL_TRANSPORT_STATUS_OK;

    return HIL_TRANSPORT_PARSE_RESULT_NEED_MORE_DATA;
}

HIL_Transport_Parse_Result_T HIL_TRANSPORT_Parser_Push_Bytes(
    HIL_Transport_Parser_T *parser,
    const uint8_t *data,
    size_t data_len,
    size_t *bytes_consumed
)
{
    size_t index = 0u;
    HIL_Transport_Parse_Result_T result = HIL_TRANSPORT_PARSE_RESULT_NEED_MORE_DATA;

    if (bytes_consumed != (size_t *)0)
    {
        *bytes_consumed = 0u;
    }

    if ((parser == (HIL_Transport_Parser_T *)0) ||
        (data == (const uint8_t *)0) ||
        (bytes_consumed == (size_t *)0))
    {
        return HIL_TRANSPORT_PARSE_RESULT_ERROR;
    }

    for (index = 0u; index < data_len; index++)
    {
        result = HIL_TRANSPORT_Parser_Push_Byte(parser, data[index]);
        *bytes_consumed = index + 1u;

        if (result != HIL_TRANSPORT_PARSE_RESULT_NEED_MORE_DATA)
        {
            return result;
        }
    }

    return result;
}

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Read_Frame(
    HIL_Transport_Parser_T *parser,
    HIL_Transport_Frame_T *out_frame,
    uint8_t *payload_buffer,
    size_t payload_buffer_len,
    size_t *payload_bytes_written
)
{
    HIL_Transport_Status_T status = HIL_TRANSPORT_STATUS_INTERNAL_ERROR;

    if (payload_bytes_written != (size_t *)0)
    {
        *payload_bytes_written = 0u;
    }

    if ((parser == (HIL_Transport_Parser_T *)0) ||
        (out_frame == (HIL_Transport_Frame_T *)0) ||
        (payload_bytes_written == (size_t *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if (parser->frame_ready == 0u)
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }

    status = HIL_TRANSPORT_Decode_Frame(
        parser->scratch_buffer,
        parser->bytes_used,
        out_frame,
        payload_buffer,
        payload_buffer_len,
        payload_bytes_written
    );

    parser->bytes_used = 0u;
    parser->frame_ready = 0u;
    parser->last_status = status;

    return status;
}

void HIL_TRANSPORT_Parser_Reset(
    HIL_Transport_Parser_T *parser
)
{
    if (parser == (HIL_Transport_Parser_T *)0)
    {
        return;
    }

    parser->bytes_used = 0u;
    parser->frame_ready = 0u;
    parser->last_status = HIL_TRANSPORT_STATUS_OK;
}
