/**
 * @file transport_application_mvp.h
 * @brief Private outbound Application-message orchestration for the MVP profile.
 *
 * @details The module owns the relationship between one complete opaque
 * Application message and the existing one-item reliable-output lifecycle. It
 * does not implement retransmission timing, output arbitration, inbound
 * Application delivery, message queues, fragmentation, or Application meaning.
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

/**
 * @brief Copy and publish one complete outbound Application message.
 *
 * @details Success is permitted only in an established session with both the
 * submitted-message slot and reliable lifecycle idle. For otherwise valid,
 * nonempty input, session readiness is checked before the configured payload
 * limit so pre-establishment calls consistently return NOT_READY. The caller
 * payload is copied synchronously, encoded as one APPLICATION_MESSAGE frame into
 * the existing reliable output region, and then published atomically. The
 * candidate
 * transmit sequence is reserved but not advanced; exact acknowledgement later
 * consumes it through the reliability module.
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
