/**
 * @file transport_frame_codec_mvp.h
 * @brief Private codec seam for the MVP one-message-per-frame model.
 *
 * @details The codec accepts INITIATE, RESPONSE, CONFIRM, one complete
 * Application message, ACK, and RESET frames. It has no fragment, reassembly,
 * advertised window, keepalive, flow-control, or queue representation.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_FRAME_CODEC_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_FRAME_CODEC_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** MVP wire protocol version encoded in every frame. */
#define HIL_TRANSPORT_MVP_PROTOCOL_VERSION ( 0x01u )

/** Fixed decoded bytes: 14-byte header plus four-byte CRC. */
#define HIL_TRANSPORT_MVP_RAW_OVERHEAD ( 18u )

/** Return worst-case transmitted capacity for one complete MVP Application message. */
size_t HIL_TRANSPORT_MVP_Max_Encoded_Size( size_t maximum_application_message_size );

/**
 * @brief Encode one private semantic MVP frame into a complete opaque item.
 *
 * @details Serializes explicit little-endian fields into raw_scratch, appends
 * CRC-32/ISO-HDLC, COBS-encodes into out_buffer, and appends a 0x00 delimiter.
 * raw_scratch and out_buffer must not overlap. No partial output size is
 * published on failure.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Encode_Frame( const HIL_Transport_Mvp_Frame_T* frame,
                                                       size_t   maximum_application_message_size,
                                                       uint8_t* raw_scratch,
                                                       size_t raw_scratch_size, uint8_t* out_buffer,
                                                       size_t  out_buffer_size,
                                                       size_t* output_size );

/**
 * @brief Decode one opaque body into MVP metadata and a complete message buffer.
 *
 * @details encoded_body excludes the delimiter. message_buffer_size is both the
 * destination capacity and the maximum Application payload accepted by this
 * operation. raw_scratch must hold that capacity plus
 * HIL_TRANSPORT_MVP_RAW_OVERHEAD; a smaller scratch returns BUFFER_TOO_SMALL.
 * Once scratch is correctly provisioned, a COBS body that expands beyond this
 * maximum is malformed wire input. The payload is copied only after complete
 * validation. Buffers must not overlap, no input pointer is retained, and no
 * partial frame or payload is published.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Decode_Frame( const uint8_t* encoded_body, size_t encoded_body_size,
                                uint8_t* raw_scratch, size_t raw_scratch_size,
                                HIL_Transport_Mvp_Frame_T* frame, uint8_t* message_buffer,
                                size_t message_buffer_size, size_t* message_size,
                                HIL_Transport_Mvp_Decode_Result_T* decode_result );

/**
 * @brief Decode one opaque body into a synchronous scratch-backed frame view.
 *
 * @details The operation performs the same COBS, size, CRC, version, field, and
 * frame-specific validation as HIL_TRANSPORT_MVP_Decode_Frame(), but does not
 * copy Application payload bytes into retained message storage. A nonempty
 * valid payload points into raw_scratch; an empty payload is NULL. That view is
 * valid only until raw_scratch is reused and must be handled synchronously.
 * No partial frame is published on failure.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Decode_Frame_View(
    const uint8_t* encoded_body, size_t encoded_body_size, uint8_t* raw_scratch,
    size_t raw_scratch_size, size_t maximum_application_message_size,
    HIL_Transport_Mvp_Frame_T* frame, HIL_Transport_Mvp_Decode_Result_T* decode_result );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_FRAME_CODEC_MVP_H */
