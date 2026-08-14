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

/** Publish at most one pending INITIATE, RESPONSE, or CONFIRM. */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Process( HIL_Transport_Mvp_Root_T* root, uint32_t now_ms );

/** Apply semantic handshake, ACK, duplicate, and active-session RESET handling. */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Handshake_Handle_Frame(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Handshake_Frame_Result_T* result );

/** Encode and publish one best-effort, non-reliable RESET control frame. */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Handshake_Publish_Reset( HIL_Transport_Mvp_Root_T* root,
                                           uint64_t session_identifier );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_HANDSHAKE_MVP_H */
