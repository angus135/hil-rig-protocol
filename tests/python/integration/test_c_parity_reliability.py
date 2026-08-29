"""Reliability/timing Python parity for caller-visible C integration tests."""

from __future__ import annotations

import pytest
from hil_rig_protocol import EventType, Failure, SessionState, TransportStatus

from .parity_helpers import (
    H2R,
    R2H,
    deliver_bytes,
    endpoint_for_direction,
    establish,
    initialize_connected,
    make_pair,
    opposite,
    take_output,
    transfer_expect_ok,
)
from .transport_pair_harness import drain_events


def _run_lost_application_frame(direction) -> None:
    with make_pair(
        host_seed=0x92340000, host_sequence=20, rig_sequence=600, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        sender, receiver = endpoint_for_direction(pair, direction)
        payload = b"\x11\x22\x33\x44"
        assert sender.submit_application_data(payload) is TransportStatus.OK
        initial = take_output(pair, direction, now_ms=100)

        if direction is H2R:
            pair.set_host_time(109)
            assert pair.process_host() is TransportStatus.OK
            assert not pair.host.get_status().output_pending
            pair.set_host_time(110)
            assert pair.process_host() is TransportStatus.OK
        else:
            pair.set_rig_time(109)
            assert pair.process_rig() is TransportStatus.OK
            assert not pair.rig.get_status().output_pending
            pair.set_rig_time(110)
            assert pair.process_rig() is TransportStatus.OK

        retry = take_output(pair, direction, now_ms=110)
        assert retry.data == initial.data
        status, offered, consumed = deliver_bytes(pair, direction, retry.data)
        assert status is TransportStatus.OK and consumed == offered
        transfer_expect_ok(pair, opposite(direction))
        assert receiver.read_application_data() == payload
        assert receiver.read_application_data() is None
        assert [event.type for event in drain_events(sender)] == [EventType.DELIVERY_CONFIRMED]
        assert sender.get_status().session_state is SessionState.ESTABLISHED
        assert receiver.get_status().session_state is SessionState.ESTABLISHED


@pytest.mark.parametrize(
    "direction",
    [H2R, R2H],
    ids=[
        "LostHostApplicationFrameRetransmitsIdenticalBytesAndDeliversOnce",
        "LostRigApplicationFrameRetransmitsIdenticalBytesAndDeliversOnce",
    ],
)
def test_lost_application_frame_retransmits_identical_bytes_and_delivers_once(direction) -> None:
    _run_lost_application_frame(direction)


def _run_lost_ack(direction) -> None:
    with make_pair(
        host_seed=0x92340000, host_sequence=20, rig_sequence=600, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        sender, receiver = endpoint_for_direction(pair, direction)
        payload = b"\x51\x52\x53"
        assert sender.submit_application_data(payload) is TransportStatus.OK
        initial_peek = sender.peek_output()
        assert initial_peek is not None
        if direction is H2R:
            pair.set_host_time(200)
        else:
            pair.set_rig_time(200)
        transfer_expect_ok(pair, direction)
        assert receiver.read_application_data() == payload
        lost_ack = take_output(pair, opposite(direction), now_ms=201)
        assert lost_ack.data

        if direction is H2R:
            pair.set_host_time(210)
            assert pair.process_host() is TransportStatus.OK
        else:
            pair.set_rig_time(210)
            assert pair.process_rig() is TransportStatus.OK
        retry = take_output(pair, direction, now_ms=210)
        assert retry.data == initial_peek
        assert deliver_bytes(pair, direction, retry.data)[0] is TransportStatus.OK
        assert receiver.read_application_data() is None
        transfer_expect_ok(pair, opposite(direction))
        assert [event.type for event in drain_events(sender)] == [EventType.DELIVERY_CONFIRMED]
        assert not sender.get_status().reliable_delivery_pending


@pytest.mark.parametrize(
    "direction",
    [H2R, R2H],
    ids=[
        "LostHostApplicationAckReacksDuplicateWithoutRedelivery",
        "LostRigApplicationAckReacksDuplicateWithoutRedelivery",
    ],
)
def test_lost_application_ack_reacks_duplicate_without_redelivery(direction) -> None:
    _run_lost_ack(direction)


def test_application_retry_exhaustion_reports_failure_and_starts_recovery() -> None:
    with make_pair(
        host_seed=0x92340000, host_sequence=20, rig_sequence=600, timeout_ms=10, max_retries=1
    ) as pair:
        establish(pair)
        payload = b"abc"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        take_output(pair, H2R, now_ms=300)  # dropped initial
        pair.set_host_time(310)
        assert pair.process_host() is TransportStatus.OK
        take_output(pair, H2R, now_ms=310)  # dropped only retry
        pair.set_host_time(320)
        assert pair.process_host() is TransportStatus.DELIVERY_FAILED
        status = pair.host.get_status()
        assert status.session_state is SessionState.RECOVERING
        assert status.last_failure is Failure.DELIVERY
        assert not status.reliable_delivery_pending
        assert status.output_pending
        assert pair.rig.read_application_data() is None
        assert pair.host.submit_application_data(payload) is TransportStatus.NOT_READY
        events = drain_events(pair.host)
        assert [event.type for event in events[:2]] == [
            EventType.DELIVERY_FAILED,
            EventType.SESSION_RESET,
        ]
        assert events[0].status is TransportStatus.DELIVERY_FAILED
        assert events[0].failure is Failure.DELIVERY
        assert events[1].failure is Failure.DELIVERY


def _continue_handshake_after_initiate(pair) -> None:
    assert pair.process_rig() is TransportStatus.OK
    transfer_expect_ok(pair, R2H)
    assert pair.process_host() is TransportStatus.OK
    transfer_expect_ok(pair, H2R)
    transfer_expect_ok(pair, R2H)
    assert pair.host.get_status().session_state is SessionState.ESTABLISHED
    assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_lost_initiate_is_retried_and_handshake_completes() -> None:
    with make_pair(
        host_seed=0xA100, host_sequence=10, rig_sequence=500, timeout_ms=10, max_retries=1
    ) as pair:
        initialize_connected(pair)
        assert pair.process_host() is TransportStatus.OK
        initial = take_output(pair, H2R, now_ms=100)
        pair.set_host_time(110)
        assert pair.process_host() is TransportStatus.OK
        retry = take_output(pair, H2R, now_ms=110)
        assert retry.data == initial.data
        assert deliver_bytes(pair, H2R, retry.data)[0] is TransportStatus.OK
        _continue_handshake_after_initiate(pair)


def test_lost_response_is_retried_and_handshake_completes() -> None:
    with make_pair(
        host_seed=0xA200, host_sequence=10, rig_sequence=500, timeout_ms=10, max_retries=1
    ) as pair:
        initialize_connected(pair)
        assert pair.process_host() is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        assert pair.process_rig() is TransportStatus.OK
        initial = take_output(pair, R2H, now_ms=100)
        pair.set_rig_time(110)
        assert pair.process_rig() is TransportStatus.OK
        retry = take_output(pair, R2H, now_ms=110)
        assert retry.data == initial.data
        assert deliver_bytes(pair, R2H, retry.data)[0] is TransportStatus.OK
        assert pair.process_host() is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        transfer_expect_ok(pair, R2H)
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_lost_confirm_is_retried_and_handshake_completes() -> None:
    with make_pair(
        host_seed=0xA300, host_sequence=10, rig_sequence=500, timeout_ms=10, max_retries=1
    ) as pair:
        initialize_connected(pair)
        assert pair.process_host() is TransportStatus.OK
        transfer_expect_ok(pair, H2R)
        assert pair.process_rig() is TransportStatus.OK
        transfer_expect_ok(pair, R2H)
        assert pair.process_host() is TransportStatus.OK
        initial = take_output(pair, H2R, now_ms=100)
        pair.set_host_time(110)
        assert pair.process_host() is TransportStatus.OK
        retry = take_output(pair, H2R, now_ms=110)
        assert retry.data == initial.data
        assert deliver_bytes(pair, H2R, retry.data)[0] is TransportStatus.OK
        transfer_expect_ok(pair, R2H)
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_peek_does_not_start_timer_and_commit_starts_it() -> None:
    with make_pair(
        host_seed=0xB2340000, host_sequence=30, rig_sequence=700, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"\x10\x20\x30") is TransportStatus.OK
        initial = pair.host.peek_output()
        assert initial is not None
        pair.set_host_time(500)
        assert pair.process_host() is TransportStatus.OK
        assert pair.host.peek_output() == initial
        assert pair.host.commit_output(500) is TransportStatus.OK
        pair.set_host_time(509)
        assert pair.process_host() is TransportStatus.OK
        assert not pair.host.get_status().output_pending
        pair.set_host_time(510)
        assert pair.process_host() is TransportStatus.OK
        assert pair.host.get_status().output_pending
        assert pair.host.peek_output() == initial


def test_ack_arriving_while_retry_ready_cancels_unpinned_retry() -> None:
    with make_pair(
        host_seed=0xB2340000, host_sequence=30, rig_sequence=700, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        payload = b"AB"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        pair.set_host_time(100)
        transfer_expect_ok(pair, H2R)
        pair.set_host_time(110)
        assert pair.process_host() is TransportStatus.OK
        assert pair.host.get_status().output_pending
        transfer_expect_ok(pair, R2H)
        status = pair.host.get_status()
        assert not status.output_pending
        assert not status.reliable_delivery_pending
        assert status.session_state is SessionState.ESTABLISHED
        assert pair.rig.read_application_data() == payload
        assert [e.type for e in drain_events(pair.host)] == [EventType.DELIVERY_CONFIRMED]


def test_ack_for_peeked_retry_is_retained_until_caller_commits_pinned_bytes() -> None:
    with make_pair(
        host_seed=0xB2340000, host_sequence=30, rig_sequence=700, timeout_ms=10, max_retries=2
    ) as pair:
        establish(pair)
        payload = b"QRS"
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        pair.set_host_time(100)
        transfer_expect_ok(pair, H2R)
        pair.set_host_time(110)
        assert pair.process_host() is TransportStatus.OK
        retry = pair.host.peek_output()
        assert retry is not None
        ack = pair.transfer_one_output(R2H)
        assert ack.delivery is not None
        assert ack.delivery.status is TransportStatus.CAPACITY_EXHAUSTED
        assert ack.delivery.bytes_consumed == ack.delivery.bytes_offered
        blocked = pair.host.get_status()
        assert blocked.reliable_delivery_pending and blocked.output_pending
        assert pair.host.commit_output(111) is TransportStatus.OK
        resumed = pair.deliver_zero(R2H)
        assert resumed.status is TransportStatus.OK
        completed = pair.host.get_status()
        assert not completed.reliable_delivery_pending and not completed.output_pending
        assert [e.type for e in drain_events(pair.host)] == [EventType.DELIVERY_CONFIRMED]


def test_retransmission_timeout_works_across_uint32_wrap_with_independent_peer_clock() -> None:
    uint32_max = (1 << 32) - 1
    with make_pair(
        host_seed=0xB2340000,
        host_sequence=30,
        rig_sequence=700,
        timeout_ms=10,
        max_retries=2,
        host_now_ms=uint32_max - 100,
        rig_now_ms=123456,
    ) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"ab") is TransportStatus.OK
        pair.set_host_time(uint32_max - 5)
        transfer_expect_ok(pair, H2R)
        pair.set_host_time(3)
        assert pair.process_host() is TransportStatus.OK
        assert not pair.host.get_status().output_pending
        pair.set_host_time(4)
        assert pair.process_host() is TransportStatus.OK
        assert pair.host.get_status().output_pending
        transfer_expect_ok(pair, R2H)
        assert not pair.host.get_status().reliable_delivery_pending
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED


def test_zero_retransmission_timeout_disables_timer_driven_retry_and_failure() -> None:
    uint32_max = (1 << 32) - 1
    with make_pair(
        host_seed=0xB2340000, host_sequence=30, rig_sequence=700, timeout_ms=0, max_retries=2
    ) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"qrs") is TransportStatus.OK
        output = pair.host.peek_output()
        assert output is not None
        assert pair.host.commit_output(100) is TransportStatus.OK
        pair.set_host_time(uint32_max)
        assert pair.process_host() is TransportStatus.OK
        status = pair.host.get_status()
        assert status.session_state is SessionState.ESTABLISHED
        assert status.reliable_delivery_pending
        assert not status.output_pending
        assert pair.host.read_event() is None
