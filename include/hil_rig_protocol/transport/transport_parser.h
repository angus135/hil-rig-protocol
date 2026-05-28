/**
 * @file transport_parser.h
 * @brief Incremental byte-stream parser API for transport frames.
 *
 * @details The parser accumulates encoded bytes from USB CDC or another byte
 * stream until a `0x00` COBS frame delimiter is observed. Future work will
 * perform COBS decoding, frame validation, CRC checks, malformed-frame
 * rejection, and resynchronisation after corrupted input.
 *
 * @note TODO: Define exact COBS decode error handling, partial-frame overflow
 * behaviour, and resynchronisation semantics.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_PARSER_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_status.h"
#include "hil_rig_protocol/transport/transport_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result of pushing bytes into the incremental parser.
 */
typedef enum
{
    HIL_TRANSPORT_PARSE_RESULT_NEED_MORE_DATA = 0,
    HIL_TRANSPORT_PARSE_RESULT_FRAME_READY,
    HIL_TRANSPORT_PARSE_RESULT_FRAME_REJECTED,
    HIL_TRANSPORT_PARSE_RESULT_RESYNCED,
    HIL_TRANSPORT_PARSE_RESULT_ERROR
} HIL_Transport_Parse_Result_T;

/**
 * @brief Incremental parser state using caller-provided scratch storage.
 */
typedef struct
{
    uint8_t *scratch_buffer;
    size_t scratch_buffer_len;
    size_t bytes_used;
    uint8_t frame_ready;
    HIL_Transport_Status_T last_status;
} HIL_Transport_Parser_T;

/**
 * @brief Initialize an incremental transport parser.
 *
 * @param parser Parser object to initialize.
 * @param scratch_buffer Caller-provided encoded-frame scratch storage.
 * @param scratch_buffer_len Length of `scratch_buffer` in bytes.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Parser_Init(
    HIL_Transport_Parser_T *parser,
    uint8_t *scratch_buffer,
    size_t scratch_buffer_len
);

/**
 * @brief Push one byte into the parser.
 *
 * @param parser Initialized parser.
 * @param byte Byte from the transport stream.
 * @return Parse result for the pushed byte.
 */
HIL_Transport_Parse_Result_T HIL_TRANSPORT_Parser_Push_Byte(
    HIL_Transport_Parser_T *parser,
    uint8_t byte
);

/**
 * @brief Push multiple bytes into the parser.
 *
 * @param parser Initialized parser.
 * @param data Byte stream data.
 * @param data_len Number of bytes available in `data`.
 * @param bytes_consumed Receives the number of bytes consumed.
 * @return Parse result after consuming bytes.
 */
HIL_Transport_Parse_Result_T HIL_TRANSPORT_Parser_Push_Bytes(
    HIL_Transport_Parser_T *parser,
    const uint8_t *data,
    size_t data_len,
    size_t *bytes_consumed
);

/**
 * @brief Read the decoded frame after the parser reports frame readiness.
 *
 * @param parser Initialized parser with a complete frame available.
 * @param out_frame Receives decoded frame metadata.
 * @param payload_buffer Caller-provided decoded payload storage.
 * @param payload_buffer_len Length of `payload_buffer` in bytes.
 * @param payload_bytes_written Receives decoded payload length.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Parser_Read_Frame(
    HIL_Transport_Parser_T *parser,
    HIL_Transport_Frame_T *out_frame,
    uint8_t *payload_buffer,
    size_t payload_buffer_len,
    size_t *payload_bytes_written
);

/**
 * @brief Reset parser state while retaining the caller-provided scratch buffer.
 *
 * @param parser Parser to reset.
 */
void HIL_TRANSPORT_Parser_Reset(
    HIL_Transport_Parser_T *parser
);

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_PARSER_H */
