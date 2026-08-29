"""Shared helpers for public-API parity with the C Transport integration suite."""

from __future__ import annotations

from dataclasses import dataclass

from hil_rig_protocol import (
    EventType,
    Failure,
    LinkState,
    SessionState,
    Transport,
    TransportConfig,
    TransportEvent,
    TransportStatus,
)

from .transport_pair_harness import (
    TransportPairHarness,
    TransportTestDirection,
    TransportTestOutputItem,
    drain_events,
)

H2R = TransportTestDirection.HOST_TO_RIG
R2H = TransportTestDirection.RIG_TO_HOST


def make_pair(
    *,
    host_seed: int = 0xA102030405060708,
    host_sequence: int = 100,
    rig_sequence: int = 700,
    timeout_ms: int = 10,
    max_retries: int = 2,
    max_application_message_size: int = 512,
    max_encoded_frame_size: int = 640,
    host_now_ms: int = 0,
    rig_now_ms: int = 0,
) -> TransportPairHarness:
    return TransportPairHarness(
        host_config=TransportConfig(
            max_application_message_size=max_application_message_size,
            max_encoded_frame_size=max_encoded_frame_size,
            session_seed=host_seed,
            initial_reliable_sequence=host_sequence,
            retransmit_timeout_ms=timeout_ms,
            max_retries=max_retries,
        ),
        rig_config=TransportConfig(
            max_application_message_size=max_application_message_size,
            max_encoded_frame_size=max_encoded_frame_size,
            session_seed=0,
            initial_reliable_sequence=rig_sequence,
            retransmit_timeout_ms=timeout_ms,
            max_retries=max_retries,
        ),
        host_now_ms=host_now_ms,
        rig_now_ms=rig_now_ms,
    )


def initialize_connected(pair: TransportPairHarness, *, drain_link_events: bool = True) -> None:
    assert pair.connect() == (TransportStatus.OK, TransportStatus.OK)
    if drain_link_events:
        assert [event.type for event in drain_events(pair.host)] == [EventType.LINK_STATE_CHANGED]
        assert [event.type for event in drain_events(pair.rig)] == [EventType.LINK_STATE_CHANGED]


def establish(pair: TransportPairHarness, *, drain: bool = True) -> None:
    initialize_connected(pair, drain_link_events=drain)
    pair.establish_clean_session()
    assert pair.host.get_status().session_state is SessionState.ESTABLISHED
    assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
    if drain:
        host_events = drain_events(pair.host)
        rig_events = drain_events(pair.rig)
        assert sum(event.type is EventType.SESSION_ESTABLISHED for event in host_events) == 1
        assert sum(event.type is EventType.SESSION_ESTABLISHED for event in rig_events) == 1


def endpoint_for_direction(
    pair: TransportPairHarness, direction: TransportTestDirection
) -> tuple[Transport, Transport]:
    if direction is H2R:
        return pair.host, pair.rig
    return pair.rig, pair.host


def opposite(direction: TransportTestDirection) -> TransportTestDirection:
    return R2H if direction is H2R else H2R


def set_sender_time(
    pair: TransportPairHarness, direction: TransportTestDirection, now_ms: int
) -> None:
    if direction is H2R:
        pair.set_host_time(now_ms)
    else:
        pair.set_rig_time(now_ms)


def take_output(
    pair: TransportPairHarness,
    direction: TransportTestDirection,
    *,
    now_ms: int | None = None,
) -> TransportTestOutputItem:
    if now_ms is not None:
        set_sender_time(pair, direction, now_ms)
    accepted = pair.accept_output(direction)
    assert accepted.status is TransportStatus.OK
    assert accepted.handle is not None
    item = pair.link.take_accepted(accepted.handle)
    assert item is not None
    assert item.data
    return item


def accept_output(
    pair: TransportPairHarness,
    direction: TransportTestDirection,
    *,
    now_ms: int | None = None,
) -> TransportTestOutputItem:
    """Accept/commit output but leave the complete item owned by the link."""
    if now_ms is not None:
        set_sender_time(pair, direction, now_ms)
    accepted = pair.accept_output(direction)
    assert accepted.status is TransportStatus.OK
    assert accepted.handle is not None
    item = pair.link.accepted_item(accepted.handle)
    assert item is not None
    return item


def deliver_bytes(
    pair: TransportPairHarness,
    direction: TransportTestDirection,
    data: bytes,
    *,
    max_bytes: int | None = None,
) -> tuple[TransportStatus, int, int]:
    pair.link.inject_ready_bytes(direction, data)
    result = pair.deliver_ready(direction, max_bytes=max_bytes)
    assert result.status is not None
    return result.status, result.bytes_offered, result.bytes_consumed


def deliver_item(pair: TransportPairHarness, item: TransportTestOutputItem) -> TransportStatus:
    pair.link.inject_ready_bytes(item.handle.direction, item.data)
    result = pair.deliver_ready(item.handle.direction)
    assert result.status is not None
    assert result.bytes_consumed == len(item.data)
    return result.status


def transfer_expect_ok(pair: TransportPairHarness, direction: TransportTestDirection) -> bytes:
    result = pair.transfer_one_output(direction)
    assert result.accept.status is TransportStatus.OK
    assert result.accept.handle is not None
    assert result.delivery is not None
    assert result.delivery.status is TransportStatus.OK
    assert result.delivery.bytes_consumed == result.delivery.bytes_offered
    return b""  # Transfer helper intentionally does not expose committed bytes.


@dataclass(frozen=True, slots=True)
class HandshakeTraffic:
    initiate: bytes
    response: bytes
    confirm: bytes
    final_ack: bytes


def handshake_through_final_ack(
    pair: TransportPairHarness,
    *,
    base_time: int = 10,
    deliver_final_ack: bool = False,
) -> HandshakeTraffic:
    assert pair.process_host() is TransportStatus.OK
    initiate = take_output(pair, H2R, now_ms=base_time).data
    assert deliver_bytes(pair, H2R, initiate)[0] is TransportStatus.OK

    assert pair.process_rig() is TransportStatus.OK
    response = take_output(pair, R2H, now_ms=base_time + 1).data
    assert deliver_bytes(pair, R2H, response)[0] is TransportStatus.OK

    assert pair.process_host() is TransportStatus.OK
    confirm = take_output(pair, H2R, now_ms=base_time + 2).data
    assert deliver_bytes(pair, H2R, confirm)[0] is TransportStatus.OK

    final_ack = take_output(pair, R2H, now_ms=base_time + 3).data
    if deliver_final_ack:
        assert deliver_bytes(pair, R2H, final_ack)[0] is TransportStatus.OK
    return HandshakeTraffic(initiate, response, confirm, final_ack)


def assert_exactly_one_establishment(events: list[TransportEvent]) -> None:
    assert sum(event.type is EventType.SESSION_ESTABLISHED for event in events) == 1
    assert not any(event.type is EventType.PROTOCOL_ERROR for event in events)
    assert not any(event.type is EventType.SESSION_RESET for event in events)


def complete_application(
    pair: TransportPairHarness,
    direction: TransportTestDirection,
    payload: bytes,
    *,
    base_time: int = 100,
) -> None:
    sender, receiver = endpoint_for_direction(pair, direction)
    assert sender.submit_application_data(payload) is TransportStatus.OK
    item = take_output(pair, direction, now_ms=base_time)
    assert deliver_item(pair, item) is TransportStatus.OK
    assert receiver.read_application_data() == payload
    ack = take_output(pair, opposite(direction), now_ms=base_time + 1)
    assert deliver_item(pair, ack) is TransportStatus.OK
    confirmation = sender.read_event()
    assert confirmation is not None
    assert confirmation.type is EventType.DELIVERY_CONFIRMED
    assert confirmation.status is TransportStatus.OK
    assert confirmation.failure is Failure.NONE


def saturate_event_queue_with_protocol_errors(
    pair: TransportPairHarness,
    endpoint: Transport,
) -> None:
    direction = R2H if endpoint.role.value == 0 else H2R
    malformed = b"\x05\x11\x22\x00"
    for _ in range(1024):
        pair.link.inject_ready_bytes(direction, malformed)
        delivery = pair.deliver_ready(direction)
        assert delivery.status in (TransportStatus.OK, TransportStatus.CAPACITY_EXHAUSTED)
        assert delivery.bytes_consumed == len(malformed)
        if delivery.status is TransportStatus.CAPACITY_EXHAUSTED:
            break
    else:
        raise AssertionError("event queue did not saturate")

    freed = endpoint.read_event()
    assert freed is not None
    assert freed.type is EventType.PROTOCOL_ERROR
    resumed = pair.deliver_zero(direction)
    assert resumed.status is TransportStatus.OK
    second = pair.deliver_zero(direction)
    assert second.status is TransportStatus.OK
    assert endpoint.get_status().event_pending


def disconnect_pair(pair: TransportPairHarness, now_ms: int) -> None:
    pair.set_both_times(now_ms)
    assert pair.host.notify_link_state(LinkState.DISCONNECTED, now_ms) is TransportStatus.OK
    assert pair.rig.notify_link_state(LinkState.DISCONNECTED, now_ms) is TransportStatus.OK


def reconnect_pair(pair: TransportPairHarness, now_ms: int) -> None:
    pair.set_both_times(now_ms)
    assert pair.host.notify_link_state(LinkState.CONNECTED, now_ms) is TransportStatus.OK
    assert pair.rig.notify_link_state(LinkState.CONNECTED, now_ms) is TransportStatus.OK


def trigger_retry(
    pair: TransportPairHarness, direction: TransportTestDirection, timeout_ms: int
) -> None:
    if direction is H2R:
        pair.advance_host_time(timeout_ms)
        status = pair.process_host()
    else:
        pair.advance_rig_time(timeout_ms)
        status = pair.process_rig()
    assert status in (TransportStatus.OK, TransportStatus.DELIVERY_FAILED)
