"""Python equivalents of C handshake retry-exhaustion integration scenarios."""

from __future__ import annotations

from hil_rig_protocol import EventType, Failure, SessionState, Transport, TransportStatus

from .parity_helpers import H2R, R2H, deliver_bytes, initialize_connected, make_pair, take_output
from .transport_pair_harness import TransportPairHarness, TransportTestDirection, drain_events


def _set_time(pair: TransportPairHarness, direction: TransportTestDirection, now_ms: int) -> None:
    if direction is H2R:
        pair.set_host_time(now_ms)
    else:
        pair.set_rig_time(now_ms)


def _process_sender(
    pair: TransportPairHarness, direction: TransportTestDirection
) -> TransportStatus:
    return pair.process_host() if direction is H2R else pair.process_rig()


def _sender(pair: TransportPairHarness, direction: TransportTestDirection) -> Transport:
    return pair.host if direction is H2R else pair.rig


def _exercise_all_retries_then_exhaust(
    pair: TransportPairHarness,
    direction: TransportTestDirection,
    original: bytes,
) -> None:
    endpoint = _sender(pair, direction)
    snapshot = endpoint.get_status()
    assert snapshot.session_state is SessionState.CONNECTING
    assert snapshot.reliable_delivery_pending
    assert not snapshot.output_pending

    _set_time(pair, direction, 109)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert not endpoint.get_status().output_pending

    _set_time(pair, direction, 110)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert endpoint.get_status().output_pending
    assert endpoint.peek_output() == original
    assert endpoint.peek_output() == original

    _set_time(pair, direction, 1000)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert endpoint.peek_output() == original
    retry1 = take_output(pair, direction, now_ms=110)
    assert retry1.data == original

    _set_time(pair, direction, 119)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert not endpoint.get_status().output_pending
    _set_time(pair, direction, 120)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert endpoint.peek_output() == original
    assert endpoint.peek_output() == original
    retry2 = take_output(pair, direction, now_ms=120)
    assert retry2.data == original

    _set_time(pair, direction, 129)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert not endpoint.get_status().output_pending
    _set_time(pair, direction, 130)
    assert _process_sender(pair, direction) is TransportStatus.OK

    snapshot = endpoint.get_status()
    assert snapshot.session_state is SessionState.RECOVERING
    assert snapshot.last_failure is Failure.DELIVERY
    assert not snapshot.reliable_delivery_pending
    assert snapshot.output_pending
    events = drain_events(endpoint)
    session_resets = [event for event in events if event.type is EventType.SESSION_RESET]
    assert len(session_resets) == 1
    assert session_resets[0].status is TransportStatus.DELIVERY_FAILED
    assert session_resets[0].failure is Failure.DELIVERY
    assert not any(event.type is EventType.DELIVERY_FAILED for event in events)
    assert not any(event.type is EventType.DELIVERY_CONFIRMED for event in events)


def _commit_recovery_reset(
    pair: TransportPairHarness, direction: TransportTestDirection, now_ms: int = 132
) -> bytes:
    endpoint = _sender(pair, direction)
    first = endpoint.peek_output()
    assert first is not None
    assert endpoint.peek_output() == first
    _set_time(pair, direction, now_ms - 1)
    assert _process_sender(pair, direction) is TransportStatus.OK
    assert endpoint.get_status().session_state is SessionState.RECOVERING
    assert endpoint.peek_output() == first
    reset = take_output(pair, direction, now_ms=now_ms)
    assert reset.data == first
    return reset.data


def _assert_replacement_establishes(pair: TransportPairHarness, now_ms: int = 200) -> None:
    pair.set_both_times(now_ms)
    pair.establish_clean_session(max_service_steps=32, max_transfers_per_step=16)
    assert pair.host.get_status().session_state is SessionState.ESTABLISHED
    assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_initiate_retries_preserve_bytes_and_exhaust_into_reset_before_fresh_session() -> None:
    with make_pair(host_seed=0xD102030405060708, host_sequence=100, rig_sequence=700) as pair:
        initialize_connected(pair)
        pair.set_host_time(90)
        assert pair.process_host() is TransportStatus.OK
        original = take_output(pair, H2R, now_ms=100).data
        _exercise_all_retries_then_exhaust(pair, H2R, original)
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        reset = _commit_recovery_reset(pair, H2R)
        assert reset
        _assert_replacement_establishes(pair)
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED


def test_response_retries_preserve_bytes_and_reset_failed_session_before_replacement() -> None:
    with make_pair(host_seed=0xD102030405060708, host_sequence=100, rig_sequence=700) as pair:
        initialize_connected(pair)
        pair.set_host_time(90)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=91)
        assert deliver_bytes(pair, H2R, initiate.data)[0] is TransportStatus.OK
        pair.set_rig_time(90)
        assert pair.process_rig() is TransportStatus.OK
        original = take_output(pair, R2H, now_ms=100).data
        _exercise_all_retries_then_exhaust(pair, R2H, original)
        reset = _commit_recovery_reset(pair, R2H)
        assert deliver_bytes(pair, R2H, reset)[0] is TransportStatus.OK
        peer_events = drain_events(pair.host)
        assert any(event.type is EventType.SESSION_RESET for event in peer_events)
        assert not any(event.type is EventType.DELIVERY_FAILED for event in peer_events)
        _assert_replacement_establishes(pair)


def test_confirm_retries_preserve_bytes_without_false_establishment_then_recover() -> None:
    with make_pair(host_seed=0xD102030405060708, host_sequence=100, rig_sequence=700) as pair:
        initialize_connected(pair)
        pair.set_host_time(80)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=81)
        assert deliver_bytes(pair, H2R, initiate.data)[0] is TransportStatus.OK
        pair.set_rig_time(82)
        assert pair.process_rig() is TransportStatus.OK
        response = take_output(pair, R2H, now_ms=83)
        assert deliver_bytes(pair, R2H, response.data)[0] is TransportStatus.OK
        pair.set_host_time(90)
        assert pair.process_host() is TransportStatus.OK
        original = take_output(pair, H2R, now_ms=100).data
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert not any(
            event.type is EventType.SESSION_ESTABLISHED for event in drain_events(pair.rig)
        )
        _exercise_all_retries_then_exhaust(pair, H2R, original)
        reset = _commit_recovery_reset(pair, H2R)
        assert deliver_bytes(pair, H2R, reset)[0] is TransportStatus.OK
        _assert_replacement_establishes(pair)
