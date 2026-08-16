/**
 * @file transport_handshake_mvp.h
 * @brief Private semantic coordinator for the MVP session handshake.
 *
 * @details This module owns only handshake decisions and phase transitions. It
 * composes the frame codec, reliable/control output lifecycles, bounded event
 * FIFO, and session coordinator without duplicating their storage, timing,
 * retry, arbitration, or recovery responsibilities.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_HANDSHAKE_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_HANDSHAKE_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Semantic disposition of a validly decoded frame supplied to the handshake. */
typedef enum
{
    HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED = 0,
    HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE,
    HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE,
    HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE
} HIL_Transport_Mvp_Handshake_Frame_Result_T;

/** Semantic result when an Application frame is considered as final host-handshake evidence. */
typedef enum
{
    HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_NOT_APPLICABLE = 0,
    HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_ACCEPTED,
    HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_STALE,
    HIL_TRANSPORT_MVP_HANDSHAKE_APPLICATION_PROOF_INCOMPATIBLE
} HIL_Transport_Mvp_Handshake_Application_Proof_Result_T;

/** Publish at most one pending INITIATE, RESPONSE, or CONFIRM. */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Process( HIL_Transport_Mvp_Root_T* root,
                                                            uint32_t                  now_ms );

/**
 * Apply semantic handshake, ACK, duplicate, and active-session RESET handling.
 *
 * @return HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED if semantic acceptance is
 * temporarily blocked by a pinned reliable retry or unavailable ACK control
 * output. The same decoded frame remains retryable without partial acceptance.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Handle_Frame( HIL_Transport_Mvp_Root_T*                   root,
                                          const HIL_Transport_Mvp_Frame_T*            frame,
                                          HIL_Transport_Mvp_Handshake_Frame_Result_T* result );

/**
 * Abandon one locally failed active session and notify the peer with RESET.
 *
 * @details Session cleanup always occurs before RESET publication so previous
 * reliable/control ownership cannot block recovery. A successfully published
 * RESET becomes a private recovery barrier and retains the failed session
 * identity until its control output is committed; later incompatible traffic
 * must not abandon again and clear it. A SESSION_RESET event may be
 * backpressured, but that never prevents the mandatory RESET attempt. Peer-
 * initiated RESET and physical-link loss must continue to use Session_Abandon()
 * directly so RESET is not echoed.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Begin_Local_Recovery( HIL_Transport_Mvp_Root_T* root,
                                                  HIL_Transport_Failure_T   failure );

/**
 * Complete the host's final handshake from a valid post-CONFIRM Application frame.
 *
 * @details This is narrowly valid only while the host is waiting for the final
 * CONFIRM ACK. A same-session, exact-next-sequence Application frame proves the
 * rig accepted CONFIRM. The retained CONFIRM is completed exactly as though its
 * ACK arrived, subject to the existing pinned-output and event-capacity rules.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Try_Complete_Host_From_Application(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Handshake_Application_Proof_Result_T* result );

/** Encode and publish one best-effort, non-reliable RESET control frame. */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Publish_Reset( HIL_Transport_Mvp_Root_T* root,
                                                                  uint64_t session_identifier );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_HANDSHAKE_MVP_H */
