/**
 * @file transport_output_mvp.h
 * @brief Private public-facing output arbitration for the MVP profile.
 *
 * @details The module presents the independent one-item reliable and control
 * output lifecycles through one stable peek/commit selection. It owns no byte
 * storage and leaves publication to the underlying lifecycle modules.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_OUTPUT_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_OUTPUT_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Copy or query the output item selected by MVP arbitration.
 *
 * @details A previously successful peek remains selected. Otherwise control
 * output is preferred over reliable output. Size queries and undersized
 * destinations report the selected item's complete size without pinning it.
 * Either public or private FAULT state prevents exposure and returns
 * INTERNAL_ERROR after clearing output_size.
 *
 * @param[in,out] root Initialized private MVP root.
 * @param[out] out_buffer Caller-owned destination, or NULL only for a size query.
 * @param[in] out_buffer_size Writable destination size.
 * @param[out] output_size Required size or number of bytes copied; cleared first.
 * @return OK, BUFFER_TOO_SMALL, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Peek_Output( HIL_Transport_Mvp_Root_T* root,
                                                             uint8_t*                  out_buffer,
                                                             size_t  out_buffer_size,
                                                             size_t* output_size );

/**
 * @brief Commit the lifecycle selected by the last successful output peek.
 *
 * @details Reliable commit records now_ms and retains bytes while awaiting an
 * acknowledgement. Control commit ignores now_ms and immediately releases its
 * slot. Selection is cleared only after the routed commit succeeds. Either
 * public or private FAULT state returns INTERNAL_ERROR without changing output
 * ownership.
 *
 * @param[in,out] root Initialized private MVP root.
 * @param[in] now_ms Caller-provided reliable external-I/O acceptance time.
 * @return OK, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Commit_Output( HIL_Transport_Mvp_Root_T* root,
                                                               uint32_t                  now_ms );

/**
 * @brief Clear reliable, control, and global output ownership.
 *
 * @details Reset repairs lifecycle or selection metadata without clearing the
 * underlying byte arrays. The first unexpected underlying reset failure is
 * returned after both lifecycles have been asked to reset.
 *
 * @param[in,out] root Initialized private MVP root.
 * @return OK or the first underlying reset error.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Reset( HIL_Transport_Mvp_Root_T* root );

/**
 * @brief Query aggregate output availability and reliable delivery ownership.
 *
 * @param[in,out] root Initialized private MVP root; invariant failure records
 * FAULT/INTERNAL.
 * @param[out] output_pending Nonzero when either lifecycle has ready or peeked output.
 * @param[out] reliable_delivery_pending Nonzero for non-IDLE reliable ownership only.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Output_Get_Pending_Status(
    HIL_Transport_Mvp_Root_T* root, uint8_t* output_pending, uint8_t* reliable_delivery_pending );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_OUTPUT_MVP_H */
