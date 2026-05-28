/**
 * @file transport_session.h
 * @brief Session and reliability API for the HIL-RIG transport layer.
 *
 * @details The session layer will eventually own endpoint role, transport
 * state, sequence numbers, acknowledgements, keep-alive timing, pending state
 * changes, retry counters, and transport statistics.
 *
 * @note TODO: Define retry semantics, state-transition rules, timeout handling,
 * duplicate-frame handling, and ACK/NACK behaviour.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_SESSION_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_status.h"
#include "hil_rig_protocol/transport/transport_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Transport session configuration.
 */
typedef struct
{
    uint32_t keep_alive_interval_ms;
    uint32_t ack_timeout_ms;
    uint8_t  max_retries;
    size_t   max_payload_len;
} HIL_Transport_Session_Config_T;

/**
 * @brief Transport session state.
 *
 * @note This public struct supports static allocation by firmware and host
 * tools. Its fields are API state, not a wire format.
 */
typedef struct
{
    HIL_Transport_Role_T           role;
    HIL_Transport_State_T          state;
    HIL_Transport_State_T          pending_state;
    HIL_Transport_Session_Config_T config;
    HIL_Transport_Stats_T          stats;
    uint16_t                       next_sequence;
    uint16_t                       expected_sequence;
    uint16_t                       last_ack_sequence;
    uint32_t                       last_rx_ms;
    uint32_t                       last_tx_ms;
    uint32_t                       state_change_deadline_ms;
    uint8_t                        retry_count;
    uint8_t                        state_change_pending;
} HIL_Transport_Session_T;

/**
 * @brief Initialize a transport session.
 *
 * @param session Session object to initialize.
 * @param role Endpoint role for this session.
 * @param config Optional configuration. Defaults are used when NULL.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Session_Init( HIL_Transport_Session_T*              session,
                                                   HIL_Transport_Role_T                  role,
                                                   const HIL_Transport_Session_Config_T* config );

/**
 * @brief Build a DATA frame using current session state.
 *
 * @param session Initialized session.
 * @param payload Opaque payload bytes.
 * @param payload_len Number of payload bytes.
 * @param out_frame Receives API-level frame metadata.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Session_Build_Data_Frame( HIL_Transport_Session_T* session,
                                                               const uint8_t*           payload,
                                                               size_t                   payload_len,
                                                               HIL_Transport_Frame_T*   out_frame );

/**
 * @brief Build a keep-alive frame.
 *
 * @param session Initialized session.
 * @param now_ms Current monotonic time in milliseconds.
 * @param out_frame Receives API-level frame metadata.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Session_Build_Keep_Alive( HIL_Transport_Session_T* session,
                                                               uint32_t                 now_ms,
                                                               HIL_Transport_Frame_T*   out_frame );

/**
 * @brief Request a transport state change.
 *
 * @param session Initialized session.
 * @param requested_state Requested target state.
 * @param now_ms Current monotonic time in milliseconds.
 * @param out_frame Receives API-level control frame metadata.
 * @return Transport status code.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_Session_Request_State_Change( HIL_Transport_Session_T* session,
                                            HIL_Transport_State_T requested_state, uint32_t now_ms,
                                            HIL_Transport_Frame_T* out_frame );

/**
 * @brief Process a received transport frame.
 *
 * @param session Initialized session.
 * @param frame Received frame metadata and payload.
 * @param now_ms Current monotonic time in milliseconds.
 * @param out_event Receives session event details.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Session_Receive_Frame( HIL_Transport_Session_T*     session,
                                                            const HIL_Transport_Frame_T* frame,
                                                            uint32_t                     now_ms,
                                                            HIL_Transport_Event_T* out_event );

/**
 * @brief Advance session timers and emit timeout/keep-alive events.
 *
 * @param session Initialized session.
 * @param now_ms Current monotonic time in milliseconds.
 * @param out_event Receives session event details.
 * @return Transport status code.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Session_Tick( HIL_Transport_Session_T* session,
                                                   uint32_t                 now_ms,
                                                   HIL_Transport_Event_T*   out_event );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_SESSION_H */
