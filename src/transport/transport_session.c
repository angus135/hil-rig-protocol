#include "hil_rig_protocol/transport/transport_session.h"

static HIL_Transport_Session_Config_T HIL_TRANSPORT_Default_Session_Config(void)
{
    HIL_Transport_Session_Config_T config;

    config.keep_alive_interval_ms = 1000u;
    config.ack_timeout_ms = 250u;
    config.max_retries = 3u;
    config.max_payload_len = HIL_TRANSPORT_DEFAULT_MAX_PAYLOAD_SIZE;

    return config;
}

static void HIL_TRANSPORT_Init_Event(HIL_Transport_Event_T *event)
{
    event->type = HIL_TRANSPORT_EVENT_NONE;
    event->status = HIL_TRANSPORT_STATUS_OK;
    event->frame_type = HIL_TRANSPORT_FRAME_TYPE_DATA;
    event->state = HIL_TRANSPORT_STATE_IDLE;
    event->sequence = 0u;
    event->flags = 0u;
}

static void HIL_TRANSPORT_Init_Frame(
    HIL_Transport_Frame_T *frame,
    HIL_Transport_Frame_Type_T frame_type
)
{
    frame->header.version_major = HIL_TRANSPORT_WIRE_VERSION_MAJOR;
    frame->header.version_minor = HIL_TRANSPORT_WIRE_VERSION_MINOR;
    frame->header.frame_type = frame_type;
    frame->header.flags = 0u;
    frame->header.sequence = 0u;
    frame->header.ack_sequence = 0u;
    frame->header.payload_len = 0u;
    frame->header.crc32 = 0u;
    frame->payload = (const uint8_t *)0;
    frame->payload_len = 0u;
}

HIL_Transport_Status_T HIL_TRANSPORT_Session_Init(
    HIL_Transport_Session_T *session,
    HIL_Transport_Role_T role,
    const HIL_Transport_Session_Config_T *config
)
{
    HIL_Transport_Session_Config_T selected_config;

    if (session == (HIL_Transport_Session_T *)0)
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ((role != HIL_TRANSPORT_ROLE_HOST) && (role != HIL_TRANSPORT_ROLE_RIG))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if (config == (const HIL_Transport_Session_Config_T *)0)
    {
        selected_config = HIL_TRANSPORT_Default_Session_Config();
    }
    else
    {
        selected_config = *config;
    }

    session->role = role;
    session->state = HIL_TRANSPORT_STATE_IDLE;
    session->pending_state = HIL_TRANSPORT_STATE_IDLE;
    session->config = selected_config;
    session->stats.frames_tx = 0u;
    session->stats.frames_rx = 0u;
    session->stats.bytes_tx = 0u;
    session->stats.bytes_rx = 0u;
    session->stats.malformed_frames = 0u;
    session->stats.crc_errors = 0u;
    session->stats.sequence_errors = 0u;
    session->stats.duplicate_frames = 0u;
    session->stats.timeouts = 0u;
    session->stats.resyncs = 0u;
    session->next_sequence = 0u;
    session->expected_sequence = 0u;
    session->last_ack_sequence = 0u;
    session->last_rx_ms = 0u;
    session->last_tx_ms = 0u;
    session->state_change_deadline_ms = 0u;
    session->retry_count = 0u;
    session->state_change_pending = 0u;

    return HIL_TRANSPORT_STATUS_OK;
}

HIL_Transport_Status_T HIL_TRANSPORT_Session_Build_Data_Frame(
    HIL_Transport_Session_T *session,
    const uint8_t *payload,
    size_t payload_len,
    HIL_Transport_Frame_T *out_frame
)
{
    if ((session == (HIL_Transport_Session_T *)0) ||
        (out_frame == (HIL_Transport_Frame_T *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ((payload == (const uint8_t *)0) && (payload_len > 0u))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    HIL_TRANSPORT_Init_Frame(out_frame, HIL_TRANSPORT_FRAME_TYPE_DATA);
    out_frame->header.sequence = session->next_sequence;
    out_frame->header.payload_len = payload_len;
    out_frame->payload = payload;
    out_frame->payload_len = payload_len;

    /*
     * TODO: Apply sequence advancement, ACK policy, fragmentation rules, and
     * session statistics once reliability semantics are defined.
     */
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_Session_Build_Keep_Alive(
    HIL_Transport_Session_T *session,
    uint32_t now_ms,
    HIL_Transport_Frame_T *out_frame
)
{
    if ((session == (HIL_Transport_Session_T *)0) ||
        (out_frame == (HIL_Transport_Frame_T *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    HIL_TRANSPORT_Init_Frame(out_frame, HIL_TRANSPORT_FRAME_TYPE_KEEP_ALIVE);
    out_frame->header.flags = HIL_TRANSPORT_FLAG_KEEP_ALIVE;
    out_frame->header.sequence = session->next_sequence;

    /*
     * TODO: Define keep-alive cadence, timeout accounting, and whether
     * keep-alive frames consume sequence numbers.
     */
    (void)now_ms;

    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_Session_Request_State_Change(
    HIL_Transport_Session_T *session,
    HIL_Transport_State_T requested_state,
    uint32_t now_ms,
    HIL_Transport_Frame_T *out_frame
)
{
    if ((session == (HIL_Transport_Session_T *)0) ||
        (out_frame == (HIL_Transport_Frame_T *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    if ((requested_state < HIL_TRANSPORT_STATE_IDLE) ||
        (requested_state > HIL_TRANSPORT_STATE_RESERVED_7))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    HIL_TRANSPORT_Init_Frame(out_frame, HIL_TRANSPORT_FRAME_TYPE_CONTROL);
    out_frame->header.flags = HIL_TRANSPORT_FLAG_STATE_CHANGE;
    out_frame->header.sequence = session->next_sequence;
    session->pending_state = requested_state;

    /*
     * TODO: Define legal state-transition rules, state-change payload encoding,
     * retry deadlines, and acknowledgement semantics.
     */
    (void)now_ms;

    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_Session_Receive_Frame(
    HIL_Transport_Session_T *session,
    const HIL_Transport_Frame_T *frame,
    uint32_t now_ms,
    HIL_Transport_Event_T *out_event
)
{
    if (out_event != (HIL_Transport_Event_T *)0)
    {
        HIL_TRANSPORT_Init_Event(out_event);
    }

    if ((session == (HIL_Transport_Session_T *)0) ||
        (frame == (const HIL_Transport_Frame_T *)0) ||
        (out_event == (HIL_Transport_Event_T *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    out_event->frame_type = frame->header.frame_type;
    out_event->state = session->state;
    out_event->sequence = frame->header.sequence;
    out_event->flags = frame->header.flags;
    out_event->status = HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;

    /*
     * TODO: Validate version, sequence numbers, duplicate frames, ACK/NACK
     * fields, state-change control frames, and keep-alive updates.
     */
    (void)now_ms;

    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T HIL_TRANSPORT_Session_Tick(
    HIL_Transport_Session_T *session,
    uint32_t now_ms,
    HIL_Transport_Event_T *out_event
)
{
    if (out_event != (HIL_Transport_Event_T *)0)
    {
        HIL_TRANSPORT_Init_Event(out_event);
    }

    if ((session == (HIL_Transport_Session_T *)0) ||
        (out_event == (HIL_Transport_Event_T *)0))
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    out_event->state = session->state;
    out_event->status = HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;

    /*
     * TODO: Emit timeout, retry, keep-alive, and state-change events according
     * to the finalized session timing rules.
     */
    (void)now_ms;

    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}
