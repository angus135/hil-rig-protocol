/**
 * @file transport_internal.h
 * @brief Common private ownership rules for Transport profile state.
 *
 * @details A selected profile places its state and bounded byte storage inside
 * HIL_Transport_Storage_T::workspace and records the private root in the public
 * context's reserved fields. No private pointer may escape through public
 * status, events, or messages. Public call buffers are borrowed only during a
 * call; submitted messages are copied before successful return.
 * A facade is completely zero-initialized before first Init. Profile Init must
 * publish the initialization cookie and private pointer only after atomic
 * success, reject a working context unchanged, and leave a failed first context
 * deterministically zero/uninitialized. Replacing retained setup is unsupported.
 *
 * Session recovery must clear negotiation, reliable delivery, parsers, partial
 * data, pinned output, and unread messages together so no work from an abandoned
 * session becomes visible later. Explicit caller reset also clears event
 * ownership, while automatic abandonment preserves unread events and appends a
 * SESSION_RESET event when capacity permits. Both retain copied configuration,
 * workspace ownership, endpoint role, and current link input.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_TRANSPORT_INTERNAL_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_TRANSPORT_INTERNAL_H

#include "hil_rig_protocol/transport/transport.h"

/** Facade marker written only after a profile initializes atomically. */
#define HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE ( UINT32_C( 0x48494C54 ) )

/**
 * @brief Profile-neutral metadata expected at the start of private state.
 *
 * @details Profiles may embed this as their first member and extend it with any
 * implementation-specific state. This type is private and does not constrain
 * future public ABI or workspace partitioning.
 */
typedef struct
{
    HIL_Transport_Config_T         config;
    HIL_Transport_Role_T           role;
    HIL_Transport_Link_State_T     link_state;
    HIL_Transport_Session_State_T  session_state;
    HIL_Transport_Operating_Mode_T operating_mode;
    uint8_t                        operating_mode_valid;
    HIL_Transport_Failure_T        last_failure;
} HIL_Transport_Internal_State_T;

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_TRANSPORT_INTERNAL_H */
