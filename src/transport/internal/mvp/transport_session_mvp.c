#include "transport_session_mvp.h"

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Init( HIL_Transport_Mvp_Session_T* session,
                                                       HIL_Transport_Role_T         role,
                                                       uint64_t                     session_seed,
                                                       uint16_t initial_reliable_sequence )
{
    /*
     * TODO: Validate session/role; require a usable nonreserved seed for HOST and
     * INVALID for RIG; copy the initial sequence; and initialize link/session/
     * handshake/reliable state to DISCONNECTED/INACTIVE/IDLE. Clear identities,
     * sequences, ACK/duplicate state, retry counter, timestamps, and failure.
     * Allocate nothing and perform no I/O.
     */
    ( void )session;
    ( void )role;
    ( void )session_seed;
    ( void )initial_reliable_sequence;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Session_Reset( HIL_Transport_Mvp_Session_T* session,
                                                        HIL_Transport_Failure_T      failure )
{
    /*
     * TODO: Validate session/failure; preserve role, link observation, configured
     * initial sequence, and advanced host identity cursor; then clear active
     * identity, handshake, sequences, ACK/duplicate state, reliable ownership,
     * and retry/timing. Record failure and enter
     * DISCONNECTED for a disconnected link or RECOVERING otherwise. The profile
     * separately abandons the single submitted/received message, parser body,
     * pinned output, encoded retry bytes, and all pending events.
     */
    ( void )session;
    ( void )failure;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Reserve_Sequence( HIL_Transport_Mvp_Session_T* session,
                                            uint16_t*                    sequence )
{
    if ( sequence == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *sequence = 0u;
    if ( session == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( session->reliable_state < HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( session->reliable_state > HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }
    if ( session->reliable_state != HIL_TRANSPORT_MVP_RELIABLE_IDLE )
    {
        return HIL_TRANSPORT_STATUS_NOT_READY;
    }
    if ( ( session->retained_reliable_frame_type != HIL_TRANSPORT_MVP_FRAME_INVALID )
         || ( session->retained_transmit_sequence != 0u )
         || ( session->retransmissions_committed != 0u )
         || ( session->reliable_last_committed_ms != 0u ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    /* The candidate is neither published nor consumed until encoding and ACK succeed. */
    *sequence = session->next_transmit_sequence;
    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Sequence( HIL_Transport_Mvp_Session_T*            session,
                                             uint16_t                                sequence,
                                             HIL_Transport_Mvp_Rx_Sequence_Result_T* result )
{
    /*
     * TODO: Validate and classify the expected sequence, the exact last accepted
     * duplicate, or incompatible traffic using approved wrap rules. Deliver and
     * advance expected data once; request another ACK without redelivery for the
     * duplicate; and require complete session restart for incompatibility. The
     * private result is mapped to public status/events by the MVP profile.
     */
    ( void )session;
    ( void )sequence;
    ( void )result;
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement( HIL_Transport_Mvp_Session_T*    session,
                                                    uint16_t                        sequence,
                                                    HIL_Transport_Mvp_Ack_Result_T* result )
{
    if ( result == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    *result = HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED;
    if ( session == NULL )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }
    if ( ( session->reliable_state < HIL_TRANSPORT_MVP_RELIABLE_IDLE )
         || ( session->reliable_state > HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED ) )
    {
        return HIL_TRANSPORT_STATUS_INTERNAL_ERROR;
    }

    /*
     * A timeout authorizes an unpinned retry; it does not invalidate the ACK
     * for the previously committed transmission. Once retry bytes are peeked,
     * pin-until-commit/reset ownership takes precedence for this MVP.
     */
    if ( ( ( session->reliable_state == HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
           || ( session->reliable_state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY ) )
         && ( sequence == session->retained_transmit_sequence ) )
    {
        *result = HIL_TRANSPORT_MVP_ACK_MATCHED;
    }
    return HIL_TRANSPORT_STATUS_OK;
}
