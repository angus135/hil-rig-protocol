/**
 * @file transport_types.h
 * @brief Stable caller-visible types for the HIL-RIG Transport facade.
 *
 * @details The declarations in this file describe only integration contracts.
 * They deliberately omit frame fields, parser state, handshake phases,
 * retransmission state, queues, and fragmentation/reassembly bookkeeping. Such
 * details belong to the selected private Transport profile and may change
 * without changing callers.
 *
 * Application messages are opaque byte strings. Transport delivery confirms
 * byte delivery only; it never means that the Application Layer accepted or
 * acted on a message. None of these C structures is a packed wire structure.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_TYPES_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_TYPES_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Minimum alignment of a non-NULL Transport workspace.
 *
 * @details This compile-time expression is usable with C11 `_Alignas` and C++
 * `alignas`. It is based on `max_align_t`, so it is sufficient for every MVP
 * private object placed directly in caller storage. Required_Storage_Size()
 * reports byte capacity assuming this alignment. A future Init() returns
 * INVALID_ARGUMENT for a non-NULL workspace that does not satisfy it.
 */
#if defined( __cplusplus )
#define HIL_TRANSPORT_WORKSPACE_ALIGNMENT ( alignof( max_align_t ) )
#elif defined( _MSC_VER )
/* MSVC's C standard library does not currently provide C11 max_align_t. */
#define HIL_TRANSPORT_WORKSPACE_ALIGNMENT ( __alignof( long double ) )
#else
#define HIL_TRANSPORT_WORKSPACE_ALIGNMENT ( _Alignof( max_align_t ) )
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/** Default maximum complete Application message accepted by the MVP profile. */
#define HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE ( 512u )

/** Default maximum complete encoded frame copied through Peek_Output(). */
#define HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE ( 640u )

/** Invalid session seed. A host configuration must not use this value. */
#define HIL_TRANSPORT_SESSION_SEED_INVALID ( UINT64_C( 0 ) )

/** Reserved session seed. A host configuration must not use this value. */
#define HIL_TRANSPORT_SESSION_SEED_RESERVED ( UINT64_MAX )

/**
 * @brief Result of a public Transport operation.
 *
 * @details These values are local API results and diagnostics. They are never
 * serialized as peer error values. More detailed parser, frame-codec, and
 * reliability errors remain private to the selected implementation profile.
 */
typedef enum
{
    /** The requested operation completed. */
    HIL_TRANSPORT_STATUS_OK = 0,

    /** A pointer, enum, size combination, or configuration value is invalid. */
    HIL_TRANSPORT_STATUS_INVALID_ARGUMENT,

    /** Caller output is too small; the size output reports required bytes. */
    HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL,

    /** The selected profile cannot support an otherwise valid configuration. */
    HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION,

    /** A complete submitted Application message exceeds the configured limit. */
    HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE,

    /** Bounded caller workspace cannot currently accept more work. */
    HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED,

    /** Reliable byte delivery failed under the configured policy. */
    HIL_TRANSPORT_STATUS_DELIVERY_FAILED,

    /** A configured high-level Transport deadline expired. */
    HIL_TRANSPORT_STATUS_TIMEOUT,

    /** No item is available or the operation must be retried later. */
    HIL_TRANSPORT_STATUS_NOT_READY,

    /** The API exists but its selected profile implementation is still a stub. */
    HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED,

    /** A private implementation invariant failed. */
    HIL_TRANSPORT_STATUS_INTERNAL_ERROR
} HIL_Transport_Status_T;

/** Logical endpoint role fixed for the lifetime of an initialized context. */
typedef enum
{
    /** Host-side protocol integration. */
    HIL_TRANSPORT_ROLE_HOST = 0,

    /** HIL-RIG firmware protocol integration. */
    HIL_TRANSPORT_ROLE_RIG
} HIL_Transport_Role_T;

/** State of the caller-owned byte-stream link. */
typedef enum
{
    /** The external link cannot currently carry bytes. */
    HIL_TRANSPORT_LINK_STATE_DISCONNECTED = 0,

    /** The external link is available to the owning caller. */
    HIL_TRANSPORT_LINK_STATE_CONNECTED
} HIL_Transport_Link_State_T;

/**
 * @brief Caller-selected local Transport operating mode.
 *
 * @details The mode is a stable policy input to HIL_TRANSPORT_Process(). It is
 * independent of the Transport session and every Application or firmware
 * lifecycle state. The mode is not negotiated or synchronized with the peer.
 * All three defined values are valid for the MVP, which may treat them
 * identically. A future extended profile may use them for private scheduling,
 * pacing, or flow-control policy without changing their public validity.
 */
typedef enum
{
    /** Ordinary Transport traffic policy. */
    HIL_TRANSPORT_OPERATING_MODE_NORMAL = 0,

    /** Policy intended for sustained transfer of opaque Application messages. */
    HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER,

    /** Policy intended to reduce new traffic during time-sensitive activity. */
    HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME
} HIL_Transport_Operating_Mode_T;

/**
 * @brief High-level Transport session state.
 *
 * @details This deliberately does not expose the selected profile's handshake
 * phase or recovery substate. Application messages are exchanged only after a
 * session reaches ESTABLISHED.
 */
typedef enum
{
    /** No usable physical link or Transport session exists. */
    HIL_TRANSPORT_SESSION_STATE_DISCONNECTED = 0,

    /** Transport is establishing a session using profile-private control work. */
    HIL_TRANSPORT_SESSION_STATE_CONNECTING,

    /** A Transport session is ready for complete Application messages. */
    HIL_TRANSPORT_SESSION_STATE_ESTABLISHED,

    /** Transport is abandoning failed session work and preparing recovery. */
    HIL_TRANSPORT_SESSION_STATE_RECOVERING,

    /**
     * A private invariant failed and normal progress is stopped.
     *
     * Explicit Reset is the only supported way for an initialized context to
     * leave this state.
     */
    HIL_TRANSPORT_SESSION_STATE_FAULT
} HIL_Transport_Session_State_T;

/** High-level reason associated with session failure or recovery. */
typedef enum
{
    /** No failure has been recorded. */
    HIL_TRANSPORT_FAILURE_NONE = 0,

    /** The caller reported that the physical link disconnected. */
    HIL_TRANSPORT_FAILURE_LINK_LOST,

    /** A configured peer-liveness deadline expired. */
    HIL_TRANSPORT_FAILURE_CONNECTION_TIMEOUT,

    /** Reliable delivery exhausted retries and forced normal session recovery. */
    HIL_TRANSPORT_FAILURE_DELIVERY,

    /** Received Transport input was malformed or incompatible with the session. */
    HIL_TRANSPORT_FAILURE_PROTOCOL,

    /** Caller workspace could not retain required incoming or outgoing work. */
    HIL_TRANSPORT_FAILURE_CAPACITY,

    /** The owning caller explicitly reset the Transport context. */
    HIL_TRANSPORT_FAILURE_LOCAL_RESET,

    /** A private implementation invariant failed. */
    HIL_TRANSPORT_FAILURE_INTERNAL
} HIL_Transport_Failure_T;

/** High-level event categories available to normal integrations. */
typedef enum
{
    /** Sentinel; never returned as a queued event. */
    HIL_TRANSPORT_EVENT_NONE = 0,

    /** A new Transport session became established. */
    HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED,

    /** The previous session was abandoned or reset. */
    HIL_TRANSPORT_EVENT_SESSION_RESET,

    /** One submitted message completed reliable Transport delivery. */
    HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED,

    /** One submitted message could not be delivered. */
    HIL_TRANSPORT_EVENT_DELIVERY_FAILED,

    /** Received data was rejected without exposing private frame diagnostics. */
    HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,

    /** Caller-owned capacity prevented retaining incoming or outgoing work. */
    HIL_TRANSPORT_EVENT_CAPACITY_EXHAUSTED,

    /** The external link state changed. */
    HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED
} HIL_Transport_Event_Type_T;

/**
 * @brief One queued high-level Transport event.
 *
 * @details Sequence numbers, frame categories, handshake steps, fragment
 * offsets, and queue slots are intentionally absent. Events report protocol
 * delivery rather than Application acceptance. A private producer constructs
 * every complete value before publication; fields not relevant to that event
 * are set to their zero/none values. The selected profile's retention depth is
 * private and is not encoded in this structure.
 */
typedef struct
{
    /** Event category. */
    HIL_Transport_Event_Type_T type;

    /**
     * Local public API status associated with the event.
     *
     * Private codec/parser/session numeric classifications are mapped before an
     * event is published and never appear in this field.
     */
    HIL_Transport_Status_T status;

    /** High-level failure cause, or HIL_TRANSPORT_FAILURE_NONE. */
    HIL_Transport_Failure_T failure;

    /** Complete Application message bytes related to a capacity failure. */
    size_t required_capacity;
} HIL_Transport_Event_T;

/**
 * @brief High-level read-only status snapshot.
 *
 * @details The snapshot is observational. It contains no mutable pointers and
 * does not expose profile-private session, frame, timer, or queue state.
 */
typedef struct
{
    /** Endpoint role selected during initialization. */
    HIL_Transport_Role_T role;

    /** Latest physical-link state reported by the caller. */
    HIL_Transport_Link_State_T link_state;

    /** Current high-level Transport session state. */
    HIL_Transport_Session_State_T session_state;

    /** Latest valid local operating mode supplied to Process(). */
    HIL_Transport_Operating_Mode_T operating_mode;

    /** Nonzero after Process() has accepted an operating-mode value. */
    uint8_t operating_mode_valid;

    /** Nonzero while control or reliable opaque encoded output can be peeked. */
    uint8_t output_pending;

    /** Nonzero while one complete received Application message is unread. */
    uint8_t application_message_pending;

    /** Nonzero while at least one event is unread; no count or depth is exposed. */
    uint8_t event_pending;

    /** Nonzero only while a reliable transmission remains owned by Transport. */
    uint8_t reliable_delivery_pending;

    /** Most recent high-level session failure. */
    HIL_Transport_Failure_T last_failure;
} HIL_Transport_Status_Snapshot_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_TYPES_H */
