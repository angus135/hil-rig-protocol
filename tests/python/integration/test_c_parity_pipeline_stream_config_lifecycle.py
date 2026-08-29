"""Python equivalents for public caller-visible C integration scenarios."""

from __future__ import annotations

import itertools

import pytest

from hil_rig_protocol import (
    EventType,
    Failure,
    LinkState,
    OperatingMode,
    SessionState,
    Transport,
    TransportConfig,
    TransportConfigurationError,
    TransportStatus,
)

from .parity_helpers import (
    H2R,
    R2H,
    complete_application,
    deliver_bytes,
    disconnect_pair,
    endpoint_for_direction,
    establish,
    initialize_connected,
    make_pair,
    opposite,
    reconnect_pair,
    take_output,
    transfer_expect_ok,
)
from .transport_pair_harness import TransportPairHarness, drain_events


def _drain_disconnect_events(transport: Transport) -> None:
    events = drain_events(transport)
    assert [event.type for event in events] == [
        EventType.LINK_STATE_CHANGED,
        EventType.SESSION_RESET,
    ]
    assert events[-1].failure is Failure.LINK_LOST


def _hard_reconnect(pair: TransportPairHarness, now_ms: int) -> None:
    pair.link.clear()
    disconnect_pair(pair, now_ms)
    _drain_disconnect_events(pair.host)
    _drain_disconnect_events(pair.rig)
    reconnect_pair(pair, now_ms + 1)
    assert [event.type for event in drain_events(pair.host)] == [
        EventType.LINK_STATE_CHANGED
    ]
    assert [event.type for event in drain_events(pair.rig)] == [
        EventType.LINK_STATE_CHANGED
    ]
    pair.establish_clean_session()
    assert [event.type for event in drain_events(pair.host)] == [
        EventType.SESSION_ESTABLISHED
    ]
    assert [event.type for event in drain_events(pair.rig)] == [
        EventType.SESSION_ESTABLISHED
    ]


def _find_minimum_encoded_capacity(application_capacity: int) -> int:
    for encoded in range(1, 641):
        try:
            with Transport(
                role=__import__("hil_rig_protocol").Role.HOST,
                config=TransportConfig(
                    max_application_message_size=application_capacity,
                    max_encoded_frame_size=encoded,
                    session_seed=1,
                ),
            ):
                return encoded
        except TransportConfigurationError:
            pass
    raise AssertionError("no supported encoded capacity found")


def test_minimum_public_configuration_sizes_and_transfers_one_byte_end_to_end() -> None:
    minimum_encoded = _find_minimum_encoded_capacity(1)
    assert minimum_encoded > 1
    with pytest.raises(TransportConfigurationError):
        Transport(
            __import__("hil_rig_protocol").Role.HOST,
            TransportConfig(
                max_application_message_size=1,
                max_encoded_frame_size=minimum_encoded - 1,
                session_seed=1,
            ),
        )

    with make_pair(
        host_seed=1,
        host_sequence=0,
        rig_sequence=0,
        max_application_message_size=1,
        max_encoded_frame_size=minimum_encoded,
    ) as pair:
        establish(pair)
        complete_application(pair, H2R, b"\xa5")


def test_zero_handshake_retries_exhaust_after_initial_transmission_and_recover() -> None:
    with make_pair(
        host_seed=0x8123456789ABCDEF,
        host_sequence=100,
        rig_sequence=300,
        timeout_ms=5,
        max_retries=0,
        host_now_ms=90,
        rig_now_ms=90,
    ) as pair:
        initialize_connected(pair)
        pair.set_host_time(100)
        assert pair.process_host() is TransportStatus.OK
        initial = take_output(pair, H2R, now_ms=100)

        pair.set_host_time(104)
        assert pair.process_host() is TransportStatus.OK
        assert not pair.host.get_status().output_pending
        assert pair.host.get_status().reliable_delivery_pending

        pair.set_host_time(105)
        assert pair.process_host() is TransportStatus.OK
        status = pair.host.get_status()
        assert status.session_state is SessionState.RECOVERING
        assert status.last_failure is Failure.DELIVERY
        assert not status.reliable_delivery_pending
        assert status.output_pending
        assert pair.host.peek_output() != initial.data
        events = drain_events(pair.host)
        assert sum(e.type is EventType.SESSION_RESET for e in events) == 1
        assert sum(e.type is EventType.DELIVERY_FAILED for e in events) == 0
        assert pair.host.commit_output(106) is TransportStatus.OK

        pair.set_both_times(110)
        pair.establish_clean_session()
        drain_events(pair.host)
        drain_events(pair.rig)
        complete_application(pair, H2R, b"\x31")


def test_reduced_application_limit_rejects_plus_one_without_owning_work() -> None:
    minimum_encoded = _find_minimum_encoded_capacity(4)
    with make_pair(
        max_application_message_size=4,
        max_encoded_frame_size=minimum_encoded,
    ) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"12345") is TransportStatus.MESSAGE_TOO_LARGE
        snapshot = pair.host.get_status()
        assert not snapshot.reliable_delivery_pending
        assert not snapshot.output_pending
        assert pair.host.submit_application_data(b"1234") is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        assert pair.rig.read_application_data() == b"1234"


def test_complete_handshake_establishes_both_endpoints() -> None:
    with make_pair() as pair:
        establish(pair, drain=False)
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def _establish_with_chunk_size(chunk_size: int, host_now: int, rig_now: int) -> None:
    with make_pair(host_now_ms=host_now, rig_now_ms=rig_now) as pair:
        initialize_connected(pair)
        transfers = 0
        for _ in range(32):
            assert pair.process_host() is TransportStatus.OK
            assert pair.process_rig() is TransportStatus.OK
            for direction in (H2R, R2H):
                sender, _ = endpoint_for_direction(pair, direction)
                while sender.get_status().output_pending:
                    accepted = pair.accept_output(direction)
                    assert accepted.status is TransportStatus.OK
                    assert accepted.handle is not None
                    assert pair.link.queue_accepted_for_delivery(accepted.handle)
                    while pair.link.ready_byte_count(direction):
                        delivery = pair.deliver_ready(direction, max_bytes=chunk_size)
                        assert delivery.status is TransportStatus.OK
                        assert delivery.bytes_consumed == delivery.bytes_offered
                    transfers += 1
            if (
                pair.host.get_status().session_state is SessionState.ESTABLISHED
                and pair.rig.get_status().session_state is SessionState.ESTABLISHED
            ):
                assert transfers == 4
                return
            pair.advance_both_times()
        raise AssertionError("session did not establish")


def test_complete_handshake_survives_byte_at_a_time_delivery() -> None:
    _establish_with_chunk_size(1, 0, 0)


def test_complete_handshake_does_not_require_shared_clock_epoch() -> None:
    _establish_with_chunk_size(3, 0x10203040, 0x90807060)


@pytest.mark.parametrize(
    ("direction", "payload"),
    [(H2R, b"\x10"), (R2H, bytes(range(0x21, 0x28)))],
    ids=[
        "HostToRigDeliversSmallApplicationExactlyOnce",
        "RigToHostDeliversRepresentativeApplicationExactlyOnce",
    ],
)
def test_one_way_application_delivery_exactly_once(direction, payload: bytes) -> None:
    with make_pair() as pair:
        establish(pair)
        sender, receiver = endpoint_for_direction(pair, direction)
        assert sender.submit_application_data(payload) is TransportStatus.OK
        assert sender.get_status().reliable_delivery_pending
        transfer_expect_ok(pair, direction)
        transfer_expect_ok(pair, opposite(direction))
        assert receiver.read_application_data() == payload
        assert receiver.read_application_data() is None
        events = drain_events(sender)
        assert [event.type for event in events] == [EventType.DELIVERY_CONFIRMED]
        assert not sender.get_status().reliable_delivery_pending


def test_maximum_configured_application_size_transfers_in_either_direction() -> None:
    payload = bytes(index & 0xFF for index in range(512))
    for direction in (H2R, R2H):
        with make_pair() as pair:
            establish(pair)
            complete_application(pair, direction, payload)


def test_sequential_messages_advance_without_cross_transaction_contamination() -> None:
    payloads = [b"\x31\x32", b"\x40\x41\x42", b"\x50", b"\x60\x61\x62\x63"]
    with make_pair(host_sequence=100, rig_sequence=1000) as pair:
        establish(pair)
        for direction, payload in zip(itertools.cycle((H2R, R2H)), payloads):
            complete_application(pair, direction, payload)


def test_successful_submission_copies_caller_buffer_before_return() -> None:
    with make_pair() as pair:
        establish(pair)
        source = bytearray(b"qrst")
        original = bytes(source)
        assert pair.host.submit_application_data(source) is TransportStatus.OK
        source[:] = b"\xee" * len(source)
        transfer_expect_ok(pair, H2R)
        transfer_expect_ok(pair, R2H)
        assert pair.rig.read_application_data() == original


def test_pinned_reliable_output_remains_stable_when_peer_traffic_creates_control_output() -> None:
    with make_pair() as pair:
        establish(pair)
        host_payload = b"\x81\x82\x83"
        rig_payload = b"\x91\x92\x93\x94"
        assert pair.host.submit_application_data(host_payload) is TransportStatus.OK
        assert pair.rig.submit_application_data(rig_payload) is TransportStatus.OK

        rig_peek_before = pair.rig.peek_output()
        assert rig_peek_before is not None
        transfer_expect_ok(pair, H2R)
        assert pair.rig.peek_output() == rig_peek_before

        transfer_expect_ok(pair, R2H)  # rig reliable data
        transfer_expect_ok(pair, R2H)  # rig ACK for host data
        transfer_expect_ok(pair, H2R)  # host ACK for rig data

        assert pair.rig.read_application_data() == host_payload
        assert pair.host.read_application_data() == rig_payload
        assert [e.type for e in drain_events(pair.host)] == [EventType.DELIVERY_CONFIRMED]
        assert [e.type for e in drain_events(pair.rig)] == [EventType.DELIVERY_CONFIRMED]


def test_established_peers_exchange_crossed_messages_using_only_public_api() -> None:
    with make_pair() as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"host") is TransportStatus.OK
        assert pair.rig.submit_application_data(b"rig") is TransportStatus.OK
        for _ in range(8):
            pair.process_both()
            for direction in (H2R, R2H):
                sender, _ = endpoint_for_direction(pair, direction)
                while sender.get_status().output_pending:
                    transfer_expect_ok(pair, direction)
            if (
                not pair.host.get_status().reliable_delivery_pending
                and not pair.rig.get_status().reliable_delivery_pending
            ):
                break
        assert pair.rig.read_application_data() == b"host"
        assert pair.host.read_application_data() == b"rig"
        assert [e.type for e in drain_events(pair.host)] == [EventType.DELIVERY_CONFIRMED]
        assert [e.type for e in drain_events(pair.rig)] == [EventType.DELIVERY_CONFIRMED]


def test_application_frame_can_arrive_one_byte_at_a_time() -> None:
    with make_pair() as pair:
        establish(pair)
        payload = b"\x11\x22\x33\x44\x55"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        accepted = pair.accept_output(H2R)
        assert accepted.handle is not None
        assert pair.link.queue_accepted_for_delivery(accepted.handle)
        while pair.link.ready_byte_count(H2R):
            result = pair.deliver_ready(H2R, max_bytes=1)
            assert result.status is TransportStatus.OK
            assert result.bytes_consumed == 1
        transfer_expect_ok(pair, R2H)
        assert pair.rig.read_application_data() == payload


def test_application_frame_survives_deterministic_irregular_chunking() -> None:
    payload = bytes((index * 17) & 0xFF for index in range(173))
    chunks = itertools.cycle((2, 7, 3, 11, 1))
    with make_pair() as pair:
        establish(pair)
        assert pair.rig.submit_application_data(payload) is TransportStatus.OK
        accepted = pair.accept_output(R2H)
        assert accepted.handle is not None
        assert pair.link.queue_accepted_for_delivery(accepted.handle)
        while pair.link.ready_byte_count(R2H):
            result = pair.deliver_ready(R2H, max_bytes=next(chunks))
            assert result.status is TransportStatus.OK
            assert result.bytes_consumed == result.bytes_offered
        transfer_expect_ok(pair, H2R)
        assert pair.host.read_application_data() == payload


def test_ack_and_crossed_application_can_be_joined_into_one_receive_call() -> None:
    with make_pair() as pair:
        establish(pair)
        host_payload = b"\x31\x32"
        rig_payload = b"\x41\x42\x43"
        assert pair.host.submit_application_data(host_payload) is TransportStatus.OK
        assert pair.rig.submit_application_data(rig_payload) is TransportStatus.OK
        transfer_expect_ok(pair, H2R)

        ack = pair.accept_output(R2H)
        data = pair.accept_output(R2H)
        assert ack.handle is not None and data.handle is not None
        assert pair.link.queue_accepted_for_delivery(ack.handle)
        assert pair.link.queue_accepted_for_delivery(data.handle)
        joined = pair.deliver_ready(R2H)
        assert joined.status is TransportStatus.OK
        assert joined.bytes_consumed == joined.bytes_offered
        status = pair.host.get_status()
        assert not status.reliable_delivery_pending
        assert status.application_message_pending
        assert status.output_pending
        assert pair.host.read_application_data() == rig_payload
        assert pair.rig.read_application_data() == host_payload
        transfer_expect_ok(pair, H2R)
        assert not pair.rig.get_status().reliable_delivery_pending


def test_leading_and_repeated_delimiters_do_not_change_valid_application_outcome() -> None:
    with make_pair() as pair:
        establish(pair)
        payload = b"qrs"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        item = take_output(pair, H2R, now_ms=100)
        status, offered, consumed = deliver_bytes(pair, H2R, b"\x00\x00\x00\x00" + item.data)
        assert status is TransportStatus.OK
        assert consumed == offered
        assert pair.rig.read_application_data() == payload
        assert pair.rig.read_event() is None
        transfer_expect_ok(pair, R2H)


def test_disconnect_while_waiting_for_ack_discards_uncertain_send_and_unread_receive() -> None:
    with make_pair(host_seed=0xF1000001, host_sequence=300, rig_sequence=300) as pair:
        establish(pair)
        payload = b"\x11\x12\x13"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        assert pair.host.get_status().reliable_delivery_pending
        assert pair.rig.get_status().application_message_pending
        _hard_reconnect(pair, 110)
        assert not pair.host.get_status().reliable_delivery_pending
        assert not pair.rig.get_status().application_message_pending
        assert pair.rig.read_application_data() is None
        complete_application(pair, H2R, b"qrs")


def test_disconnect_discards_partial_incoming_frame_before_replacement_session() -> None:
    with make_pair(host_seed=0xF1000001, host_sequence=300, rig_sequence=300) as pair:
        establish(pair)
        payload = b"\x21\x22\x23\x24"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        accepted = pair.accept_output(H2R)
        assert accepted.handle is not None and accepted.size > 2
        assert pair.link.queue_accepted_for_delivery(accepted.handle)
        partial = pair.deliver_ready(H2R, max_bytes=accepted.size // 2)
        assert partial.status is TransportStatus.OK
        assert partial.bytes_consumed > 0
        assert pair.link.ready_byte_count(H2R) > 0
        assert not pair.rig.get_status().application_message_pending
        _hard_reconnect(pair, 210)
        complete_application(pair, H2R, b"qrs")


def test_disconnect_during_handshake_restarts_instead_of_resuming_old_attempt() -> None:
    with make_pair(host_seed=0xF2000002, host_sequence=400, rig_sequence=400) as pair:
        initialize_connected(pair)
        assert pair.process_host() is TransportStatus.OK
        accepted = pair.accept_output(H2R)
        assert accepted.handle is not None
        assert pair.link.hold_accepted(accepted.handle)
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        _hard_reconnect(pair, 20)
        assert pair.link.held_item_count(H2R) == 0
        complete_application(pair, H2R, b"qrs")


def test_observes_links_publishes_events_and_resets_explicitly() -> None:
    with make_pair() as pair:
        # Initial DISCONNECTED notification is a no-op.
        assert pair.host.notify_link_state(LinkState.DISCONNECTED, 1) is TransportStatus.OK
        assert pair.host.read_event() is None

        assert pair.host.notify_link_state(LinkState.CONNECTED, 2) is TransportStatus.OK
        snapshot = pair.host.get_status()
        assert snapshot.link_state is LinkState.CONNECTED
        assert snapshot.session_state is SessionState.CONNECTING
        event = pair.host.read_event()
        assert event is not None and event.type is EventType.LINK_STATE_CHANGED

        # Repeating the same physical state does not publish another event.
        assert pair.host.notify_link_state(LinkState.CONNECTED, 3) is TransportStatus.OK
        assert pair.host.read_event() is None

        assert pair.host.notify_link_state(LinkState.DISCONNECTED, 4) is TransportStatus.OK
        snapshot = pair.host.get_status()
        assert snapshot.link_state is LinkState.DISCONNECTED
        assert snapshot.session_state is SessionState.DISCONNECTED
        assert snapshot.last_failure is Failure.LINK_LOST
        events = drain_events(pair.host)
        assert [event.type for event in events] == [
            EventType.LINK_STATE_CHANGED,
            EventType.SESSION_RESET,
        ]
        assert events[-1].failure is Failure.LINK_LOST

        assert pair.host.reset() is TransportStatus.OK
        snapshot = pair.host.get_status()
        assert snapshot.last_failure is Failure.LOCAL_RESET
        assert snapshot.session_state is SessionState.DISCONNECTED
        assert not snapshot.event_pending
