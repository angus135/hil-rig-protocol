/**
 * @file transport_profile.h
 * @brief Private delegation boundary between the facade and a build profile.
 *
 * @details Exactly one implementation of these functions is linked into the
 * library. The interface mirrors the public operations so transport.c contains
 * no profile algorithms or build-profile conditionals. This header is private:
 * function names, state layout, and contracts may evolve without caller source
 * compatibility.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_TRANSPORT_PROFILE_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_TRANSPORT_PROFILE_H

#include "hil_rig_protocol/transport/transport.h"

void HIL_TRANSPORT_PROFILE_Default_Config( HIL_Transport_Config_T* config );

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Required_Storage_Size( const HIL_Transport_Config_T* config,
                                             size_t*                       required_size );

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Init( HIL_Transport_Context_T*       context,
                                                   HIL_Transport_Role_T           role,
                                                   const HIL_Transport_Config_T*  config,
                                                   const HIL_Transport_Storage_T* storage );

/**
 * Delegate complete caller-requested recovery and event clearing to the MVP
 * session coordinator.
 */
HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Reset( HIL_Transport_Context_T* context );

/** Validate the link value, ignore now_ms for this MVP, and delegate the transition. */
HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Notify_Link_State( HIL_Transport_Context_T*   context,
                                         HIL_Transport_Link_State_T link_state, uint32_t now_ms );

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Submit_Application_Data( HIL_Transport_Context_T* context,
                                               const uint8_t* payload, size_t payload_len );

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Receive_Bytes( HIL_Transport_Context_T* context,
                                                            const uint8_t* data, size_t data_len,
                                                            size_t* bytes_consumed );

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Process( HIL_Transport_Context_T* context, uint32_t now_ms,
                               HIL_Transport_Operating_Mode_T operating_mode );

/**
 * @brief Query or copy one complete encoded output without marking it accepted.
 *
 * @details When nothing is pinned, the MVP prefers control output over reliable
 * output. Only a complete successful copy pins the selected lifecycle. Size
 * queries and insufficient buffers leave selection and lifecycle state
 * unchanged; repeated peeks route to the pinned item and do not start timing.
 */
HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Peek_Output( HIL_Transport_Context_T* context,
                                                          uint8_t*                 out_buffer,
                                                          size_t                   out_buffer_size,
                                                          size_t*                  output_size );

/**
 * @brief Record external acceptance of the complete last-peeked output.
 *
 * @details The MVP routes commit to the pinned lifecycle. Control commit
 * releases immediately and ignores time. Reliable commit records the ACK timer
 * origin, retains exact bytes, and increments the retry counter only for a
 * committed retry. The operation performs no I/O or encoding.
 */
HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Commit_Output( HIL_Transport_Context_T* context,
                                                            uint32_t                 now_ms );

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Read_Application_Data( HIL_Transport_Context_T* context, uint8_t* out_buffer,
                                             size_t out_buffer_size, size_t* message_size );

/** Copy and consume one oldest event only on success; preserve output on failure. */
HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Read_Event( HIL_Transport_Context_T* context,
                                                         HIL_Transport_Event_T*   event );

/** Aggregate output state and validated event presence without exposing private counts. */
HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Get_Status( const HIL_Transport_Context_T*   context,
                                                         HIL_Transport_Status_Snapshot_T* status );

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_TRANSPORT_PROFILE_H */
