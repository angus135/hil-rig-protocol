/**
 * @file transport_control_output_mvp.h
 * @brief Private one-item encoded control-output lifecycle for the MVP profile.
 *
 * @details This module owns a complete copy of at most one opaque encoded
 * control item independently of the reliable retry buffer. It does not encode
 * or validate frames, perform I/O, start timers, retransmit, choose output
 * priority, modify global output selection, or publish events. ACK/RESET wire
 * semantics remain the responsibility of a future private producer.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_CONTROL_OUTPUT_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_CONTROL_OUTPUT_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Publish one complete, already encoded control item.
 *
 * @details The source is borrowed only for this call. From IDLE, success copies
 * all bytes into the fixed private slot and enters READY. In READY or PEEKED,
 * byte-for-byte identical publication is idempotent and returns OK without
 * disturbing ownership; a different item returns NOT_READY. The bytes are
 * opaque: this operation performs no COBS, delimiter, CRC, frame-type, session,
 * or acknowledgement validation. Overlap with the embedded destination is
 * supported.
 *
 * @param[in,out] root Private MVP root owning the fixed control slot.
 * @param[in] encoded_item Complete candidate bytes, borrowed for this call.
 * @param[in] encoded_item_size Candidate size from one through the fixed capacity.
 * @return OK, INVALID_ARGUMENT, NOT_READY, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded(
    HIL_Transport_Mvp_Root_T* root, const uint8_t* encoded_item, size_t encoded_item_size );

/**
 * @brief Query or copy the retained control item.
 *
 * @details A NULL destination with zero capacity queries the complete size and
 * returns BUFFER_TOO_SMALL while an item exists. Any insufficient destination
 * reports the required size without a partial copy or state change. A complete
 * READY copy enters PEEKED; repeated PEEKED copies return the same bytes and
 * remain pinned. This private operation deliberately leaves global output
 * selection unchanged until a later arbitration layer owns that decision.
 *
 * @param[in,out] root Private MVP root owning the fixed control slot.
 * @param[out] out_buffer Caller-owned destination, or NULL only with zero capacity.
 * @param[in] out_buffer_size Writable destination capacity.
 * @param[out] output_size Required size or number of bytes copied; cleared first.
 * @return OK, BUFFER_TOO_SMALL, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Control_Output_Peek_Output( HIL_Transport_Mvp_Root_T* root,
                                                                     uint8_t* out_buffer,
                                                                     size_t   out_buffer_size,
                                                                     size_t*  output_size );

/**
 * @brief Release a control item after a successful private peek.
 *
 * @details Only PEEKED may commit. Success immediately returns the slot to IDLE
 * and invalidates its size without clearing the small array. Control output has
 * no acknowledgement timer, retry count, sequence ownership, or external I/O.
 *
 * @param[in,out] root Private MVP root owning the fixed control slot.
 * @return OK, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Control_Output_Commit_Output( HIL_Transport_Mvp_Root_T* root );

/**
 * @brief Abandon or repair private control-output ownership.
 *
 * @details Reset accepts every valid or corrupted lifecycle state, enters IDLE,
 * and sets the size to zero without validating or clearing stored bytes. It is
 * the explicit recovery path and changes no reliable, session, parser, message,
 * event, configuration, or global output-selection state.
 *
 * @param[in,out] root Private MVP root to recover.
 * @return OK or INVALID_ARGUMENT.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Control_Output_Reset( HIL_Transport_Mvp_Root_T* root );

/**
 * @brief Query whether the private control slot owns output.
 *
 * @details READY and PEEKED report one; IDLE reports zero. The result is cleared
 * before later validation. This query is intentionally not connected to public
 * status until public output arbitration can return the same item.
 *
 * @param[in,out] root Private MVP root; invariant failure records FAULT/INTERNAL.
 * @param[out] pending Zero or one, defensively cleared before validation.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( HIL_Transport_Mvp_Root_T* root,
                                                     uint8_t*                  pending );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_CONTROL_OUTPUT_MVP_H */
