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

/**
 * @brief Initialize minimal MVP session state.
 *
 * @details The future implementation validates role-specific seed rules, copies
 * the initial sequence, and clears all private handshake, reliable, duplicate,
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
 * @brief Abandon private MVP session work for a high-level failure reason.
 *
 * @details The future implementation retains role, link observation, initial
 * sequence and advanced host identity cursor while clearing all session-scoped
 * progress. Retry exhaustion and incompatible identities both use this complete
 * abandonment boundary; they never continue with uncertain sequence state. The
 * profile separately clears owned bytes and events.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Reset( HIL_Transport_Mvp_Session_T* session,
                                                        HIL_Transport_Failure_T      failure );

/**
 * @brief Reserve, without advancing, the sole reliable transmit sequence.
 *
 * @details This succeeds only with an IDLE reliable slot and either an
 * appropriate private handshake phase or public ESTABLISHED state, after all
 * required byte storage is reserved. Handshake and Application work share this
 * one slot. The matching ACK advances the retained sequence exactly once.
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
 * @details The private result distinguishes one matching ACK from stale or
 * unexpected input. Only a match may advance/release reliable ownership.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement( HIL_Transport_Mvp_Session_T*    session,
                                                    uint16_t                        sequence,
                                                    HIL_Transport_Mvp_Ack_Result_T* result );

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_SESSION_MVP_H */
