/**
 * @file transport_reliability_mvp.h
 * @brief Private one-item reliable encoded-output lifecycle for the MVP profile.
 *
 * @details The module owns metadata for exactly one already encoded reliable
 * frame. It never encodes, reconstructs, allocates, performs I/O, publishes
 * public events, or chooses recovery policy. The existing encoded-output region
 * remains the only retry copy until matching acknowledgement or reset.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_RELIABILITY_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_RELIABILITY_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Result returned to the private owner after ACK or timeout processing. */
typedef enum
{
    /** No lifecycle transition occurred. */
    HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE = 0,

    /** The retained bytes became available for a permitted retransmission. */
    HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY,

    /** An exact ACK completed and released the retained reliable item. */
    HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED,

    /** Retry allowance ended; the owning session must choose recovery policy. */
    HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED
} HIL_Transport_Mvp_Reliability_Outcome_T;

/**
 * @brief Publish metadata for a frame already encoded in the retained region.
 *
 * @details Publication requires IDLE reliable ownership, a reliable
 * INITIATE/RESPONSE/CONFIRM/APPLICATION type, a nonzero encoded size within the
 * configured region, and the current candidate transmit sequence. Success
 * enters READY without copying bytes, advancing the sequence, starting timing,
 * retaining any caller buffer, or changing global output selection. Invalid
 * publication from IDLE leaves the slot unpublished and its encoded size zero.
 * A non-IDLE slot returns NOT_READY and preserves the active item.
 *
 * @param[in,out] root Initialized private MVP root whose encoded region already
 * contains the complete valid frame.
 * @param[in] frame_type Reliable semantic frame type represented by those bytes.
 * @param[in] sequence Candidate returned by Session_Reserve_Sequence().
 * @param[in] encoded_size Complete encoded transmission size including delimiter.
 * @return OK, INVALID_ARGUMENT, NOT_READY, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Reliability_Publish_Encoded( HIL_Transport_Mvp_Root_T*      root,
                                               HIL_Transport_Mvp_Frame_Type_T frame_type,
                                               uint16_t sequence, size_t encoded_size );

/**
 * @brief Copy or query the sole available reliable output item.
 *
 * @details A NULL buffer with zero capacity is a size query. Insufficient
 * capacity reports the complete size without copying or pinning. A successful
 * READY peek enters PEEKED; a successful retry peek enters
 * RETRANSMIT_PEEKED. Repeated peeks in either peeked state return the same
 * retained bytes and do not affect sequence, timing, retry accounting, or
 * global output selection. The output arbiter alone owns that selection.
 *
 * @param[in,out] root Initialized private MVP root.
 * @param[out] out_buffer Caller-owned destination, or NULL only for a size query.
 * @param[in] out_buffer_size Writable destination size.
 * @param[out] output_size Required destination receiving copied/required size.
 * @return OK, BUFFER_TOO_SMALL, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Peek_Output( HIL_Transport_Mvp_Root_T* root,
                                                                  uint8_t* out_buffer,
                                                                  size_t   out_buffer_size,
                                                                  size_t*  output_size );

/**
 * @brief Commit the reliable item after a successful private reliable peek.
 *
 * @details Commit means external I/O accepted the complete copied item. An
 * initial commit enters AWAITING_ACK, records now_ms, and leaves the committed
 * retransmission count at zero. A retransmission commit increments that count
 * exactly once and replaces the timestamp. Both retain the exact encoded bytes,
 * type, and sequence. Timing starts here rather than at peek because a peek does
 * not prove external acceptance. This local operation does not clear or
 * otherwise change global output selection; the output arbiter routes commit
 * and clears its selection after success.
 *
 * @param[in,out] root Initialized private MVP root.
 * @param[in] now_ms Caller-supplied external-I/O acceptance time.
 * @return OK, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Commit_Output( HIL_Transport_Mvp_Root_T* root,
                                                                    uint32_t now_ms );

/**
 * @brief Classify and atomically complete an exact acknowledgement.
 *
 * @details Exact ACKs may match in AWAITING_ACK or RETRANSMIT_READY. Timeout
 * authorizes a retry but does not invalidate acknowledgement of the previous
 * commit; a match in RETRANSMIT_READY therefore cancels the unpinned retry.
 * Different sequences and ACKs in other states return NO_CHANGE without
 * mutation. RETRANSMIT_PEEKED remains nonmatching because its retry output is
 * pinned until commit or reset.
 *
 * On a match the completed type is captured, next_transmit_sequence advances
 * exactly once with natural uint16_t wrap, and retained metadata/size are
 * invalidated without clearing the large byte region or changing global output
 * selection. This permits reliable completion while control output is pinned.
 *
 * @param[in,out] root Initialized private MVP root.
 * @param[in] acknowledgement_sequence Validated received ACK sequence.
 * @param[out] completed_frame_type Completed type on ACKNOWLEDGED, INVALID otherwise.
 * @param[out] outcome Private owner-facing result, initialized to NO_CHANGE.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
    HIL_Transport_Mvp_Root_T* root, uint16_t acknowledgement_sequence,
    HIL_Transport_Mvp_Frame_Type_T*          completed_frame_type,
    HIL_Transport_Mvp_Reliability_Outcome_T* outcome );

/**
 * @brief Progress timeout and retry availability for one committed item.
 *
 * @details Timing is evaluated only in AWAITING_ACK. Zero timeout disables
 * progression. Otherwise unsigned `now_ms - reliable_last_committed_ms`
 * deliberately provides correct elapsed time across uint32_t wrap. At expiry a
 * remaining retry enters RETRANSMIT_READY without incrementing its counter;
 * exhaustion enters EXHAUSTED while preserving type, sequence, and bytes for
 * the owning session. The module returns policy to its owner and never resets a
 * session or publishes an event itself.
 *
 * @param[in,out] root Initialized private MVP root.
 * @param[in] now_ms Current caller-provided monotonic time.
 * @param[out] outcome NO_CHANGE, RETRANSMIT_READY, or RETRIES_EXHAUSTED.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Reliability_Process_Pending( HIL_Transport_Mvp_Root_T* root, uint32_t now_ms,
                                               HIL_Transport_Mvp_Reliability_Outcome_T* outcome );

/**
 * @brief Make retained bytes available again after an exact peer duplicate.
 *
 * @details READY/PEEKED and RETRANSMIT_READY/RETRANSMIT_PEEKED already expose
 * or pin the retained bytes and therefore do not change. AWAITING_ACK moves to
 * RETRANSMIT_READY while retry allowance remains, or EXHAUSTED otherwise.
 * Requesting or peeking never consumes retry allowance; only the existing
 * retransmission commit path increments retransmissions_committed. The
 * operation never changes the arbiter's pinned output selection.
 *
 * IDLE reports no work. An active retained item whose type differs from the
 * expected duplicate-response type is a private invariant failure.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
    HIL_Transport_Mvp_Root_T* root, HIL_Transport_Mvp_Frame_Type_T expected_frame_type,
    HIL_Transport_Mvp_Reliability_Outcome_T* outcome );

/**
 * @brief Abandon all reliable ownership and return the slot to IDLE.
 *
 * @details Reset works from every lifecycle state. It invalidates encoded size,
 * retained type/sequence, retry count, and timestamp. It does not change global
 * output selection, clear the encoded region, alter configuration/role, choose
 * session policy, or perform I/O. The output arbiter coordinates full output
 * reset and clears selection.
 *
 * @param[in,out] root Private MVP root with an initialized encoded region.
 * @return OK or INVALID_ARGUMENT.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Reset( HIL_Transport_Mvp_Root_T* root );

/**
 * @brief Return reliable-local pending flags after validating local state.
 *
 * @param[in,out] root Initialized private MVP root; invariant failure records
 * FAULT/INTERNAL.
 * @param[out] output_pending Nonzero for ready or peeked initial/retry output.
 * @param[out] delivery_pending Nonzero for every non-IDLE lifecycle state,
 * including EXHAUSTED.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Reliability_Get_Pending_Status(
    HIL_Transport_Mvp_Root_T* root, uint8_t* output_pending, uint8_t* delivery_pending );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_RELIABILITY_MVP_H */
