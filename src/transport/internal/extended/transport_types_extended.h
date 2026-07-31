/**
 * @file transport_types_extended.h
 * @brief Uncompiled private type skeleton for a future extended profile.
 *
 * @details These names capture possible fragmentation, reassembly, advertised
 * window, keepalive, and queueing concepts without redefining any public type.
 * They do not finalize a wire representation and are not required by common or
 * MVP sources. Every extended name is private and may change before that profile
 * is implemented.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_EXTENDED_TRANSPORT_TYPES_EXTENDED_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_EXTENDED_TRANSPORT_TYPES_EXTENDED_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_types.h"

/** Candidate extended-only Transport frame category. */
typedef enum
{
    HIL_TRANSPORT_EXTENDED_FRAME_INVALID = 0,
    HIL_TRANSPORT_EXTENDED_FRAME_HANDSHAKE,
    HIL_TRANSPORT_EXTENDED_FRAME_APPLICATION_FRAGMENT,
    HIL_TRANSPORT_EXTENDED_FRAME_ACK,
    HIL_TRANSPORT_EXTENDED_FRAME_WINDOW_UPDATE,
    HIL_TRANSPORT_EXTENDED_FRAME_KEEPALIVE,
    HIL_TRANSPORT_EXTENDED_FRAME_RESET
} HIL_Transport_Extended_Frame_Type_T;

/** Candidate private metadata identifying part of one complete message. */
typedef struct
{
    uint32_t message_identifier;
    size_t   complete_message_size;
    size_t   fragment_offset;
} HIL_Transport_Extended_Fragment_T;

/** Candidate extended receive-window and pacing policy. */
typedef struct
{
    size_t   maximum_advertised_window;
    uint32_t new_data_interval_ms;
    uint32_t keepalive_interval_ms;
} HIL_Transport_Extended_Flow_Policy_T;

/** Candidate private state for one incomplete reassembly. */
typedef struct
{
    uint8_t*                          message_storage;
    size_t                            message_storage_size;
    uint8_t*                          coverage_storage;
    size_t                            coverage_storage_size;
    HIL_Transport_Extended_Fragment_T identity;
    size_t                            unique_bytes_received;
    uint8_t                           active;
} HIL_Transport_Extended_Reassembly_T;

/** Candidate API-independent semantic frame passed to an extended codec. */
typedef struct
{
    HIL_Transport_Extended_Frame_Type_T type;
    HIL_Transport_Role_T                source_role;
    uint64_t                            session_identifier;
    uint16_t                            sequence;
    uint16_t                            acknowledgement_sequence;
    size_t                              advertised_window;
    HIL_Transport_Extended_Fragment_T   fragment;
    const uint8_t*                      payload;
    size_t                              payload_size;
} HIL_Transport_Extended_Frame_T;

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_EXTENDED_TRANSPORT_TYPES_EXTENDED_H */
