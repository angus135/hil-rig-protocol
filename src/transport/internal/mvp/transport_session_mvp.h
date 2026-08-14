/**
 * @file transport_session_mvp.h
 * @brief Private MVP session and reliable-sequence operation seams.
 *
 * @details These declarations use only public integration types and the minimal
 * MVP-private types. They do not expose or depend on fragmentation, reassembly,
 * windows, keepalives, flow control, or message queues.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_SESSION_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_SESSION_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize minimal MVP session state.
 *
 * @details Validates role-specific seed rules before mutation, copies the
 * initial sequence, and clears all private handshake, reliable, duplicate,
 * retry and failure state. Host INITIATE, rig RESPONSE, and host CONFIRM are
 * reliable and share the public retry policy. The rig establishes after valid
 * CONFIRM and offers its ACK; the host establishes only after that matching ACK.
 * A duplicate CONFIRM is re-ACKed idempotently. It performs no allocation or
 * I/O.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Init( HIL_Transport_Mvp_Session_T* session,
                                                       HIL_Transport_Role_T         role,
                                                       uint64_t                     session_seed,
                                                       uint16_t initial_reliable_sequence );

/**
 * @brief Prepare a fresh handshake attempt without encoding or publishing output.
 *
 * @details Requires a caller-observed connected link and an otherwise clean
 * DISCONNECTED or RECOVERING session. A host consumes and advances its identity
 * cursor when the attempt begins; a rig waits to adopt the peer's identity.
 * Duplicate calls while establishment is already active return NOT_READY.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Begin_Establishment( HIL_Transport_Mvp_Root_T* root );

/**
 * @brief Abandon all session-scoped ownership for an automatic recovery reason.
 *
 * @details Preserves setup, link observation, the advanced host identity cursor,
 * and unread events, then appends one SESSION_RESET event when capacity permits.
 * NONE and LOCAL_RESET are not automatic-abandonment reasons.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Abandon( HIL_Transport_Mvp_Root_T* root,
                                                          HIL_Transport_Failure_T failure );

/**
 * @brief Perform the complete caller-requested reset of an initialized root.
 *
 * @details Clears all session-scoped ownership and every queued event, records
 * LOCAL_RESET, repairs defined private mirrors from valid retained setup,
 * canonicalizes the link-observed flag, preserves the advanced host cursor,
 * and is the only operation that can leave FAULT. Unusable essential retained
 * setup leaves the context in FAULT and returns INTERNAL_ERROR.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Explicit_Reset( HIL_Transport_Mvp_Root_T* root );

/**
 * @brief Reserve, without advancing, the sole reliable transmit sequence.
 *
 * @details This operation checks only sequence ownership: it succeeds with an
 * IDLE reliable slot, copies next_transmit_sequence, and changes no state. The
 * owning handshake/Application path remains responsible for phase eligibility,
 * encoding into reserved storage, and atomic reliability publication. A failed
 * encode or publication therefore creates no sequence gap. Successful
 * publication retains the candidate, and only an exact ACK advances it.
 *
 * @param[in] session Initialized private session metadata.
 * @param[out] sequence Current unconsumed transmit sequence.
 * @return OK, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Reserve_Sequence( HIL_Transport_Mvp_Session_T* session,
                                            uint16_t*                    sequence );

/**
 * @brief Classify received reliable traffic without leaking private statuses.
 *
 * @details The private result distinguishes expected, duplicate and incompatible
 * sequence values. The profile maps it to delivery, duplicate re-ACK, session
 * restart, public status and high-level events.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Sequence( HIL_Transport_Mvp_Session_T*            session,
                                             uint16_t                                sequence,
                                             HIL_Transport_Mvp_Rx_Sequence_Result_T* result );

/**
 * @brief Classify an ACK against the sole committed reliable item.
 *
 * @details The private result reports MATCHED when state is AWAITING_ACK or
 * RETRANSMIT_READY and sequence equals retained_transmit_sequence. Timeout only
 * authorizes an unpinned retry, so an ACK for the preceding commit remains
 * valid in RETRANSMIT_READY. It never mutates or advances the session. The
 * root-level reliability helper uses this classification to release retained
 * bytes and advance sequence atomically. ACKs in READY, PEEKED,
 * RETRANSMIT_PEEKED, EXHAUSTED, or IDLE are stale/unexpected; the peeked retry
 * remains pinned until commit or reset.
 *
 * @param[in] session Initialized private session metadata.
 * @param[in] sequence Validated received acknowledgement sequence.
 * @param[out] result MATCHED or STALE_OR_UNEXPECTED.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR for an invalid private state.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement( HIL_Transport_Mvp_Session_T*    session,
                                                    uint16_t                        sequence,
                                                    HIL_Transport_Mvp_Ack_Result_T* result );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_SESSION_MVP_H */
