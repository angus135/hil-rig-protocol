"""Capacity/backpressure and malformed-input parity with C integration tests."""

from __future__ import annotations

from hil_rig_protocol import EventType, Failure, SessionState, TransportStatus

from .parity_helpers import (
    H2R,
    R2H,
    complete_application,
    deliver_bytes,
    establish,
    initialize_connected,
    make_pair,
    saturate_event_queue_with_protocol_errors,
    take_output,
    transfer_expect_ok,
)
from .transport_pair_harness import drain_events


def _protocol_error(transport) -> None:
    event = transport.read_event()
    assert event is not None
    assert event.type is EventType.PROTOCOL_ERROR
    assert event.failure is Failure.PROTOCOL


def _fill_event_queue(pair, transport) -> int:
    direction = R2H if transport is pair.host else H2R
    malformed = b"\x05\x11\x22\x00"
    queued = 0
    for _ in range(1024):
        pair.link.inject_ready_bytes(direction, malformed)
        result = pair.deliver_ready(direction)
        assert result.status in (TransportStatus.OK, TransportStatus.CAPACITY_EXHAUSTED)
        assert result.bytes_consumed == len(malformed)
        if result.status is TransportStatus.CAPACITY_EXHAUSTED:
            break
        queued += 1
    else:
        raise AssertionError("event queue did not fill")
    freed = transport.read_event()
    assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
    assert pair.deliver_zero(direction).status is TransportStatus.OK
    return queued


def test_corrupted_application_is_rejected_then_original_retry_delivers_once() -> None:
    with make_pair(
        host_seed=0xD2340000, host_sequence=50, rig_sequence=900, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        payload = b"\x11\x12\x13\x14"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        pair.set_host_time(100)
        accepted = pair.accept_output(H2R)
        assert accepted.handle is not None and accepted.size > 3
        assert pair.link.corrupt_accepted_byte(accepted.handle, 2, 1)
        assert pair.link.queue_accepted_for_delivery(accepted.handle)
        assert pair.deliver_ready(H2R).status is TransportStatus.OK
        assert pair.rig.read_application_data() is None
        _protocol_error(pair.rig)
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        pair.set_host_time(110)
        assert pair.process_host() is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        transfer_expect_ok(pair, R2H)
        assert pair.rig.read_application_data() == payload
        assert pair.rig.read_application_data() is None


def test_corrupted_ack_cannot_falsely_complete_delivery_and_duplicate_is_reacked() -> None:
    with make_pair(
        host_seed=0xD2340000, host_sequence=50, rig_sequence=900, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        payload = b"\x21\x22\x23"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        pair.set_host_time(200)
        transfer_expect_ok(pair, H2R)
        ack = pair.accept_output(R2H)
        assert ack.handle is not None and ack.size > 3
        assert pair.link.corrupt_accepted_byte(ack.handle, 2, 1)
        assert pair.link.queue_accepted_for_delivery(ack.handle)
        assert pair.deliver_ready(R2H).status is TransportStatus.OK
        assert pair.host.get_status().reliable_delivery_pending
        _protocol_error(pair.host)
        pair.set_host_time(210)
        assert pair.process_host() is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        transfer_expect_ok(pair, R2H)
        assert pair.rig.read_application_data() == payload
        assert pair.rig.read_application_data() is None
        event = pair.host.read_event()
        assert event is not None and event.type is EventType.DELIVERY_CONFIRMED


def test_malformed_cobs_then_valid_application_in_same_chunk_resynchronizes() -> None:
    with make_pair() as pair:
        establish(pair)
        payload = b"123"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        valid = take_output(pair, H2R, now_ms=100)
        status, offered, consumed = deliver_bytes(pair, H2R, b"\x05\x11\x22\x00" + valid.data)
        assert status is TransportStatus.OK and consumed == offered
        _protocol_error(pair.rig)
        assert pair.rig.read_application_data() == payload
        transfer_expect_ok(pair, R2H)


def test_oversized_body_then_valid_application_in_same_chunk_resynchronizes() -> None:
    with make_pair() as pair:
        establish(pair)
        payload = b"AB"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        valid = take_output(pair, H2R, now_ms=100)
        oversized = b"\x11" * (640 + 8) + b"\x00"
        status, offered, consumed = deliver_bytes(pair, H2R, oversized + valid.data)
        assert status is TransportStatus.OK and consumed == offered
        _protocol_error(pair.rig)
        assert pair.rig.read_application_data() == payload
        transfer_expect_ok(pair, R2H)


def test_unread_application_retains_next_frame_and_exact_caller_suffix() -> None:
    with make_pair(host_seed=0xD2340000, host_sequence=50, rig_sequence=900) as pair:
        establish(pair)
        first, second = b"\x11\x12\x13", b"\x21\x22\x23\x24"
        assert pair.host.submit_application_data(first) is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        transfer_expect_ok(pair, R2H)
        event = pair.host.read_event()
        assert event is not None and event.type is EventType.DELIVERY_CONFIRMED
        assert pair.host.submit_application_data(second) is TransportStatus.OK
        frame = take_output(pair, H2R, now_ms=110)
        suffix = b"\x00\x00"
        pair.link.inject_ready_bytes(H2R, frame.data + suffix)
        blocked = pair.deliver_ready(H2R)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert blocked.bytes_consumed == len(frame.data)
        assert pair.link.ready_byte_count(H2R) == len(suffix)
        assert pair.rig.get_status().application_message_pending
        assert pair.rig.read_application_data() == first
        resumed = pair.deliver_zero(H2R)
        assert resumed.status is TransportStatus.OK and resumed.bytes_consumed == 0
        assert pair.link.ready_byte_count(H2R) == len(suffix)
        assert pair.rig.read_application_data() == second
        transfer_expect_ok(pair, R2H)
        event = pair.host.read_event()
        assert event is not None and event.type is EventType.DELIVERY_CONFIRMED
        suffix_delivery = pair.deliver_ready(H2R)
        assert suffix_delivery.status is TransportStatus.OK
        assert suffix_delivery.bytes_consumed == len(suffix)


def test_full_event_queue_defers_exact_ack_until_caller_frees_event_slot() -> None:
    with make_pair(host_seed=0xD2340000, host_sequence=50, rig_sequence=900) as pair:
        establish(pair)
        saturate_event_queue_with_protocol_errors(pair, pair.host)
        payload = b"123"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        ack = pair.accept_output(R2H)
        assert ack.handle is not None
        assert pair.link.queue_accepted_for_delivery(ack.handle)
        blocked = pair.deliver_ready(R2H)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert blocked.bytes_consumed == blocked.bytes_offered
        assert pair.host.get_status().reliable_delivery_pending
        freed = pair.host.read_event()
        assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert not pair.host.get_status().reliable_delivery_pending
        remaining = drain_events(pair.host)
        assert any(e.type is EventType.PROTOCOL_ERROR for e in remaining)
        assert sum(e.type is EventType.DELIVERY_CONFIRMED for e in remaining) == 1
        assert pair.rig.read_application_data() == payload


def test_full_event_queue_defers_retry_exhaustion_until_failure_can_be_reported() -> None:
    with make_pair(
        host_seed=0xD2340000, host_sequence=50, rig_sequence=900, timeout_ms=10, max_retries=0
    ) as pair:
        establish(pair)
        saturate_event_queue_with_protocol_errors(pair, pair.host)
        payload = b"ABC"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        take_output(pair, H2R, now_ms=300)
        pair.set_host_time(310)
        assert pair.process_host() is TransportStatus.CAPACITY_EXHAUSTED
        blocked = pair.host.get_status()
        assert blocked.session_state is SessionState.ESTABLISHED
        assert blocked.reliable_delivery_pending
        assert pair.host.submit_application_data(payload) is TransportStatus.CAPACITY_EXHAUSTED
        freed = pair.host.read_event()
        assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
        pair.set_host_time(311)
        assert pair.process_host() is TransportStatus.DELIVERY_FAILED
        failed = pair.host.get_status()
        assert failed.session_state is SessionState.RECOVERING
        assert failed.last_failure is Failure.DELIVERY
        assert failed.output_pending
        events = drain_events(pair.host)
        assert any(e.type is EventType.PROTOCOL_ERROR for e in events)
        assert sum(e.type is EventType.DELIVERY_FAILED for e in events) == 1
        assert pair.rig.read_application_data() is None


def _handshake_to_final_ack_pending(pair) -> None:
    assert pair.process_host() is TransportStatus.OK
    transfer_expect_ok(pair, H2R)
    assert pair.process_rig() is TransportStatus.OK
    transfer_expect_ok(pair, R2H)
    assert pair.process_host() is TransportStatus.OK
    transfer_expect_ok(pair, H2R)
    assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
    assert pair.host.get_status().session_state is SessionState.CONNECTING


def test_final_ack_waits_transactionally_for_host_event_capacity() -> None:
    with make_pair(host_seed=0x123456789ABCDE10, host_sequence=0x1200, rig_sequence=0x3400) as pair:
        initialize_connected(pair)
        _handshake_to_final_ack_pending(pair)
        _fill_event_queue(pair, pair.host)
        ack = pair.accept_output(R2H)
        assert ack.handle is not None
        assert pair.link.queue_accepted_for_delivery(ack.handle)
        blocked = pair.deliver_ready(R2H)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        for _ in range(3):
            assert pair.deliver_zero(R2H).status is TransportStatus.CAPACITY_EXHAUSTED
        freed = pair.host.read_event()
        assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        events = drain_events(pair.host)
        assert sum(e.type is EventType.SESSION_ESTABLISHED for e in events) == 1
        assert not any(e.type is EventType.SESSION_RESET for e in events)
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert pair.host.read_event() is None


def test_rig_confirm_acceptance_waits_transactionally_for_event_capacity() -> None:
    with make_pair(host_seed=0x223456789ABCDE20, host_sequence=0x2200, rig_sequence=0x4400) as pair:
        initialize_connected(pair)
        assert pair.process_host() is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        assert pair.process_rig() is TransportStatus.OK
        transfer_expect_ok(pair, R2H)
        assert pair.process_host() is TransportStatus.OK
        confirm = pair.accept_output(H2R)
        assert confirm.handle is not None
        duplicate = pair.link.duplicate_accepted(confirm.handle)
        assert duplicate is not None
        _fill_event_queue(pair, pair.rig)
        assert pair.link.queue_accepted_for_delivery(confirm.handle)
        blocked = pair.deliver_ready(H2R)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert pair.rig.get_status().reliable_delivery_pending
        assert not pair.rig.get_status().output_pending
        for _ in range(3):
            assert pair.deliver_zero(H2R).status is TransportStatus.CAPACITY_EXHAUSTED
        freed = pair.rig.read_event()
        assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
        assert pair.deliver_zero(H2R).status is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        events = drain_events(pair.rig)
        assert sum(e.type is EventType.SESSION_ESTABLISHED for e in events) == 1
        first_ack = take_output(pair, R2H)
        assert pair.link.queue_accepted_for_delivery(duplicate)
        assert pair.deliver_ready(H2R).status is TransportStatus.OK
        assert pair.rig.peek_output() == first_ack.data
        assert pair.rig.commit_output(pair.rig_now_ms) is TransportStatus.OK
        assert pair.host.receive_bytes(first_ack.data).status is TransportStatus.OK
        drain_events(pair.host)
        complete_application(pair, H2R, b"\x31")


def test_final_ack_waits_for_caller_pinned_confirm_retry() -> None:
    with make_pair(
        host_seed=0x323456789ABCDE30,
        host_sequence=0x3200,
        rig_sequence=0x5400,
        timeout_ms=10,
        max_retries=2,
    ) as pair:
        initialize_connected(pair)
        _handshake_to_final_ack_pending(pair)
        pair.set_host_time(10)
        assert pair.process_host() is TransportStatus.OK
        pinned = pair.host.peek_output()
        assert pinned is not None
        ack = pair.transfer_one_output(R2H)
        assert ack.delivery is not None
        assert ack.delivery.status is TransportStatus.CAPACITY_EXHAUSTED
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        assert pair.host.peek_output() == pinned
        for _ in range(3):
            assert pair.deliver_zero(R2H).status is TransportStatus.CAPACITY_EXHAUSTED
            assert pair.host.peek_output() == pinned
        assert pair.host.commit_output(11) is TransportStatus.OK
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert [e.type for e in drain_events(pair.host)] == [EventType.SESSION_ESTABLISHED]


def test_application_proof_waits_for_caller_pinned_confirm_retry() -> None:
    with make_pair(
        host_seed=0x423456789ABCDE40,
        host_sequence=0x4200,
        rig_sequence=0x6400,
        timeout_ms=10,
        max_retries=2,
    ) as pair:
        initialize_connected(pair)
        _handshake_to_final_ack_pending(pair)
        drain_events(pair.rig)
        final_ack = take_output(pair, R2H)
        pair.set_host_time(10)
        assert pair.process_host() is TransportStatus.OK
        pinned = pair.host.peek_output()
        assert pinned is not None
        payload = b"\x00\x51\x00\x52"
        assert pair.rig.submit_application_data(payload) is TransportStatus.OK
        application = pair.accept_output(R2H)
        assert application.handle is not None
        assert pair.link.queue_accepted_for_delivery(application.handle)
        blocked = pair.deliver_ready(R2H)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert not pair.host.get_status().application_message_pending
        assert pair.host.peek_output() == pinned
        assert pair.deliver_zero(R2H).status is TransportStatus.CAPACITY_EXHAUSTED
        assert pair.host.commit_output(11) is TransportStatus.OK
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.host.read_application_data() == payload
        assert [e.type for e in drain_events(pair.host)] == [EventType.SESSION_ESTABLISHED]
        transfer_expect_ok(pair, H2R)
        delivered = pair.rig.read_event()
        assert delivered is not None and delivered.type is EventType.DELIVERY_CONFIRMED
        assert pair.host.receive_bytes(final_ack.data).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.host.read_event() is None


def test_multi_frame_chunk_stops_at_retained_final_ack_and_preserves_exact_suffix() -> None:
    with make_pair(host_seed=0x523456789ABCDE50, host_sequence=0x5200, rig_sequence=0x7400) as pair:
        initialize_connected(pair)
        _handshake_to_final_ack_pending(pair)
        final_ack = take_output(pair, R2H)
        event_capacity = _fill_event_queue(pair, pair.host)
        freed = pair.host.read_event()
        assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
        malformed = b"\x05\x11\x22\x00"
        pair.link.inject_ready_bytes(R2H, malformed + final_ack.data + final_ack.data)
        blocked = pair.deliver_ready(R2H)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert blocked.bytes_consumed == len(malformed) + len(final_ack.data)
        assert pair.link.ready_byte_count(R2H) == len(final_ack.data)
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        freed = pair.host.read_event()
        assert freed is not None and freed.type is EventType.PROTOCOL_ERROR
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        suffix = pair.deliver_ready(R2H)
        assert suffix.status is TransportStatus.OK and suffix.bytes_consumed == len(final_ack.data)
        events = drain_events(pair.host)
        assert len(events) == event_capacity
        assert sum(e.type is EventType.SESSION_ESTABLISHED for e in events) == 1


def test_foreign_session_ack_remains_invalid_after_event_capacity_is_released() -> None:
    with (
        make_pair(host_seed=0x623456789ABCDE60, host_sequence=0x6200, rig_sequence=0x8400) as pair,
        make_pair(
            host_seed=0x723456789ABCDE70, host_sequence=0x7200, rig_sequence=0x9400
        ) as foreign,
    ):
        initialize_connected(pair)
        _handshake_to_final_ack_pending(pair)
        initialize_connected(foreign)
        _handshake_to_final_ack_pending(foreign)
        foreign_ack = take_output(foreign, R2H)
        _fill_event_queue(pair, pair.host)
        pair.link.inject_ready_bytes(R2H, foreign_ack.data)
        blocked = pair.deliver_ready(R2H)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        recovering = pair.host.get_status()
        assert recovering.session_state is SessionState.RECOVERING
        assert recovering.last_failure is Failure.PROTOCOL
        assert recovering.output_pending
        for _ in range(2):
            assert pair.deliver_zero(R2H).status is TransportStatus.CAPACITY_EXHAUSTED
        old_events = drain_events(pair.host)
        assert not any(e.type is EventType.SESSION_ESTABLISHED for e in old_events)
        assert pair.deliver_zero(R2H).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.RECOVERING
        recovery_events = drain_events(pair.host)
        assert [e.type for e in recovery_events] == [EventType.PROTOCOL_ERROR]
        transfer_expect_ok(pair, H2R)
        drain_events(pair.rig)
        pair.establish_clean_session()
        drain_events(pair.host)
        drain_events(pair.rig)
        complete_application(pair, H2R, b"ab")
