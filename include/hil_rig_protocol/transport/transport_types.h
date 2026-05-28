/**
 * @file transport_types.h
 * @brief Core public types for HIL-RIG transport frames and sessions.
 *
 * @details The transport layer owns byte-stream framing, transport frame
 * metadata, CRC validation, sequence/acknowledgement fields, keep-alive
 * signalling, and session state tracking. Payload bytes remain opaque to this
 * layer and are decoded by the application layer.
 *
 * @note These structs describe the public C API representation. They are not
 * packed wire structs. The final raw transport header byte layout and maximum
 * frame sizes must be fixed before golden vectors are created.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_TYPES_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Major version of the transport wire format currently targeted. */
#define HIL_TRANSPORT_WIRE_VERSION_MAJOR (0u)

/** @brief Minor version of the transport wire format currently targeted. */
#define HIL_TRANSPORT_WIRE_VERSION_MINOR (1u)

/** @brief Default maximum opaque payload size accepted by the transport API. */
#define HIL_TRANSPORT_DEFAULT_MAX_PAYLOAD_SIZE (1024u)

/** @brief Default maximum encoded COBS-delimited frame size. */
#define HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE (1200u)

/** @brief Absolute placeholder maximum encoded frame size for static buffers. */
#define HIL_TRANSPORT_MAX_ENCODED_FRAME_SIZE (4096u)

/** @brief Transport flag requesting acknowledgement. */
#define HIL_TRANSPORT_FLAG_ACK (1u << 0u)

/** @brief Transport flag indicating a negative acknowledgement. */
#define HIL_TRANSPORT_FLAG_NACK (1u << 1u)

/** @brief Transport flag indicating reset/control intent. */
#define HIL_TRANSPORT_FLAG_RST (1u << 2u)

/** @brief Transport flag indicating the final fragment in a sequence. */
#define HIL_TRANSPORT_FLAG_FIN (1u << 3u)

/** @brief Transport flag indicating keep-alive traffic. */
#define HIL_TRANSPORT_FLAG_KEEP_ALIVE (1u << 4u)

/** @brief Transport flag indicating a requested state change. */
#define HIL_TRANSPORT_FLAG_STATE_CHANGE (1u << 5u)

/** @brief Transport flag acknowledging a state change. */
#define HIL_TRANSPORT_FLAG_STATE_CHANGE_ACK (1u << 6u)

/** @brief Transport flag indicating more fragments will follow. */
#define HIL_TRANSPORT_FLAG_MORE_FRAGMENTS (1u << 7u)

/**
 * @brief Endpoint role for transport session behaviour.
 */
typedef enum
{
    HIL_TRANSPORT_ROLE_HOST = 0,
    HIL_TRANSPORT_ROLE_RIG
} HIL_Transport_Role_T;

/**
 * @brief High-level transport session state.
 */
typedef enum
{
    HIL_TRANSPORT_STATE_IDLE = 0,
    HIL_TRANSPORT_STATE_CONFIGURING,
    HIL_TRANSPORT_STATE_ARMED,
    HIL_TRANSPORT_STATE_RUNNING,
    HIL_TRANSPORT_STATE_REPORTING,
    HIL_TRANSPORT_STATE_FAULT,
    HIL_TRANSPORT_STATE_RESERVED_6,
    HIL_TRANSPORT_STATE_RESERVED_7
} HIL_Transport_State_T;

/**
 * @brief Transport frame category.
 */
typedef enum
{
    HIL_TRANSPORT_FRAME_TYPE_DATA = 0,
    HIL_TRANSPORT_FRAME_TYPE_ACK,
    HIL_TRANSPORT_FRAME_TYPE_NACK,
    HIL_TRANSPORT_FRAME_TYPE_KEEP_ALIVE,
    HIL_TRANSPORT_FRAME_TYPE_RESET,
    HIL_TRANSPORT_FRAME_TYPE_CONTROL
} HIL_Transport_Frame_Type_T;

/**
 * @brief API-level transport header metadata.
 *
 * @note TODO: Define the exact raw header byte layout, field widths, byte
 * order, CRC coverage, and version negotiation rules.
 */
typedef struct
{
    uint8_t version_major;
    uint8_t version_minor;
    HIL_Transport_Frame_Type_T frame_type;
    uint16_t flags;
    uint16_t sequence;
    uint16_t ack_sequence;
    size_t payload_len;
    uint32_t crc32;
} HIL_Transport_Header_T;

/**
 * @brief Transport frame with an opaque byte payload.
 *
 * @note The payload pointer is owned by the caller. The transport layer must
 * not assume any application-layer structure inside these bytes.
 */
typedef struct
{
    HIL_Transport_Header_T header;
    const uint8_t *payload;
    size_t payload_len;
} HIL_Transport_Frame_T;

/**
 * @brief Session and parser counters for diagnostics.
 */
typedef struct
{
    uint32_t frames_tx;
    uint32_t frames_rx;
    uint32_t bytes_tx;
    uint32_t bytes_rx;
    uint32_t malformed_frames;
    uint32_t crc_errors;
    uint32_t sequence_errors;
    uint32_t duplicate_frames;
    uint32_t timeouts;
    uint32_t resyncs;
} HIL_Transport_Stats_T;

/**
 * @brief Event categories emitted by the transport session layer.
 */
typedef enum
{
    HIL_TRANSPORT_EVENT_NONE = 0,
    HIL_TRANSPORT_EVENT_FRAME_RECEIVED,
    HIL_TRANSPORT_EVENT_ACK_RECEIVED,
    HIL_TRANSPORT_EVENT_NACK_RECEIVED,
    HIL_TRANSPORT_EVENT_KEEP_ALIVE_RECEIVED,
    HIL_TRANSPORT_EVENT_STATE_CHANGE_REQUESTED,
    HIL_TRANSPORT_EVENT_STATE_CHANGED,
    HIL_TRANSPORT_EVENT_TIMEOUT,
    HIL_TRANSPORT_EVENT_FAULT
} HIL_Transport_Event_Type_T;

/**
 * @brief Transport session event data.
 */
typedef struct
{
    HIL_Transport_Event_Type_T type;
    HIL_Transport_Status_T status;
    HIL_Transport_Frame_Type_T frame_type;
    HIL_Transport_State_T state;
    uint16_t sequence;
    uint16_t flags;
} HIL_Transport_Event_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_TYPES_H */
