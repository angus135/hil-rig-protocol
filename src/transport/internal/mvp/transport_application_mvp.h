/**
 * @file transport_application_mvp.h
 * @brief Private Application-message delivery orchestration for the MVP profile.
 *
 * @details The module owns the relationship between complete opaque Application
 * messages and the MVP's single outbound reliable item / single unread inbound
 * message. Retransmission timing, output arbitration, stream parsing and
 * Application meaning remain owned elsewhere.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_APPLICATION_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_APPLICATION_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Result of semantically handling one received ACK while Application delivery is active. */
typedef enum
{
    /** The ACK did not complete the active Application delivery. */
    HIL_TRANSPORT_MVP_APPLICATION_ACK_STALE = 0,

    /** The exact ACK completed the active Application delivery. */
    HIL_TRANSPORT_MVP_APPLICATION_ACK_ACCEPTED
} HIL_Transport_Mvp_Application_Ack_Result_T;

/** Result of semantically handling one decoded inbound Application frame. */
typedef enum
{
    /** One new expected message was retained and its ACK was published. */
    HIL_TRANSPORT_MVP_APPLICATION_FRAME_ACCEPTED = 0,

    /** The last accepted Application sequence was re-ACKed without redelivery. */
    HIL_TRANSPORT_MVP_APPLICATION_FRAME_DUPLICATE,

    /** Obsolete disposition; cross-session stale traffic is filtered before this handler. */
    HIL_TRANSPORT_MVP_APPLICATION_FRAME_STALE,

    /** Current-session sequence/session semantics are incompatible with stop-and-wait state. */
    HIL_TRANSPORT_MVP_APPLICATION_FRAME_INCOMPATIBLE
} HIL_Transport_Mvp_Application_Frame_Result_T;

/**
 * @brief Copy and publish one complete outbound Application message.
 *
 * @details Success is permitted only in an established session with both the
 * submitted-message slot and reliable lifecycle idle. For otherwise valid,
 * nonempty input, session readiness is checked before the configured payload
 * limit so pre-establishment calls consistently return NOT_READY. The caller
 * payload is copied synchronously, encoded as one APPLICATION_MESSAGE frame into
 * the existing reliable output region, and then published atomically. The
 * candidate transmit sequence is reserved but not advanced; exact
 * acknowledgement later consumes it through the reliability module.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Submit( HIL_Transport_Mvp_Root_T* root,
                                                             const uint8_t*            payload,
                                                             size_t payload_size );

/**
 * @brief Handle one decoded ACK while an outbound Application message is retained.
 *
 * @details A stale or unexpected ACK changes no delivery ownership. An exact ACK
 * is committed only when one delivery event can be retained, then releases the
 * submitted-message and reliable ownership, advances the transmit sequence
 * through the reliability module, and publishes DELIVERY_CONFIRMED exactly once.
 * An exact ACK for a pinned retransmission returns CAPACITY_EXHAUSTED so the
 * transactional receive path retains it until that output is committed.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Handle_Acknowledgement(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Application_Ack_Result_T* result );

/**
 * @brief Handle one decoded inbound APPLICATION_MESSAGE frame transactionally.
 *
 * @details Expected traffic is copied from decoder scratch into the sole unread
 * message region before ACK encoding can reuse codec scratch. Message ownership
 * and the receive sequence are committed only after the ACK has been retained.
 * A repeat of the last accepted Application sequence is re-ACKed without copying or exposing
 * the payload again. Temporary unread-message or control-output pressure returns
 * CAPACITY_EXHAUSTED without committing receive state so the parser can retain
 * and retry the same encoded body.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Handle_Received_Frame(
    HIL_Transport_Mvp_Root_T* root, const HIL_Transport_Mvp_Frame_T* frame,
    HIL_Transport_Mvp_Application_Frame_Result_T* result );

/**
 * @brief Copy and consume the sole unread complete Application message.
 *
 * @details NULL with zero output capacity is a size query. BUFFER_TOO_SMALL
 * reports the required complete-message size without consuming ownership. A
 * successful full copy clears the unread slot. No partial message is exposed.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Application_Read( HIL_Transport_Mvp_Root_T* root,
                                                           uint8_t*                  out_buffer,
                                                           size_t  out_buffer_size,
                                                           size_t* message_size );

/**
 * @brief Report whether one unread Application message is pending.
 *
 * @details Validates the received-message ownership metadata before exposing it
 * to the public status snapshot. Corrupt pending/size combinations are treated
 * as private invariant failures rather than being copied into public state.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Application_Get_Pending_Status( HIL_Transport_Mvp_Root_T* root,
                                                  uint8_t* application_message_pending );

/**
 * @brief Apply outbound Application policy after reliable retry exhaustion.
 *
 * @details DELIVERY_FAILED is retained before the uncertain session is
 * abandoned. If the event FIFO is full, no delivery/session ownership changes
 * and CAPACITY_EXHAUSTED allows Process() to retry this transition later. Once
 * the failure event is retained, normal session abandonment clears the message
 * and starts replacement-session recovery. SESSION_RESET remains best-effort
 * under the existing event-capacity policy.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Application_Handle_Retry_Exhaustion( HIL_Transport_Mvp_Root_T* root );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_APPLICATION_MVP_H */
