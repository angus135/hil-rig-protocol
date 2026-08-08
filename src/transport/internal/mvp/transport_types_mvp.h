/**
 * @file transport_types_mvp.h
 * @brief Minimal private state and semantic types for the default MVP profile.
 *
 * @details The MVP carries exactly one complete Application message per frame,
 * uses a private three-message session handshake, and owns at most one reliable
 * transmission. It deliberately has no fragment identity, reassembly, window,
 * keepalive, flow-policy, or multi-message queue types.
 *
 * This header may be included by internal compile tests. It is not installed and
 * creates no compatibility promise for integrations.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_TYPES_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_TYPES_MVP_H

#include <stddef.h>
#include <stdint.h>

#include "../common/transport_parser.h"
#include "../transport_internal.h"

/** Explicit private MVP progress through session establishment. */
typedef enum
{
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE = 0,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING,
    HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED
} HIL_Transport_Mvp_Handshake_Phase_T;

/** MVP wire frame type values. Values not listed here are reserved. */
typedef enum
{
    HIL_TRANSPORT_MVP_FRAME_INVALID             = 0x00,
    HIL_TRANSPORT_MVP_FRAME_INITIATE            = 0x01,
    HIL_TRANSPORT_MVP_FRAME_RESPONSE            = 0x02,
    HIL_TRANSPORT_MVP_FRAME_CONFIRM             = 0x03,
    HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE = 0x04,
    HIL_TRANSPORT_MVP_FRAME_ACK                 = 0x05,
    HIL_TRANSPORT_MVP_FRAME_RESET               = 0x06
} HIL_Transport_Mvp_Frame_Type_T;

/**
 * @brief Ownership state of the sole MVP reliable encoded transmission.
 *
 * @details READY/PEEKED describe the initial transmission. RETRANSMIT_READY/
 * RETRANSMIT_PEEKED deliberately remain separate so a retry is counted only
 * after external I/O accepts it through Commit_Output(). AWAITING_ACK is the
 * only timed state. An exact ACK may complete the item there or cancel an
 * unpinned retry in RETRANSMIT_READY. RETRANSMIT_PEEKED remains pinned until
 * commit or reset. EXHAUSTED retains ownership and identifying metadata for
 * later session policy but exposes no output.
 */
typedef enum
{
    /** No reliable bytes or sequence are owned. */
    HIL_TRANSPORT_MVP_RELIABLE_IDLE = 0,

    /** A newly published encoded frame is available for its first peek. */
    HIL_TRANSPORT_MVP_RELIABLE_READY,

    /** Initial bytes were peeked and are pinned until commit or reset. */
    HIL_TRANSPORT_MVP_RELIABLE_PEEKED,

    /** A committed transmission is retained while its exact ACK is awaited. */
    HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK,

    /** Timeout made the retained bytes available for one permitted retry. */
    HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,

    /** Retry bytes were peeked and are pinned until commit or reset. */
    HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,

    /** Retry allowance ended; the owner must choose session recovery policy. */
    HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED
} HIL_Transport_Mvp_Reliable_State_T;

/** Private selection pinned by a successful Peek_Output() operation. */
typedef enum
{
    /** No output is pinned for a following commit. */
    HIL_TRANSPORT_MVP_OUTPUT_NONE = 0,

    /** The sole retained reliable encoded frame is pinned. */
    HIL_TRANSPORT_MVP_OUTPUT_RELIABLE,

    /** Reserved for a later ACK/control-output implementation. */
    HIL_TRANSPORT_MVP_OUTPUT_CONTROL
} HIL_Transport_Mvp_Output_Selection_T;

/** Private classification of a received reliable sequence. */
typedef enum
{
    HIL_TRANSPORT_MVP_RX_SEQUENCE_EXPECTED = 0,
    HIL_TRANSPORT_MVP_RX_SEQUENCE_DUPLICATE,
    HIL_TRANSPORT_MVP_RX_SEQUENCE_INCOMPATIBLE
} HIL_Transport_Mvp_Rx_Sequence_Result_T;

/** Private classification of a received acknowledgement. */
typedef enum
{
    /** Exact retained sequence while awaiting ACK or before an unpinned retry. */
    HIL_TRANSPORT_MVP_ACK_MATCHED = 0,

    /** Wrong sequence or a lifecycle state where ACK completion is not allowed. */
    HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED
} HIL_Transport_Mvp_Ack_Result_T;

/** Private structural classification produced by the MVP decoder. */
typedef enum
{
    HIL_TRANSPORT_MVP_DECODE_VALID = 0,
    HIL_TRANSPORT_MVP_DECODE_MALFORMED,
    HIL_TRANSPORT_MVP_DECODE_INTEGRITY_INVALID,
    HIL_TRANSPORT_MVP_DECODE_SESSION_INCOMPATIBLE
} HIL_Transport_Mvp_Decode_Result_T;

/** Minimal semantic frame passed only between the MVP profile and codec. */
typedef struct
{
    HIL_Transport_Mvp_Frame_Type_T type;
    uint64_t                       session_identifier;
    uint16_t                       sequence;
    uint16_t                       acknowledgement_sequence;
    const uint8_t*                 payload;
    size_t                         payload_size;
} HIL_Transport_Mvp_Frame_T;

/** Private MVP session and stop-and-wait state. */
typedef struct
{
    HIL_Transport_Role_T                role;
    HIL_Transport_Link_State_T          link_state;
    HIL_Transport_Session_State_T       state;
    HIL_Transport_Mvp_Handshake_Phase_T handshake_phase;
    uint64_t                            session_identifier;
    uint8_t                             session_identifier_valid;
    uint64_t                            next_host_session_identifier;
    uint16_t                            initial_reliable_sequence;
    uint16_t                            next_transmit_sequence;
    uint16_t                            expected_receive_sequence;
    /** Sequence owned by the active reliable item; valid outside IDLE. */
    uint16_t retained_transmit_sequence;

    /** Frame category owned by the active reliable item; INVALID in IDLE. */
    HIL_Transport_Mvp_Frame_Type_T retained_reliable_frame_type;

    uint16_t last_accepted_receive_sequence;
    uint8_t  accepted_receive_sequence_valid;

    /** Current ownership/peek/commit/retry state for the sole reliable item. */
    HIL_Transport_Mvp_Reliable_State_T reliable_state;

    /** Number of retransmissions successfully committed to external I/O. */
    uint8_t retransmissions_committed;

    /** Caller time supplied by the most recent initial or retry commit. */
    uint32_t reliable_last_committed_ms;

    uint32_t                last_valid_receive_ms;
    HIL_Transport_Failure_T last_failure;
} HIL_Transport_Mvp_Session_T;

/**
 * @brief Private root object placed at the aligned start of MVP workspace.
 *
 * @details Byte pointers identify single retained regions, not queues: one
 * submitted message, one stable encoded output/retry copy, one parser body, one
 * decoded-frame scratch region, and one complete received message. Workspace
 * sizing reserves these non-overlapping regions with checked arithmetic.
 */
typedef struct
{
    HIL_Transport_Internal_State_T base;
    HIL_Transport_Mvp_Session_T    session;
    HIL_Transport_Parser_T         parser;
    uint8_t*                       submitted_message;
    size_t                         submitted_message_size;
    uint8_t                        submitted_message_pending;
    /** Existing single full-size region holding the published encoded frame. */
    uint8_t* encoded_output;

    /** Configured writable capacity of encoded_output. */
    size_t encoded_output_capacity;

    /** Valid retained byte count; zero means no encoded output is owned. */
    size_t encoded_output_size;

    /** Item selected by a successful peek and protected until commit/reset. */
    HIL_Transport_Mvp_Output_Selection_T output_selection;
    uint8_t*                             codec_scratch;
    size_t                               codec_scratch_size;
    uint8_t*                             received_message;
    size_t                               received_message_size;
    uint8_t                              received_message_pending;
    HIL_Transport_Event_T                pending_event;
    uint8_t                              event_pending;
} HIL_Transport_Mvp_Root_T;

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_TYPES_MVP_H */
