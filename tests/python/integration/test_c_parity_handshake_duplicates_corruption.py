"""Handshake duplicate and corruption parity with the public C integration suite."""

from __future__ import annotations

from hil_rig_protocol import EventType, Failure, SessionState, TransportStatus

from .parity_helpers import (
    H2R,
    R2H,
    complete_application,
    deliver_bytes,
    handshake_through_final_ack,
    initialize_connected,
    make_pair,
    take_output,
)
from .transport_pair_harness import drain_events


def _corrupt_integrity(data: bytes) -> bytes:
    assert len(data) >= 3 and data[-1] == 0
    corrupted = bytearray(data)
    offset = len(corrupted) - 2
    corrupted[offset] ^= 2 if corrupted[offset] == 1 else 1
    return bytes(corrupted)


def _expect_protocol_error_only(transport) -> None:
    events = drain_events(transport)
    assert len(events) == 1
    event = events[0]
    assert event.type is EventType.PROTOCOL_ERROR
    assert event.status is TransportStatus.NOT_READY
    assert event.failure is Failure.PROTOCOL
    assert event.required_capacity == 0


def _expect_exactly_one_establishment(transport) -> None:
    events = drain_events(transport)
    assert sum(e.type is EventType.SESSION_ESTABLISHED for e in events) == 1
    assert not any(e.type in (EventType.PROTOCOL_ERROR, EventType.SESSION_RESET) for e in events)


def _publish_initiate(pair, t=100) -> bytes:
    assert pair.process_host() is TransportStatus.OK
    return take_output(pair, H2R, now_ms=t).data


def _publish_response(pair, initiate: bytes, t=101) -> bytes:
    assert deliver_bytes(pair, H2R, initiate)[0] is TransportStatus.OK
    assert pair.process_rig() is TransportStatus.OK
    return take_output(pair, R2H, now_ms=t).data


def _publish_confirm(pair, response: bytes, t=102) -> bytes:
    assert deliver_bytes(pair, R2H, response)[0] is TransportStatus.OK
    assert pair.process_host() is TransportStatus.OK
    return take_output(pair, H2R, now_ms=t).data


def _publish_final_ack(pair, confirm: bytes, t=103) -> bytes:
    assert deliver_bytes(pair, H2R, confirm)[0] is TransportStatus.OK
    return take_output(pair, R2H, now_ms=t).data


def _finish_from_initiate(pair, base=110) -> None:
    assert pair.process_rig() is TransportStatus.OK
    response = take_output(pair, R2H, now_ms=base).data
    assert deliver_bytes(pair, R2H, response)[0] is TransportStatus.OK
    assert pair.process_host() is TransportStatus.OK
    confirm = take_output(pair, H2R, now_ms=base + 1).data
    assert deliver_bytes(pair, H2R, confirm)[0] is TransportStatus.OK
    final_ack = take_output(pair, R2H, now_ms=base + 2).data
    assert deliver_bytes(pair, R2H, final_ack)[0] is TransportStatus.OK
    _expect_exactly_one_establishment(pair.host)
    _expect_exactly_one_establishment(pair.rig)


def _reset_and_prepare_replacement(pair, now_ms: int) -> None:
    assert pair.host.reset() is TransportStatus.OK
    reset = take_output(pair, H2R, now_ms=now_ms)
    assert deliver_bytes(pair, H2R, reset.data)[0] is TransportStatus.OK
    drain_events(pair.host)
    drain_events(pair.rig)
    pair.set_both_times(now_ms + 1)


def test_joined_duplicate_initiates_are_coalesced_before_response_publication() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 10)
        assert deliver_bytes(pair, H2R, initiate + initiate)[0] is TransportStatus.OK
        assert pair.process_rig() is TransportStatus.OK
        response = take_output(pair, R2H, now_ms=11).data
        assert deliver_bytes(pair, R2H, response)[0] is TransportStatus.OK
        assert pair.process_host() is TransportStatus.OK
        confirm = take_output(pair, H2R, now_ms=12).data
        assert deliver_bytes(pair, H2R, confirm)[0] is TransportStatus.OK
        ack = take_output(pair, R2H, now_ms=13).data
        assert deliver_bytes(pair, R2H, ack)[0] is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host)
        _expect_exactly_one_establishment(pair.rig)


def test_duplicate_initiate_replays_committed_response_without_restarting_attempt() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 20)
        assert deliver_bytes(pair, H2R, initiate)[0] is TransportStatus.OK
        assert pair.process_rig() is TransportStatus.OK
        lost_response = take_output(pair, R2H, now_ms=21).data
        assert not pair.rig.get_status().output_pending
        assert pair.rig.get_status().reliable_delivery_pending
        assert deliver_bytes(pair, H2R, initiate)[0] is TransportStatus.OK
        assert pair.process_rig() is TransportStatus.OK
        replay = take_output(pair, R2H, now_ms=22).data
        assert replay == lost_response
        assert deliver_bytes(pair, R2H, replay)[0] is TransportStatus.OK
        assert pair.process_host() is TransportStatus.OK
        confirm = take_output(pair, H2R, now_ms=23).data
        assert deliver_bytes(pair, H2R, confirm)[0] is TransportStatus.OK
        ack = take_output(pair, R2H, now_ms=24).data
        assert deliver_bytes(pair, R2H, ack)[0] is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host)
        _expect_exactly_one_establishment(pair.rig)


def test_duplicate_response_replays_confirm_without_restarting_attempt() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 20)
        response = _publish_response(pair, initiate, 21)
        assert deliver_bytes(pair, R2H, response)[0] is TransportStatus.OK
        assert pair.process_host() is TransportStatus.OK
        first_confirm = take_output(pair, H2R, now_ms=22).data
        assert deliver_bytes(pair, R2H, response)[0] is TransportStatus.OK
        assert pair.process_host() is TransportStatus.OK
        replay = take_output(pair, H2R, now_ms=23).data
        assert replay == first_confirm
        assert deliver_bytes(pair, H2R, replay)[0] is TransportStatus.OK
        ack = take_output(pair, R2H, now_ms=24).data
        assert deliver_bytes(pair, R2H, ack)[0] is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host)
        _expect_exactly_one_establishment(pair.rig)


def test_duplicate_confirm_reissues_final_ack_without_duplicate_establishment() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        traffic = handshake_through_final_ack(pair, base_time=30)
        assert deliver_bytes(pair, H2R, traffic.confirm)[0] is TransportStatus.OK
        replacement_ack = take_output(pair, R2H, now_ms=34).data
        assert replacement_ack == traffic.final_ack
        assert deliver_bytes(pair, R2H, replacement_ack)[0] is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host)
        _expect_exactly_one_establishment(pair.rig)


def test_duplicate_final_ack_does_not_publish_second_host_establishment() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        traffic = handshake_through_final_ack(pair, base_time=40)
        assert deliver_bytes(pair, R2H, traffic.final_ack)[0] is TransportStatus.OK
        assert deliver_bytes(pair, R2H, traffic.final_ack)[0] is TransportStatus.OK
        status = pair.host.get_status()
        assert status.session_state is SessionState.ESTABLISHED
        assert not status.output_pending and not status.reliable_delivery_pending
        _expect_exactly_one_establishment(pair.host)
        _expect_exactly_one_establishment(pair.rig)


def test_delayed_response_from_abandoned_attempt_does_not_disrupt_replacement_handshake() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        initiate_a = _publish_initiate(pair, 50)
        response_a = _publish_response(pair, initiate_a, 51)
        _reset_and_prepare_replacement(pair, 60)
        initiate_b = _publish_initiate(pair, 61)
        response_b = _publish_response(pair, initiate_b, 62)
        assert deliver_bytes(pair, R2H, response_a)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        assert deliver_bytes(pair, R2H, response_b)[0] is TransportStatus.OK
        assert pair.process_host() is TransportStatus.OK
        confirm_b = take_output(pair, H2R, now_ms=63).data
        assert deliver_bytes(pair, H2R, confirm_b)[0] is TransportStatus.OK
        ack_b = take_output(pair, R2H, now_ms=64).data
        assert deliver_bytes(pair, R2H, ack_b)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        drain_events(pair.host); drain_events(pair.rig)
        complete_application(pair, H2R, b"qrs")


def test_delayed_confirm_from_previous_session_does_not_disrupt_established_replacement() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        initiate_a = _publish_initiate(pair, 70)
        response_a = _publish_response(pair, initiate_a, 71)
        confirm_a = _publish_confirm(pair, response_a, 72)
        _reset_and_prepare_replacement(pair, 80)
        pair.establish_clean_session(); drain_events(pair.host); drain_events(pair.rig)
        assert deliver_bytes(pair, H2R, confirm_a)[0] is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        assert not pair.rig.get_status().output_pending
        assert not any(e.type is EventType.SESSION_ESTABLISHED for e in drain_events(pair.rig))
        complete_application(pair, H2R, b"qrs")


def test_delayed_final_ack_from_previous_attempt_cannot_complete_new_confirm() -> None:
    with make_pair() as pair:
        initialize_connected(pair)
        a = handshake_through_final_ack(pair, base_time=90)
        _reset_and_prepare_replacement(pair, 100)
        b = handshake_through_final_ack(pair, base_time=110)
        assert b.final_ack != a.final_ack
        assert deliver_bytes(pair, R2H, a.final_ack)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        assert deliver_bytes(pair, R2H, b.final_ack)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED


def test_incomplete_initiate_waits_for_remaining_bytes_without_diagnostic_or_advancement() -> None:
    with make_pair(host_seed=0xC102030405060708, timeout_ms=10, max_retries=2) as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 100)
        assert len(initiate) > 1
        first = pair.rig.receive_bytes(initiate[:-1])
        assert first.status is TransportStatus.OK and first.bytes_consumed == len(initiate) - 1
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert not pair.rig.get_status().output_pending
        assert pair.rig.read_event() is None
        last = pair.rig.receive_bytes(initiate[-1:])
        assert last.status is TransportStatus.OK and last.bytes_consumed == 1
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert not pair.rig.get_status().output_pending
        assert pair.rig.read_event() is None
        _finish_from_initiate(pair, 110)


def test_structurally_malformed_input_does_not_prevent_later_clean_initiate() -> None:
    with make_pair(host_seed=0xC102030405060708) as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 100)
        assert pair.rig.receive_bytes(b"\x05\x11\x22\x00").status is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        _expect_protocol_error_only(pair.rig)
        assert pair.rig.receive_bytes(initiate).status is TransportStatus.OK
        _finish_from_initiate(pair, 110)


def test_integrity_invalid_initiate_does_not_advance_rig_and_clean_copy_completes_handshake(
) -> None:
    with make_pair(host_seed=0xC102030405060708) as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 100)
        assert pair.rig.receive_bytes(_corrupt_integrity(initiate)).status is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert not pair.rig.get_status().output_pending
        assert pair.host.get_status().reliable_delivery_pending
        _expect_protocol_error_only(pair.rig)
        assert pair.rig.receive_bytes(initiate).status is TransportStatus.OK
        _finish_from_initiate(pair, 110)


def test_integrity_invalid_response_retains_initiate_until_retry_and_clean_response() -> None:
    with make_pair(host_seed=0xC102030405060708, timeout_ms=10) as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 100)
        response = _publish_response(pair, initiate, 101)
        assert pair.host.receive_bytes(_corrupt_integrity(response)).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        assert pair.host.get_status().reliable_delivery_pending
        _expect_protocol_error_only(pair.host)
        pair.set_host_time(109)
        assert pair.process_host() is TransportStatus.OK
        assert not pair.host.get_status().output_pending
        pair.set_host_time(110); assert pair.process_host() is TransportStatus.OK
        assert take_output(pair, H2R, now_ms=110).data == initiate
        confirm = _publish_confirm(pair, response, 111)
        final_ack = _publish_final_ack(pair, confirm, 112)
        assert pair.host.receive_bytes(final_ack).status is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host); _expect_exactly_one_establishment(pair.rig)


def test_integrity_invalid_confirm_does_not_establish_rig_or_publish_final_ack() -> None:
    with make_pair(host_seed=0xC102030405060708) as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 100)
        response = _publish_response(pair, initiate, 101)
        confirm = _publish_confirm(pair, response, 102)
        assert pair.rig.receive_bytes(_corrupt_integrity(confirm)).status is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert not pair.rig.get_status().output_pending
        assert pair.rig.get_status().reliable_delivery_pending
        _expect_protocol_error_only(pair.rig)
        final_ack = _publish_final_ack(pair, confirm, 103)
        assert pair.host.receive_bytes(final_ack).status is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host); _expect_exactly_one_establishment(pair.rig)


def test_integrity_invalid_final_ack_retains_confirm_until_retry_and_clean_ack() -> None:
    with make_pair(host_seed=0xC102030405060708, timeout_ms=10) as pair:
        initialize_connected(pair)
        initiate = _publish_initiate(pair, 100)
        response = _publish_response(pair, initiate, 101)
        confirm = _publish_confirm(pair, response, 102)
        final_ack = _publish_final_ack(pair, confirm, 103)
        assert pair.host.receive_bytes(_corrupt_integrity(final_ack)).status is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        assert pair.host.get_status().reliable_delivery_pending
        _expect_protocol_error_only(pair.host)
        pair.set_host_time(111)
        assert pair.process_host() is TransportStatus.OK
        assert not pair.host.get_status().output_pending
        pair.set_host_time(112); assert pair.process_host() is TransportStatus.OK
        assert take_output(pair, H2R, now_ms=112).data == confirm
        assert pair.host.receive_bytes(final_ack).status is TransportStatus.OK
        _expect_exactly_one_establishment(pair.host); _expect_exactly_one_establishment(pair.rig)
        assert not pair.host.get_status().output_pending


def test_valid_response_with_incompatible_session_fields_triggers_documented_recovery() -> None:
    with (
        make_pair(host_seed=0xC102030405060708) as pair,
        make_pair(host_seed=0xC202030405060708) as foreign,
    ):
        initialize_connected(pair); initialize_connected(foreign)
        response = _publish_response(pair, _publish_initiate(pair, 100), 101)
        foreign_response = _publish_response(foreign, _publish_initiate(foreign, 100), 101)
        assert foreign_response != response
        assert pair.host.receive_bytes(foreign_response).status is TransportStatus.OK
        recovering = pair.host.get_status()
        assert recovering.session_state is SessionState.RECOVERING
        assert recovering.last_failure is Failure.PROTOCOL
        assert not recovering.reliable_delivery_pending
        assert recovering.output_pending
        events = drain_events(pair.host)
        assert sum(e.type is EventType.PROTOCOL_ERROR for e in events) == 1
        assert sum(e.type is EventType.SESSION_RESET for e in events) == 1
        assert sum(e.type is EventType.SESSION_ESTABLISHED for e in events) == 0
        reset = take_output(pair, H2R, now_ms=110).data
        assert pair.rig.receive_bytes(reset).status is TransportStatus.OK
        assert sum(e.type is EventType.SESSION_RESET for e in drain_events(pair.rig)) == 1
        pair.set_both_times(120)
        pair.establish_clean_session()
        _expect_exactly_one_establishment(pair.host); _expect_exactly_one_establishment(pair.rig)
