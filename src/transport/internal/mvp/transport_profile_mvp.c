/**
 * @file transport_profile_mvp.c
 * @brief Intentional stubs for the default minimal Transport profile.
 *
 * @details The eventual MVP uses simple session establishment, one complete
 * Application message per frame, framing plus integrity, and one outstanding
 * reliable transmission. It does not provide fragmentation, reliable
 * pipelining, flow control, or multiple queued messages. Every operation remains
 * deliberately non-functional in this API/source-architecture change.
 */
#include "../transport_profile.h"

#include "../transport_internal.h"
#include "transport_types_mvp.h"

void HIL_TRANSPORT_PROFILE_Default_Config( HIL_Transport_Config_T* config )
{
    if ( config == NULL )
    {
        return;
    }

    config->max_application_message_size = HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE;
    config->max_encoded_frame_size       = HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;
    config->session_seed                 = HIL_TRANSPORT_SESSION_SEED_INVALID;
    config->initial_reliable_sequence    = 0u;
    config->connection_timeout_ms        = 0u;
    config->retransmit_timeout_ms        = 0u;
    config->max_retries                  = 0u;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Required_Storage_Size( const HIL_Transport_Config_T* config,
                                             size_t*                       required_size )
{
    /*
     * TODO: Clear required_size; require nonzero message/frame limits; validate
     * role-independent numeric policy without deciding whether session_seed is
     * valid for a host or rig; return UNSUPPORTED_CONFIGURATION when
     * connection_timeout_ms is nonzero because the MVP has no idle-liveness
     * traffic; reject a relationship in which one complete message cannot fit
     * one MVP frame; and use checked aligned arithmetic for
     * HIL_Transport_Mvp_Root_T plus one submitted message, one stable
     * encoded/retry item, parser scratch, and one received message. Assume the
     * public HIL_TRANSPORT_WORKSPACE_ALIGNMENT contract and expose no private
     * partition offsets.
     */
    ( void )config;
    if ( required_size != NULL )
    {
        *required_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Init( HIL_Transport_Context_T*       context,
                                                   HIL_Transport_Role_T           role,
                                                   const HIL_Transport_Config_T*  config,
                                                   const HIL_Transport_Storage_T* storage )
{
    /*
     * TODO: Require a completely zero-initialized facade and reject an already
     * initialized context without changing it. Validate all other inputs and the
     * required non-NULL configuration. Require nonzero message/frame limits;
     * require HOST seed to be neither INVALID nor RESERVED; require RIG seed to
     * be exactly INVALID; return UNSUPPORTED_CONFIGURATION for nonzero
     * connection_timeout_ms; and reject a relationship requiring fragmentation.
     * Require workspace alignment/capacity, partition with checked arithmetic,
     * copy configuration, and initialize MVP root/session atomically. Publish
     * facade fields and the initialization cookie only after every check and
     * write succeeds; a failed first initialization must leave every context
     * field zero. A rig later adopts a valid host-proposed identity. Never
     * replace a working context's role, configuration, or workspace, and perform
     * no I/O or allocation.
     */
    ( void )context;
    ( void )role;
    ( void )config;
    ( void )storage;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Reset( HIL_Transport_Context_T* context )
{
    /*
     * TODO: Validate ownership; clear every previously queued event without
     * enqueuing SESSION_RESET for this caller-initiated action; abandon the
     * single submitted/received message, parser body, sequence/ACK/retry state,
     * pinned output, and encoded bytes. Retain configuration, workspace, role,
     * link observation, and advanced host identity cursor. Record LOCAL_RESET;
     * enter DISCONNECTED when the link is disconnected or RECOVERING when it is
     * connected so a later Process starts a fresh session. This explicit reset
     * is the only supported operation that clears terminal FAULT on an
     * initialized context; it never changes configuration, role, or workspace.
     */
    ( void )context;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Notify_Link_State( HIL_Transport_Context_T*   context,
                                         HIL_Transport_Link_State_T link_state, uint32_t now_ms )
{
    /*
     * TODO: Validate inputs, record caller-owned link state/time, start private
     * session establishment on connection, and perform full session reset on
     * disconnection. Never call, configure, or poll external hardware.
     */
    ( void )context;
    ( void )link_state;
    ( void )now_ms;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Submit_Application_Data( HIL_Transport_Context_T* context,
                                               const uint8_t* payload, size_t payload_len )
{
    /*
     * TODO: Validate the pointer/size and complete one-frame bound. Require public
     * session state ESTABLISHED; otherwise return NOT_READY, retain no input
     * pointer, and change nothing. The MVP does not queue before establishment.
     * Reserve its sole message/reliable capacity before sequence allocation,
     * copy synchronously, and publish atomically. A second reliable item is
     * forbidden while the first is prepared, pinned, awaiting ACK, or retrying.
     */
    ( void )context;
    ( void )payload;
    ( void )payload_len;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Receive_Bytes( HIL_Transport_Context_T* context,
                                                            const uint8_t* data, size_t data_len,
                                                            size_t* bytes_consumed )
{
    /*
     * TODO: Clear and require bytes_consumed, validate the borrowed chunk, accept
     * arbitrary boundaries, and advance the exact count per accepted byte.
     * Preserve a retryable suffix on temporary capacity exhaustion; consume
     * malformed data only through a known resynchronization boundary; never
     * overwrite unread output or expose partial Application messages. Map
     * malformed/integrity/incompatible/stale private classifications to one
     * public PROTOCOL_ERROR event after consuming through the appropriate
     * resynchronization boundary; map capacity to CAPACITY_EXHAUSTED. Deliver an
     * expected reliable frame once, re-ACK its exact duplicate without
     * redelivery, and restart the complete session for incompatible traffic. A
     * private invariant sets FAULT/INTERNAL failure and returns INTERNAL_ERROR.
     */
    ( void )context;
    ( void )data;
    ( void )data_len;
    if ( bytes_consumed != NULL )
    {
        *bytes_consumed = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Process( HIL_Transport_Context_T* context, uint32_t now_ms,
                               HIL_Transport_Operating_Mode_T operating_mode )
{
    /*
     * TODO: Accept NORMAL, BULK_TRANSFER, and QUIET_REAL_TIME, recording the
     * latest valid value (the MVP may schedule them identically). Reject every
     * other numeric value with INVALID_ARGUMENT without changing the recorded
     * mode or progressing any work. Progress private INITIATE, RESPONSE, CONFIRM
     * establishment and one-message-per-frame work. A rig becomes ESTABLISHED
     * after valid CONFIRM and offers its ACK; a host becomes ESTABLISHED only
     * after the matching CONFIRM ACK. A lost CONFIRM or ACK retransmits the same
     * CONFIRM; an established rig re-ACKs an identical duplicate without another
     * transition. Apply the global retransmission timeout and max_retries to all
     * reliable handshake and Application work, retaining identical frame bytes,
     * session identity, and sequence. Accept only the matching ACK and advance
     * once. On accepted-Application retry exhaustion, return DELIVERY_FAILED as
     * appropriate, publish its DELIVERY_FAILED event, abandon the uncertain
     * session, and enter recovery. On handshake retry
     * exhaustion, publish no Application delivery event, abandon the incomplete
     * handshake, and restart establishment using the host's next derived session
     * identity. Incompatible identities likewise abandon the complete session.
     * Map configured deadlines to TIMEOUT. In FAULT, stop normal progress until
     * explicit Reset. Any invariant failure enters FAULT, records INTERNAL, and
     * returns INTERNAL_ERROR. Never infer Application lifecycle or call I/O.
     */
    ( void )context;
    ( void )now_ms;
    ( void )operating_mode;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Peek_Output( HIL_Transport_Context_T* context,
                                                          uint8_t*                 out_buffer,
                                                          size_t                   out_buffer_size,
                                                          size_t*                  output_size )
{
    /*
     * TODO: Clear and require output_size, validate size-query combinations,
     * preserve an existing pinned selection, report required bytes without
     * consuming on insufficient capacity, and copy/pin a complete item only
     * after success. Repeated peeks return identical bytes until commit. Do not
     * start timing or release the sole reliable ownership slot.
     */
    ( void )context;
    ( void )out_buffer;
    ( void )out_buffer_size;
    if ( output_size != NULL )
    {
        *output_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Commit_Output( HIL_Transport_Context_T* context,
                                                            uint32_t                 now_ms )
{
    /*
     * TODO: Require successfully pinned complete output, record external
     * acceptance time once, release explicitly unreliable output, and retain
     * reliable bytes until matching acknowledgement, failure, or reset. Commit
     * must not permit another reliable item to replace retained output. Perform
     * no hardware call and never commit a partial item.
     */
    ( void )context;
    ( void )now_ms;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_PROFILE_Read_Application_Data( HIL_Transport_Context_T* context, uint8_t* out_buffer,
                                             size_t out_buffer_size, size_t* message_size )
{
    /*
     * TODO: Clear and require message_size, expose only the oldest complete
     * message, support a size query, leave it unchanged on insufficient output,
     * and release private storage only after a complete successful copy. Report
     * Transport delivery only, never Application semantic acceptance.
     */
    ( void )context;
    ( void )out_buffer;
    ( void )out_buffer_size;
    if ( message_size != NULL )
    {
        *message_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Read_Event( HIL_Transport_Context_T* context,
                                                         HIL_Transport_Event_T*   event )
{
    /*
     * TODO: Validate initialized ownership and non-NULL event, copy one fully
     * initialized high-level event, and consume it only after success. Keep
     * frame/parser/handshake details and Application outcomes private. The
     * current stub defensively clears a non-NULL destination.
     */
    ( void )context;
    if ( event != NULL )
    {
        *event = ( HIL_Transport_Event_T ){ 0 };
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_PROFILE_Get_Status( const HIL_Transport_Context_T*   context,
                                                         HIL_Transport_Status_Snapshot_T* status )
{
    /*
     * TODO: Validate initialized ownership and non-NULL status, then copy one
     * consistent high-level snapshot. FAULT must expose session state FAULT and
     * INTERNAL failure; explicit reset exposes LOCAL_RESET and the link-derived
     * DISCONNECTED/RECOVERING state. Expose no private pointers, parser/session
     * substates, handshake phases, or sequences. The current stub defensively
     * clears a non-NULL destination.
     */
    ( void )context;
    if ( status != NULL )
    {
        *status = ( HIL_Transport_Status_Snapshot_T ){ 0 };
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}
