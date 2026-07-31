/**
 * @file transport_frame_codec_extended.h
 * @brief Uncompiled private frame-codec seam for a future extended profile.
 *
 * @details This design may eventually serialize fragmentation, windows,
 * keepalives, and recovery fields represented by transport_types_extended.h.
 * Field widths, byte order, framing and integrity remain undecided. The MVP and
 * common parser do not include this header.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_EXTENDED_TRANSPORT_FRAME_CODEC_EXTENDED_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_EXTENDED_TRANSPORT_FRAME_CODEC_EXTENDED_H

#include "transport_types_extended.h"

size_t HIL_TRANSPORT_EXTENDED_Encoded_Size( const HIL_Transport_Extended_Frame_T* frame );

HIL_Transport_Status_T
HIL_TRANSPORT_EXTENDED_Encode_Frame( const HIL_Transport_Extended_Frame_T* frame,
                                     uint8_t* out_buffer, size_t out_buffer_size,
                                     size_t* output_size );

HIL_Transport_Status_T
HIL_TRANSPORT_EXTENDED_Decode_Frame( const uint8_t* encoded_body, size_t encoded_body_size,
                                     HIL_Transport_Extended_Frame_T* frame, uint8_t* payload_buffer,
                                     size_t payload_buffer_size, size_t* payload_size );

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_EXTENDED_TRANSPORT_FRAME_CODEC_EXTENDED_H */
