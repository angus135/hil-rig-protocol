#include "hil_rig_protocol/transport/transport_frame.h"

static HIL_Transport_Status_T HIL_TRANSPORT_COBS_Encode(
    const uint8_t *raw_frame,
    size_t raw_frame_len,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
);

static HIL_Transport_Status_T HIL_TRANSPORT_COBS_Decode(
    const uint8_t *encoded_frame,
    size_t encoded_frame_len,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
);

static void HIL_TRANSPORT_Init_Empty_Frame(HIL_Transport_Frame_T *frame)
{
    frame->header.version_major = HIL_TRANSPORT_WIRE_VERSION_MAJOR;
    frame->header.version_minor = HIL_TRANSPORT_WIRE_VERSION_MINOR;
    frame->header.frame_type = HIL_TRANSPORT_FRAME_TYPE_DATA;
    frame->header.flags = 0u;
    frame->header.sequence = 0u;
    frame->header.ack_sequence = 0u;
    frame->header.payload_len = 0u;
    frame->header.crc32 = 0u;
    frame->payload = (const uint8_t *)0;
    frame->payload_len = 0u;
}

static HIL_Transport_Status_T HIL_TRANSPORT_COBS_Encode(
    const uint8_t *raw_frame,
    size_t raw_frame_len,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
)
{
    if (bytes_written != (size_t *)0)
    {
        *bytes_written = 0u;
    }

    if (((raw_frame == (const uint8_t *)0) && (raw_frame_len > 0u)) ||
        (out_buffer == (uint8_t *)0) ||
        (bytes_written == (size_t *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if (out_buffer_len == 0u)
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    /*
     * TODO: Implement internal COBS encoding for a serialized raw transport
     * frame and append the trailing 0x00 delimiter at the public frame layer.
     */
    (void)raw_frame;
    (void)raw_frame_len;
    (void)out_buffer;

    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

static HIL_Transport_Status_T HIL_TRANSPORT_COBS_Decode(
    const uint8_t *encoded_frame,
    size_t encoded_frame_len,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
)
{
    if (bytes_written != (size_t *)0)
    {
        *bytes_written = 0u;
    }

    if ((encoded_frame == (const uint8_t *)0) ||
        (out_buffer == (uint8_t *)0) ||
        (bytes_written == (size_t *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ((encoded_frame_len == 0u) || (out_buffer_len == 0u))
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    /*
     * TODO: Implement internal COBS decoding and exact malformed-frame error
     * classification.
     */
    (void)encoded_frame;
    (void)encoded_frame_len;
    (void)out_buffer;

    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

size_t HIL_TRANSPORT_Encoded_Size(
    const HIL_Transport_Frame_T *frame
)
{
    if (frame == (const HIL_Transport_Frame_T *)0)
    {
        return 0u;
    }

    /*
     * TODO: Calculate header + payload + CRC + COBS overhead + delimiter once
     * the raw transport header layout and CRC coverage are finalized.
     */
    return 0u;
}

HIL_Transport_Status_T HIL_TRANSPORT_Encode_Frame(
    const HIL_Transport_Frame_T *frame,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
)
{
    if (bytes_written != (size_t *)0)
    {
        *bytes_written = 0u;
    }

    if ((frame == (const HIL_Transport_Frame_T *)0) ||
        (out_buffer == (uint8_t *)0) ||
        (bytes_written == (size_t *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if (out_buffer_len == 0u)
    {
        return HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL;
    }

    /*
     * TODO: Serialize the transport header, payload, and CRC before passing the
     * raw frame bytes to the internal COBS encoder.
     */
    return HIL_TRANSPORT_COBS_Encode(
        frame->payload,
        frame->payload_len,
        out_buffer,
        out_buffer_len,
        bytes_written
    );
}

HIL_Transport_Status_T HIL_TRANSPORT_Decode_Frame(
    const uint8_t *encoded_frame,
    size_t encoded_frame_len,
    HIL_Transport_Frame_T *out_frame,
    uint8_t *payload_buffer,
    size_t payload_buffer_len,
    size_t *payload_bytes_written
)
{
    uint8_t raw_frame_placeholder = 0u;
    size_t raw_bytes_written = 0u;

    if (payload_bytes_written != (size_t *)0)
    {
        *payload_bytes_written = 0u;
    }

    if (out_frame != (HIL_Transport_Frame_T *)0)
    {
        HIL_TRANSPORT_Init_Empty_Frame(out_frame);
    }

    if ((encoded_frame == (const uint8_t *)0) ||
        (out_frame == (HIL_Transport_Frame_T *)0) ||
        (payload_bytes_written == (size_t *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ((payload_buffer == (uint8_t *)0) && (payload_buffer_len > 0u))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if (encoded_frame_len == 0u)
    {
        return HIL_TRANSPORT_STATUS_MALFORMED_FRAME;
    }

    /*
     * TODO: COBS-decode the frame into a raw-frame scratch buffer, then
     * validate the raw header and CRC before copying the opaque payload.
     */
    (void)payload_buffer;
    (void)payload_buffer_len;

    return HIL_TRANSPORT_COBS_Decode(
        encoded_frame,
        encoded_frame_len,
        &raw_frame_placeholder,
        sizeof(raw_frame_placeholder),
        &raw_bytes_written
    );
}
