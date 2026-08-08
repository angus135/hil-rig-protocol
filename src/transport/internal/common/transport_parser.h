/**
 * @file transport_parser.h
 * @brief Private profile-independent delimited-body accumulator.
 *
 * @details This seam recognizes 0x00 boundaries in a COBS-encoded byte stream.
 * It does not decode frame fields and has no dependency on MVP or extended
 * frame, session, fragmentation, window, queue, or reassembly types. A selected
 * profile reads a complete opaque body and passes it to its own codec.
 *
 * This is an internal header. Internal tests may include it, but that creates no
 * installation or compatibility promise.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_PARSER_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Private result after accepting one or more encoded stream bytes. */
typedef enum
{
    /** More bytes are required before a complete nonempty body is available. */
    HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA = 0,

    /** One complete opaque encoded body is ready and must not be overwritten. */
    HIL_TRANSPORT_PARSER_RESULT_BODY_READY,

    /** The current body overflowed and input is discarded until a delimiter. */
    HIL_TRANSPORT_PARSER_RESULT_DISCARDING,

    /** A pointer, size combination, or private parser invariant is invalid. */
    HIL_TRANSPORT_PARSER_RESULT_ERROR
} HIL_Transport_Parser_Result_T;

/** Private state for one bounded delimited-body accumulator. */
typedef struct
{
    /** Caller-workspace bytes retained for the current opaque body. */
    uint8_t* scratch_buffer;

    /** Writable bytes available at scratch_buffer. */
    size_t scratch_buffer_size;

    /** Bytes accumulated for the current nonempty body. */
    size_t accumulated_size;

    /** Nonzero while a completed body remains unread. */
    uint8_t body_ready;

    /** Nonzero after overflow until the next delimiter is consumed. */
    uint8_t discarding;
} HIL_Transport_Parser_T;

/**
 * @brief Initialize opaque-body accumulation over caller-workspace scratch.
 *
 * @details Validates the pointer/size combination, retains the scratch region,
 * and clears length, ready, and discard state. It neither allocates nor
 * interprets COBS fields.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Parser_Init( HIL_Transport_Parser_T* parser,
                                                  uint8_t*                scratch_buffer,
                                                  size_t                  scratch_buffer_size );

/**
 * @brief Accept one raw stream byte.
 *
 * @details Ignores empty delimiters, completes a nonempty body at a delimiter,
 * refuses to overwrite an unread body, appends
 * within capacity, and discards an oversized body through the next delimiter.
 */
HIL_Transport_Parser_Result_T HIL_TRANSPORT_Parser_Push_Byte( HIL_Transport_Parser_T* parser,
                                                              uint8_t                 byte );

/**
 * @brief Accept an exact prefix of an arbitrary raw byte chunk.
 *
 * @details bytes_consumed reports precisely how many input bytes changed or
 * advanced parser ownership. The implementation stops when a body becomes
 * ready, discard completion must be reported, or capacity is unavailable. The
 * profile may retry only the remaining suffix.
 */
HIL_Transport_Parser_Result_T HIL_TRANSPORT_Parser_Push_Bytes( HIL_Transport_Parser_T* parser,
                                                               const uint8_t*          data,
                                                               size_t                  data_size,
                                                               size_t* bytes_consumed );

/**
 * @brief Copy the unread opaque encoded body without decoding it.
 *
 * @details On OK body_size is bytes copied and the ready body is consumed. On
 * BUFFER_TOO_SMALL it is required bytes and parser state is unchanged. On
 * NOT_READY it is zero. NULL output with zero capacity is a size query.
 * out_buffer may overlap the parser scratch region, including exact aliasing.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Parser_Read_Body( HIL_Transport_Parser_T* parser,
                                                       uint8_t* out_buffer, size_t out_buffer_size,
                                                       size_t* body_size );

/**
 * @brief Clear accumulated, ready, and discard state while retaining scratch.
 */
void HIL_TRANSPORT_Parser_Reset( HIL_Transport_Parser_T* parser );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_PARSER_H */
