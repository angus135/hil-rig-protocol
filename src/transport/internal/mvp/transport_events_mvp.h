/**
 * @file transport_events_mvp.h
 * @brief Private fixed-capacity high-level event lifecycle for the MVP profile.
 *
 * @details Complete public event values are copied into a four-entry FIFO.
 * The queue owns only event storage: producers remain responsible for all
 * Transport transitions, semantic duplicate prevention, and the public status
 * they return when publication cannot retain another event.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_EVENTS_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_EVENTS_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Copy one complete event into the next FIFO slot.
 *
 * @details The caller's pointer is borrowed only for the call. Full capacity
 * returns CAPACITY_EXHAUSTED without changing queue metadata or overwriting an
 * older event. Repeated identical values remain separate FIFO occurrences.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Publish( HIL_Transport_Mvp_Root_T*    root,
                                                         const HIL_Transport_Event_T* event );

/**
 * Copy and consume the oldest event only after a complete successful copy,
 * preserving the destination on NOT_READY and every other failure.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Read( HIL_Transport_Mvp_Root_T* root,
                                                      HIL_Transport_Event_T*    event );

/**
 * @brief Release ownership of every queued event without clearing slot bytes.
 *
 * @details This bypasses queue validation so explicit public reset can repair
 * corrupted event metadata. Future automatic session abandonment must preserve
 * older events and attempt to publish SESSION_RESET instead of calling this
 * function.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Reset( HIL_Transport_Mvp_Root_T* root );

/** Report only whether at least one validated event is owned, never its count or capacity. */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Events_Get_Pending_Status( HIL_Transport_Mvp_Root_T* root,
                                                                    uint8_t* event_pending );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_EVENTS_MVP_H */
