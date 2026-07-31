/**
 * @file transport_frame_codec_mvp.h
 * @brief Private codec seam for the MVP one-message-per-frame model.
 *
 * @details The future codec accepts only MVP handshake, complete Application
 * message, ACK, and reset frames. It has no fragment, reassembly, advertised
 * window, keepalive, flow-control, or queue representation. Exact wire fields,
 * framing, byte order, and integrity remain deferred.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_FRAME_CODEC_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_FRAME_CODEC_MVP_H

#include "transport_types_mvp.h"

/** Return the future encoded capacity for one complete MVP Application message. */
size_t HIL_TRANSPORT_MVP_Max_Encoded_Size( size_t maximum_application_message_size );

/**
 * @brief Encode one private semantic MVP frame into a complete opaque item.
 *
 * @details The future implementation validates frame/session/role/sequence and
 * complete-message bounds, serializes explicit fixed-width fields, calculates
 * integrity internally, and publishes no partial output. BUFFER_TOO_SMALL must
 * report required bytes without discarding a valid frame description.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Encode_Frame( const HIL_Transport_Mvp_Frame_T* frame,
                                                       uint8_t* out_buffer, size_t out_buffer_size,
                                                       size_t* output_size );

/**
 * @brief Decode one opaque body into MVP metadata and a complete message buffer.
 *
 * @details Detailed malformed, integrity, stale-session, and sequence results
 * remain private to the profile and are mapped there to PROTOCOL_ERROR or a
 * public operation status. The codec retains no pointer and publishes no partial
 * frame or message.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Decode_Frame( const uint8_t* encoded_body, size_t encoded_body_size,
                                HIL_Transport_Mvp_Frame_T* frame, uint8_t* message_buffer,
                                size_t message_buffer_size, size_t* message_size,
                                HIL_Transport_Mvp_Decode_Result_T* decode_result );

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_FRAME_CODEC_MVP_H */
