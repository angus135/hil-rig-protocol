/**
 * @file transport_frame.h
 * @brief High-level transport frame encode/decode API.
 *
 * @details These APIs will eventually serialize transport header metadata,
 * opaque payload bytes, CRC, COBS framing, and a trailing `0x00` delimiter for
 * transmission over USB CDC or another byte stream.
 *
 * @note The final raw transport header layout, COBS error handling, CRC
 * coverage, and maximum frame sizes are TODOs. No protocol logic is currently
 * implemented by the stub definitions.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_FRAME_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_status.h"
#include "hil_rig_protocol/transport/transport_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the encoded wire size required for a transport frame.
 *
 * @details The future implementation will include raw transport header bytes,
 * payload bytes, CRC bytes, COBS overhead, and the trailing `0x00` delimiter.
 *
 * @param frame Frame description to size.
 * @return Required encoded size in bytes, or 0 when the size cannot be
 * determined.
 */
size_t HIL_TRANSPORT_Encoded_Size(
    const HIL_Transport_Frame_T *frame
);

/**
 * @brief Encode a complete transport frame for the byte stream.
 *
 * @details The future implementation will write one complete COBS-delimited
 * wire frame, including the trailing `0x00` delimiter.
 *
 * @param frame API-level transport frame to encode.
 * @param out_buffer Caller-provided output buffer.
 * @param out_buffer_len Length of `out_buffer` in bytes.
 * @param bytes_written Receives the number of bytes written.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Encode_Frame(
    const HIL_Transport_Frame_T *frame,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
);

/**
 * @brief Decode one complete encoded transport frame.
 *
 * @details The preferred input is one complete COBS-encoded frame without the
 * trailing `0x00` delimiter. The future implementation will also validate the
 * transport header, CRC, and version before copying the opaque payload into the
 * caller-provided payload buffer.
 *
 * @param encoded_frame Encoded frame bytes.
 * @param encoded_frame_len Length of `encoded_frame` in bytes.
 * @param out_frame Receives decoded transport metadata and payload pointer.
 * @param payload_buffer Caller-provided storage for the decoded payload.
 * @param payload_buffer_len Length of `payload_buffer` in bytes.
 * @param payload_bytes_written Receives decoded payload length.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Decode_Frame(
    const uint8_t *encoded_frame,
    size_t encoded_frame_len,
    HIL_Transport_Frame_T *out_frame,
    uint8_t *payload_buffer,
    size_t payload_buffer_len,
    size_t *payload_bytes_written
);

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_FRAME_H */
