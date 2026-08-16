/**
 * @file transport.h
 * @brief Public, implementation-independent HIL-RIG Transport facade.
 *
 * @details Firmware and host integrations use this header to exchange opaque,
 * complete Application messages over a caller-owned byte-stream link. The
 * selected private Transport profile decides how sessions, framing, integrity,
 * reliability, and any future fragmentation work. Callers never construct
 * frames, calculate integrity fields, schedule acknowledgements, or manipulate
 * parser/session state.
 *
 * The library performs no USB, UART, serial, DMA, RTOS, callback, clock, or
 * random-number operations. The owning integration supplies storage, link
 * observations, monotonic time, and all external I/O. There is no heap use.
 *
 * @par Execution-context rule
 * One initialized HIL_Transport_Context_T has exactly one owning task, thread,
 * or execution context. Every call using that context must originate from that
 * owner. Calling different functions from different threads, even with external
 * locking, is not a supported integration model. Interrupt and USB callbacks
 * must hand bytes or notifications to the owner. Separate contexts may have
 * separate owners. The library provides no locks, atomics, callbacks, or
 * re-entrancy protection.
 *
 * @par Current implementation status
 * MVP workspace sizing, initialization, CRC, COBS framing, frame codec, and
 * bounded stream parsing are implemented. Independent one-item reliable and
 * control-output lifecycles, public priority arbitration with stable pinned
 * selection, aggregate output status, commit routing, and output reset are
 * implemented. Private bounded event retention, FIFO reads, pending status, and
 * explicit event reset are implemented. Session initialization, establishment
 * preparation, link observation, automatic abandonment, and explicit reset are
 * coordinated through the MVP session module. The private semantic handshake,
 * ACK/RESET production, duplicate recovery, handshake retry policy, and public
 * Process() scheduling are implemented; link, establishment, and abandonment
 * transitions produce events. Public arbitrary-byte receive dispatch now
 * transactionally drives that handshake. Outbound Application submission, exact
 * ACK completion, delivery confirmation, retry exhaustion, and delivery-failure
 * recovery are implemented. Expected inbound Application messages, duplicate
 * re-ACK without redelivery, unread-message backpressure, and public complete-message
 * reads are implemented.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Caller-selected bounds and high-level timing policy.
 *
 * @details HIL_TRANSPORT_Init() copies this structure, so it need not outlive
 * initialization. These limits describe the stable integration boundary, not
 * internal queue depths, fragment sizes, or a final wire representation.
 *
 * Both capacity limits must be nonzero. The selected profile may additionally
 * reject a valid pair as UNSUPPORTED_CONFIGURATION when their relationship
 * cannot be represented.
 *
 * The host supplies session_seed because the library has no entropy source.
 * Each newly initiated session must use a value different from the active or
 * recently abandoned session. The MVP advances the seed when an establishment
 * attempt begins, skips INVALID and RESERVED, and wraps deterministically without
 * consulting clocks or platform services. A HIL-RIG adopts the identity in a
 * valid host initiation; it does not originate one. A HOST must configure a
 * value other than INVALID or RESERVED. A RIG must configure exactly INVALID;
 * any other rig seed is INVALID_ARGUMENT. Transport session identity is
 * unrelated to any Application test identifier.
 *
 * Zero timing values have explicit policy meanings: zero connection timeout
 * disables peer-liveness expiry; zero retransmission timeout retains reliable
 * output until acknowledgement, explicit reset, or link/session abandonment
 * without timer-driven retry/failure; and zero retries permits only the initial
 * committed transmission. If
 * retransmission timing is enabled, the first expiry after that initial
 * transmission therefore exhausts a zero-retry policy. These are API semantics,
 * not proposed production values.
 *
 * The MVP has no keepalive or other idle peer-liveness traffic and therefore
 * supports only connection_timeout_ms == 0. Its Required_Storage_Size() and
 * Init() operations will return UNSUPPORTED_CONFIGURATION for a nonzero value.
 * The field remains public for future profiles that can distinguish an idle,
 * healthy peer from a lost connection.
 */
typedef struct
{
    /** Maximum bytes in one complete submitted or received Application message. */
    size_t max_application_message_size;

    /** Maximum bytes copied in one complete encoded output item. */
    size_t max_encoded_frame_size;

    /** Caller-provided starting identity for host-initiated sessions. */
    uint64_t session_seed;

    /** Caller-provided first reliable sequence value for a new local session. */
    uint16_t initial_reliable_sequence;

    /** Future peer-liveness deadline; zero disables it and is required by MVP. */
    uint32_t connection_timeout_ms;

    /** Milliseconds before retry/delivery failure; zero disables that timing. */
    uint32_t retransmit_timeout_ms;

    /** Retransmissions after the initial commit; zero means no retransmission. */
    uint8_t max_retries;
} HIL_Transport_Config_T;

/**
 * @brief One caller-owned workspace used by the selected private profile.
 *
 * @details The byte region is borrowed for the entire initialized-context
 * lifetime. It must remain writable, at a stable address, and exclusively owned
 * by this context until the context is discarded. The region must not overlap
 * the context or any buffer concurrently passed to an API call. The profile may
 * partition it internally, but those partitions and their layouts are not part
 * of the public contract.
 *
 * Use HIL_TRANSPORT_Required_Storage_Size() for the selected build profile and
 * configuration. A non-NULL workspace must be aligned to at least
 * HIL_TRANSPORT_WORKSPACE_ALIGNMENT. Initialization rejects NULL/nonzero
 * mismatches, insufficient capacity, misalignment, and configurations the
 * selected profile cannot represent. No pointer passed to Submit, Receive,
 * Peek, or Read is retained after that individual call.
 */
typedef struct
{
    /** Start of the caller-owned implementation workspace. */
    uint8_t* workspace;

    /** Number of writable bytes available at workspace. */
    size_t workspace_size;
} HIL_Transport_Storage_T;

/**
 * @brief Caller-allocated facade context.
 *
 * @details Callers may allocate this object statically, automatically, or as a
 * containing-structure member. Its fields are reserved facade bookkeeping and,
 * apart from zero-initializing the complete object before its first Init call,
 * must not be read, written, copied, serialized, or used to infer implementation
 * state. Profile-specific state resides in the caller-provided workspace and is
 * reached only through this facade.
 *
 * The visible representation supports embedded allocation without heap use; it
 * is not permission for integrations to depend on private layout or construct a
 * context manually. The caller must zero-initialize the complete object before
 * its first Init() call. Init accepts only an uninitialized context; a second
 * Init call is invalid and leaves the existing context unchanged.
 */
typedef struct
{
    /** Private profile state located within caller-owned workspace. */
    void* implementation;

    /** Private profile state/workspace size recorded during initialization. */
    size_t implementation_size;

    /** Private facade initialization marker; callers must leave it untouched. */
    uint32_t initialization_cookie;
} HIL_Transport_Context_T;

/**
 * @brief Populate deterministic, non-production configuration defaults.
 *
 * @details The implemented helper initializes every field deterministically. It
 * sets the documented default message/frame capacities, INVALID session seed,
 * zero initial sequence, and zero for all timing/retry policy. It allocates
 * nothing and touches no context. A host caller must replace the seed before
 * Init(); the INVALID default is correct for a rig.
 *
 * @param[out] config Structure to initialize. NULL is ignored defensively.
 *
 */
void HIL_TRANSPORT_Default_Config( HIL_Transport_Config_T* config );

/**
 * @brief Query workspace required by the selected build profile.
 *
 * @details The implementation validates only role-independent
 * configuration and the selected profile's capacity relationships because this
 * operation does not receive an endpoint role. It does not decide whether the
 * configured session seed is valid for a host or rig; that role-specific check
 * belongs to Init(). Both capacity limits must be nonzero. The returned size
 * includes private state, bounded message ownership, parser/frame scratch,
 * stable peek output, received-message retention, and event retention required
 * by that profile. It assumes the
 * workspace start meets HIL_TRANSPORT_WORKSPACE_ALIGNMENT and does not allocate
 * memory. An MVP profile returns UNSUPPORTED_CONFIGURATION when a configured
 * complete message cannot fit its one-message-per-frame model or when
 * connection_timeout_ms is nonzero.
 *
 * @param[in] config Required configuration; NULL is invalid.
 * @param[out] required_size Receives required workspace bytes on OK. It is set
 * to zero before validation and remains zero on failure.
 *
 * @retval HIL_TRANSPORT_STATUS_OK The size was calculated.
 * @retval HIL_TRANSPORT_STATUS_INVALID_ARGUMENT A pointer/value is invalid.
 * @retval HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION The selected profile
 * cannot provide the requested contract.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Required_Storage_Size( const HIL_Transport_Config_T* config,
                                                            size_t* required_size );

/**
 * @brief Initialize a context over caller-owned workspace.
 *
 * @details The caller must zero-initialize the complete context before its first
 * call. Init accepts only an uninitialized context. Calling it for an already
 * initialized context returns INVALID_ARGUMENT and leaves that working context,
 * its workspace ownership, and all pending work unchanged. A failed first call
 * leaves the context deterministically uninitialized.
 *
 * Configuration is mandatory; passing NULL never requests implicit defaults.
 * The implementation requires both capacity limits to be nonzero;
 * requires a usable HOST seed and exactly INVALID for a RIG; validates timing
 * policy, workspace pointer/size/alignment, checked storage arithmetic, and
 * profile support before copying configuration into private state. The MVP
 * returns UNSUPPORTED_CONFIGURATION when connection_timeout_ms is nonzero. On
 * success it retains only the workspace descriptor, copied configuration, and
 * private state. It does not retain config or storage pointers themselves and
 * performs no I/O. To change role, configuration, or workspace, the caller
 * discards the old context and creates a new zero-initialized context.
 * Reinitialization and workspace replacement are not supported, and there is no
 * Destroy operation.
 *
 * @param[in,out] context Zero-initialized caller object, exclusively owned after
 * success.
 * @param[in] role Endpoint role fixed for the initialized context lifetime.
 * @param[in] config Required configuration borrowed only during this call.
 * @param[in] storage Required workspace descriptor borrowed only during this call;
 * the described byte region remains retained as documented above.
 *
 * @retval HIL_TRANSPORT_STATUS_OK Initialization completed.
 * @retval HIL_TRANSPORT_STATUS_INVALID_ARGUMENT Inputs are invalid.
 * @retval HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION Profile cannot support
 * the requested message/frame relationship or another valid option.
 * @retval HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL Workspace is insufficient.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Init( HIL_Transport_Context_T*       context,
                                           HIL_Transport_Role_T           role,
                                           const HIL_Transport_Config_T*  config,
                                           const HIL_Transport_Storage_T* storage );

/**
 * @brief Abandon all session-scoped Transport work while retaining setup.
 *
 * @details Reset clears session negotiation, sequence and ACK state,
 * retransmission ownership, timers, partial input, any future partial
 * reassembly, pinned/uncommitted output, submitted messages, unread received
 * messages, and every queued event. It does not enqueue SESSION_RESET for this
 * explicit caller-initiated action. It retains copied configuration, workspace
 * ownership, endpoint role, and the latest caller-reported link observation.
 * If connected and the previous active session identity is coherent, Reset
 * publishes one best-effort RESET for that identity after local cleanup; later
 * Process() waits for that control output to be committed before beginning a fresh
 * session rather
 * than continuing the previous sequence/acknowledgement state. A reset used to
 * repair FAULT does not transmit an identity that cannot be trusted. It records
 * HIL_TRANSPORT_FAILURE_LOCAL_RESET in status, enters DISCONNECTED if the link
 * is disconnected, or enters RECOVERING if it remains connected so later
 * Process() can start a fresh session. A host uses a new derived identity. This
 * call is the only supported way to clear terminal FAULT on an initialized
 * context. Event slot bytes need not be cleared because zero queue ownership
 * makes them inaccessible. It does not permit changing configuration, role, or
 * workspace. No Application state or hardware is reset. Automatic abandonment
 * preserves existing queued events and attempts to append
 * SESSION_RESET rather than clearing the event FIFO. Reset canonicalizes
 * repairable private lifecycle metadata, including the link-observed flag and
 * private role/link mirrors, while preserving a valid advanced host identity
 * cursor. If essential retained setup cannot be reconstructed safely, it
 * clears ownership where possible, remains in FAULT with INTERNAL recorded,
 * and returns INTERNAL_ERROR; arbitrary memory corruption is not guaranteed to
 * be recoverable.
 *
 * @param[in,out] context Initialized single-owner context.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Reset( HIL_Transport_Context_T* context );

/**
 * @brief Notify Transport of caller-owned physical-link availability.
 *
 * @details The first DISCONNECTED observation is silent because it does not
 * change the effective initialized state. A change to CONNECTED publishes
 * LINK_STATE_CHANGED and prepares a fresh handshake attempt; a host consumes a
 * new session identity and a rig waits for INITIATE. A change to DISCONNECTED
 * publishes LINK_STATE_CHANGED, abandons session-scoped work, preserves older
 * unread events, and attempts to append SESSION_RESET with LINK_LOST. Repeated
 * same-state observations are idempotent and never retry a failed event
 * publication. Reconnection never resumes old reliable, parser, duplicate, or
 * handshake state. In FAULT, the latest link observation is retained without
 * leaving FAULT or publishing normal events. A FAULT-state disconnection still
 * performs best-effort mandatory cleanup and returns INTERNAL_ERROR if that
 * cleanup detects another private invariant failure. The function neither
 * operates nor polls hardware.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[in] link_state CONNECTED or DISCONNECTED.
 * @param[in] now_ms Current caller-provided monotonic time. The MVP accepts but
 * does not use it because connection timeout and link-liveness timing are not
 * implemented.
 * @retval HIL_TRANSPORT_STATUS_OK The observation and required transition completed.
 * @retval HIL_TRANSPORT_STATUS_INVALID_ARGUMENT The context or link value is invalid.
 * @retval HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED The physical transition completed,
 * but one or more resulting events could not be retained.
 * @retval HIL_TRANSPORT_STATUS_INTERNAL_ERROR A private invariant failed; mandatory
 * disconnect cleanup still completed where possible and the context remains in FAULT.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Notify_Link_State( HIL_Transport_Context_T*   context,
                                                        HIL_Transport_Link_State_T link_state,
                                                        uint32_t                   now_ms );

/**
 * @brief Submit one complete opaque Application message for reliable delivery.
 *
 * @details The initial profile-independent contract accepts a message only while
 * public session state is ESTABLISHED. DISCONNECTED, CONNECTING, RECOVERING, or
 * FAULT returns NOT_READY, retains no input pointer, and changes no state. The
 * MVP does not queue messages before establishment. For a nonempty payload with
 * otherwise valid arguments, session readiness is checked before the configured
 * message-size limit, so a pre-establishment submission returns NOT_READY even
 * when that payload would be too large for an established session. On success
 * every payload byte is copied into the retained caller workspace before return;
 * the input pointer is never borrowed beyond the call. The selected profile owns
 * the copy until delivery, failure, reset, or disconnection; reset and
 * disconnection abandon any accepted message. The caller never fragments
 * messages. An MVP may support only messages
 * that fit one frame; that limitation is rejected during initialization rather
 * than exposed as fragment metadata here.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[in] payload Complete message bytes; NULL is valid only when payload_len
 * is zero and the selected profile permits an empty Application message.
 * @param[in] payload_len Complete message size in bytes.
 * @retval HIL_TRANSPORT_STATUS_OK Message was copied and accepted.
 * @retval HIL_TRANSPORT_STATUS_NOT_READY Session is not ESTABLISHED.
 * @retval HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE Configured limit exceeded.
 * @retval HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED No complete-message capacity.
 * @retval HIL_TRANSPORT_STATUS_INVALID_ARGUMENT Inputs are invalid.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Submit_Application_Data( HIL_Transport_Context_T* context,
                                                              const uint8_t*           payload,
                                                              size_t payload_len );

/**
 * @brief Supply an arbitrary chunk from the caller-owned byte-stream receiver.
 *
 * @details Chunks may end at any byte and may contain no frame, one frame, or
 * several frames. On return bytes_consumed is the exact accepted prefix.
 * The caller retries only data + bytes_consumed when less than data_len was
 * consumed. Temporary capacity exhaustion may therefore consume a prefix and
 * return CAPACITY_EXHAUSTED. Structurally malformed input is consumed through
 * the implementation's resynchronization boundary and reported as a high-level
 * PROTOCOL_ERROR event; bytes beyond an unconsumed suffix are never silently
 * discarded. Capacity maps to CAPACITY_EXHAUSTED. Detailed malformed,
 * integrity, stale-session, and sequence classifications remain private and no
 * private numeric status escapes this facade.
 *
 * Complete parser bodies remain internally owned until semantic handling can
 * commit them. If event, reliable-output, or control-output capacity blocks
 * acceptance, the already accepted body remains retained and a later call,
 * including a zero-byte call, retries it without caller resubmission. An
 * oversized body is discarded through its delimiter; if its error event is
 * blocked, only the later unconsumed suffix remains caller-owned.
 *
 * Receive accepts no bytes while the reported link is DISCONNECTED and returns
 * NOT_READY. During an established session, an ACK completes only the active
 * outbound Application delivery that it exactly matches. Stale, duplicate, or
 * otherwise unexpected established-session ACKs are reported as PROTOCOL_ERROR
 * without abandoning the session and are not reinterpreted as handshake input.
 * An expected same-session Application frame is accepted only when the sole
 * unread-message slot and required ACK control output can both commit. Its payload
 * is copied before ACK encoding reuses decode scratch, then receive sequence and
 * message ownership are committed after ACK publication succeeds. A repeat of the last accepted
 * Application sequence is re-ACKed without redelivery. Temporary unread-message
 * or control-output pressure retains the completed parser body and returns
 * CAPACITY_EXHAUSTED. Incompatible current-session sequence/identity follows normal
 * protocol recovery. Traffic carrying the most recently abandoned session
 * identity is rejected as stale without abandoning a replacement session, and an
 * unbound RIG waiting for INITIATE rejects other frame types without starting a
 * second recovery cycle. After a locally generated recovery RESET has been
 * committed, Receive_Bytes() may enter replacement establishment before consuming
 * new input, so callers do not need an intervening Process() call. A valid peer
 * RESET likewise prepares fresh establishment before receive continues, so a RIG
 * may accept the replacement INITIATE from the same offered byte chunk. While a
 * HOST is waiting
 * specifically for the final CONFIRM ACK, a valid same-session exact-next-sequence
 * Application frame proves the RIG accepted CONFIRM and may complete establishment
 * before that frame is handled normally. Peeked CONFIRM bytes remain pinned, so
 * this proof is backpressured until their routed commit. The input is borrowed only
 * for this call. INVALID_ARGUMENT and NOT_READY report zero consumption.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[in] data Received bytes; may be NULL only when data_len is zero.
 * @param[in] data_len Bytes offered in this call.
 * @param[out] bytes_consumed Required output receiving the exact accepted prefix.
 * @return OK for complete consumption, CAPACITY_EXHAUSTED for retryable
 * blockage, NOT_READY while disconnected, INVALID_ARGUMENT, or INTERNAL_ERROR
 * for a detected invariant failure.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Receive_Bytes( HIL_Transport_Context_T* context,
                                                    const uint8_t* data, size_t data_len,
                                                    size_t* bytes_consumed );

/**
 * @brief Advance caller-driven Transport work.
 *
 * @details The owner calls this regularly with current monotonic time and local
 * Transport operating mode. The MVP publishes at most one pending handshake
 * frame, progresses the sole reliable timeout lifecycle once, and routes
 * exhausted INITIATE, RESPONSE, or CONFIRM delivery through local session
 * recovery. That recovery abandons the uncertain session and publishes RESET for
 * the failed identity before replacement establishment can begin. Exhausted outbound
 * Application delivery first retains one
 * DELIVERY_FAILED event, records DELIVERY failure, abandons the uncertain
 * session, publishes the same RESET synchronization signal, and returns
 * DELIVERY_FAILED on that transition. If event capacity is
 * unavailable, that Application transition remains pending and returns
 * CAPACITY_EXHAUSTED until the failure event can be retained. Deferred receive
 * diagnostics are serviced before replacement establishment, so event backpressure
 * after RESET commit remains retryable rather than becoming an invariant fault. A
 * later Process() or Receive_Bytes() call starts the replacement attempt after any
 * old control output has been committed. A host consumes a new session identity; a
 * rig returns to waiting for INITIATE.
 * Handshake exhaustion publishes no Application delivery event. The function
 * never calls hardware; automatic encoded output is retrieved through
 * Peek_Output() and Commit_Output().
 *
 * A valid mode is recorded before state-dependent progress. A disconnected
 * context otherwise performs no work. FAULT returns INTERNAL_ERROR without
 * normal progress. A private invariant failure enters FAULT, records INTERNAL,
 * and stops normal progress until explicit Reset. Idle peer-liveness scheduling
 * remains deferred.
 *
 * NORMAL, BULK_TRANSFER, and QUIET_REAL_TIME are all valid. The MVP may treat
 * them identically, but records the latest valid value in the status snapshot.
 * A numeric value outside the defined enumeration returns INVALID_ARGUMENT,
 * does not replace the previous valid mode, and does not progress Transport
 * work. The mode is local policy only and is never negotiated with the peer.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[in] now_ms Current monotonic milliseconds.
 * @param[in] operating_mode Current local Transport policy mode.
 * @retval HIL_TRANSPORT_STATUS_OK Valid mode accepted and work progressed.
 * @retval HIL_TRANSPORT_STATUS_INVALID_ARGUMENT The context is invalid or the
 * mode is outside the three defined enumeration values; no work is progressed.
 * @retval HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED Required event/output capacity
 * temporarily prevents a pending receive, handshake, or Application-failure
 * transition from completing.
 * @retval HIL_TRANSPORT_STATUS_NOT_READY Pending handshake publication is
 * temporarily blocked by outstanding lifecycle ownership.
 * @retval HIL_TRANSPORT_STATUS_DELIVERY_FAILED An accepted outbound Application
 * message exhausted its configured retries; its failure event was retained and
 * the uncertain session was abandoned.
 * @retval HIL_TRANSPORT_STATUS_INTERNAL_ERROR A private invariant failed and the
 * context entered terminal FAULT.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Process( HIL_Transport_Context_T* context, uint32_t now_ms,
                                              HIL_Transport_Operating_Mode_T operating_mode );

/**
 * @brief Copy the next complete encoded output without committing transmission.
 *
 * @details On OK output_size is bytes copied. On BUFFER_TOO_SMALL it is required
 * bytes and the queued item is unchanged. On NOT_READY it is zero. Passing NULL
 * with size zero is a size query and returns BUFFER_TOO_SMALL with required size
 * when output exists. Output can be private control or reliable traffic; its
 * type remains opaque to the caller. When nothing is pinned, control output is
 * preferred. The same priority applies to size queries and undersized buffers,
 * which do not pin selection or fall back based on caller capacity.
 *
 * A successful complete copy pins the exact selected bytes: repeated peeks
 * return the same item until Commit_Output(), Reset(), or later failure
 * recovery, even if another item becomes ready. Peeking does not start retry
 * timing, update transmitted-byte accounting, or tell Transport that external
 * hardware accepted the frame. A low-level output-buffer-too-small condition is
 * always retryable and cannot discard a valid item. If either session-state
 * mirror is FAULT, peek exposes no bytes, sets output_size to zero, returns
 * INTERNAL_ERROR, and leaves output ownership unchanged until explicit reset.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[out] out_buffer Caller output, or NULL only with out_buffer_size zero.
 * @param[in] out_buffer_size Writable bytes at out_buffer.
 * @param[out] output_size Required output receiving copied/required bytes or zero.
 * @return OK, BUFFER_TOO_SMALL, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Peek_Output( HIL_Transport_Context_T* context,
                                                  uint8_t* out_buffer, size_t out_buffer_size,
                                                  size_t* output_size );

/**
 * @brief Confirm that external I/O accepted the complete last-peeked item.
 *
 * @details Call only after the caller's USB/serial implementation accepted all
 * bytes as one transmission. Transport routes commit to the lifecycle that
 * produced the pinned item. Control commit releases its slot immediately and
 * deliberately ignores now_ms. Reliable acknowledgement timing starts at
 * now_ms, not when bytes were peeked; reliable bytes remain retained until a
 * matching acknowledgement, later owner-directed recovery, or reset. The
 * initial reliable commit does not count as a retransmission; each committed
 * retry increments the private count exactly once. Commit never performs
 * hardware I/O, reconstruction, CRC, or COBS work. If either session-state
 * mirror is FAULT, commit returns INTERNAL_ERROR without changing output
 * ownership; explicit reset is the only supported recovery.
 *
 * @param[in,out] context Initialized context with successfully peeked output.
 * @param[in] now_ms Monotonic acceptance time supplied by the caller.
 * @return OK, NOT_READY when no complete item was successfully peeked,
 * INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Commit_Output( HIL_Transport_Context_T* context,
                                                    uint32_t                 now_ms );

/**
 * @brief Read the next complete received Application message.
 *
 * @details Partial profile data is never exposed. On OK message_size is bytes
 * copied and the message is consumed. On BUFFER_TOO_SMALL it is required bytes
 * and the message remains unchanged. On NOT_READY it is zero. NULL with size
 * zero is a size query. Successful Transport delivery is not semantic
 * Application acceptance; any Application response is another opaque message.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[out] out_buffer Caller destination, or NULL only with size zero.
 * @param[in] out_buffer_size Writable destination bytes.
 * @param[out] message_size Copied/required complete-message size or zero.
 * @return OK, BUFFER_TOO_SMALL, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Read_Application_Data( HIL_Transport_Context_T* context,
                                                            uint8_t*                 out_buffer,
                                                            size_t  out_buffer_size,
                                                            size_t* message_size );

/**
 * @brief Retrieve and consume the oldest high-level Transport event.
 *
 * @details The MVP retains a private bounded FIFO whose depth is not part of the
 * public contract. This operation copies one fully initialized oldest event and
 * consumes exactly that event only after the complete copy succeeds. NOT_READY,
 * INVALID_ARGUMENT, and INTERNAL_ERROR leave event unchanged. Events do not
 * instruct callers to construct ACK, recovery, or handshake frames; such output
 * remains an internal Transport responsibility. Event storage and reading are
 * implemented. Link changes, handshake establishment, receive/protocol errors,
 * automatic session abandonment, and outbound Application delivery success or
 * failure generate their events. Inbound Application availability is reported by
 * application_message_pending rather than a separate delivery event.
 *
 * @param[in,out] context Initialized single-owner context.
 * @param[out] event Destination for one event; must not be NULL.
 * @return OK, NOT_READY, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Read_Event( HIL_Transport_Context_T* context,
                                                 HIL_Transport_Event_T*   event );

/**
 * @brief Obtain a consistent high-level status snapshot.
 *
 * @details This observational operation exposes role, link, session, operating
 * mode, pending-work indicators, and high-level failure only. Output pending is
 * set while either control or reliable initial/retry bytes are ready or peeked.
 * Reliable delivery pending is unaffected by control ownership and remains set
 * through reliable ACK wait and retry exhaustion until the owning session later
 * handles it. Event pending is a boolean derived from the validated private
 * FIFO; the snapshot never exposes its count or capacity. The snapshot does not
 * expose output type, private profile state, sequences, retry counts, borrowed
 * workspace pointers, or mutable handles. Core role/link/session/failure mirrors,
 * operating-mode metadata, and private pending-state metadata are validated before
 * they are copied into the snapshot; invariant failure returns INTERNAL_ERROR rather
 * than exposing invalid enums, booleans, or inconsistent state.
 *
 * @param[in] context Initialized single-owner context.
 * @param[out] status Snapshot destination; must not be NULL.
 * @return OK, INVALID_ARGUMENT, or INTERNAL_ERROR.
 */
HIL_Transport_Status_T HIL_TRANSPORT_Get_Status( const HIL_Transport_Context_T*   context,
                                                 HIL_Transport_Status_Snapshot_T* status );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_H */
