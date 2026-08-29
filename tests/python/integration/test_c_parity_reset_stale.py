"""Python public-facade equivalents of C reset and stale-traffic integration tests."""

from __future__ import annotations

from hil_rig_protocol import EventType, Failure, LinkState, SessionState, TransportStatus

from .parity_helpers import (
    H2R,
    R2H,
    complete_application,
    deliver_bytes,
    establish,
    make_pair,
    take_output,
)
from .transport_pair_harness import drain_events


def _deliver_rig_reset_and_establish_replacement(pair, first_now_ms: int) -> None:
    reset = take_output(pair, R2H, now_ms=first_now_ms)
    assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
    pair.set_rig_time(first_now_ms + 1)
    assert pair.process_rig() is TransportStatus.OK
    pair.set_host_time(first_now_ms + 2)
    assert pair.process_host() is TransportStatus.OK
    pair.set_both_times(first_now_ms + 3)
    pair.establish_clean_session()
    assert pair.host.get_status().session_state is SessionState.ESTABLISHED
    assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_explicit_reset_clears_crossed_session_work_and_events_before_replacement() -> None:
    with make_pair(host_seed=0xA123, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"host") is TransportStatus.OK
        assert pair.rig.submit_application_data(b"rig") is TransportStatus.OK
        host_item = take_output(pair, H2R, now_ms=20)
        rig_item = take_output(pair, R2H, now_ms=21)
        assert deliver_bytes(pair, H2R, host_item.data)[0] is TransportStatus.OK
        assert deliver_bytes(pair, R2H, rig_item.data)[0] is TransportStatus.OK
        before = pair.host.get_status()
        assert before.reliable_delivery_pending
        assert before.application_message_pending
        assert before.output_pending

        assert pair.host.reset() is TransportStatus.OK
        after = pair.host.get_status()
        assert after.session_state is SessionState.RECOVERING
        assert not after.reliable_delivery_pending
        assert not after.application_message_pending
        assert not after.event_pending
        assert after.output_pending
        assert after.last_failure is Failure.LOCAL_RESET
        assert pair.host.read_application_data() is None
        assert pair.host.read_event() is None

        pair.link.clear()
        reset = take_output(pair, H2R, now_ms=30)
        assert deliver_bytes(pair, H2R, reset.data)[0] is TransportStatus.OK
        pair.set_both_times(31)
        pair.establish_clean_session()
        drain_events(pair.host)
        drain_events(pair.rig)
        complete_application(pair, H2R, b"fresh", base_time=50)


def test_explicit_reset_discards_partial_parser_state_before_fresh_session() -> None:
    with make_pair(host_seed=0xB123, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"old") is TransportStatus.OK
        item = take_output(pair, H2R, now_ms=20)
        split = max(1, len(item.data) // 2)
        status, offered, consumed = deliver_bytes(pair, H2R, item.data[:split])
        assert status is TransportStatus.OK
        assert consumed == offered == split

        assert pair.rig.reset() is TransportStatus.OK
        status = pair.rig.get_status()
        assert not status.application_message_pending
        assert not status.event_pending
        assert status.output_pending

        pair.link.clear()
        reset = take_output(pair, R2H, now_ms=21)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_both_times(22)
        pair.establish_clean_session()
        drain_events(pair.host)
        drain_events(pair.rig)
        complete_application(pair, H2R, b"replacement", base_time=40)


def test_dropped_best_effort_reset_recovers_through_physical_reconnect() -> None:
    with make_pair(host_seed=0xC123, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.rig.reset() is TransportStatus.OK
        dropped = take_output(pair, R2H, now_ms=20)
        assert dropped.data

        pair.set_rig_time(21)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().session_state is not SessionState.ESTABLISHED
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED

        for endpoint in (pair.host, pair.rig):
            assert endpoint.notify_link_state(LinkState.DISCONNECTED, 30) is TransportStatus.OK
            drain_events(endpoint)
            assert endpoint.notify_link_state(LinkState.CONNECTED, 31) is TransportStatus.OK
            drain_events(endpoint)
        pair.set_both_times(32)
        pair.establish_clean_session()
        drain_events(pair.host)
        drain_events(pair.rig)
        complete_application(pair, H2R, b"after reconnect", base_time=50)


def test_unrelated_established_session_identity_forces_recovery_without_exposure() -> None:
    with (
        make_pair(host_seed=0xE1000001, host_sequence=100, rig_sequence=100) as target,
        make_pair(host_seed=0xE2000002, host_sequence=100, rig_sequence=100) as foreign,
    ):
        establish(target)
        establish(foreign)
        assert foreign.host.submit_application_data(b"foreign") is TransportStatus.OK
        stale = take_output(foreign, H2R, now_ms=20)

        assert deliver_bytes(target, H2R, stale.data)[0] is TransportStatus.OK
        snapshot = target.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert not snapshot.application_message_pending
        assert snapshot.output_pending
        assert target.rig.read_application_data() is None
        first = target.rig.read_event()
        second = target.rig.read_event()
        assert first is not None and first.type is EventType.PROTOCOL_ERROR
        assert second is not None and second.type is EventType.SESSION_RESET

        reset = take_output(target, R2H, now_ms=21)
        assert deliver_bytes(target, R2H, reset.data)[0] is TransportStatus.OK
        assert target.host.get_status().session_state is not SessionState.ESTABLISHED
        target.set_both_times(22)
        target.establish_clean_session()
        drain_events(target.host)
        drain_events(target.rig)
        complete_application(target, H2R, b"fresh", base_time=40)


def test_older_current_session_application_after_newer_delivery_forces_recovery() -> None:
    with make_pair(host_seed=0xE3000003, host_sequence=200, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"old") is TransportStatus.OK
        old = take_output(pair, H2R, now_ms=20)
        assert deliver_bytes(pair, H2R, old.data)[0] is TransportStatus.OK
        assert pair.rig.read_application_data() == b"old"
        ack = take_output(pair, R2H, now_ms=21)
        assert deliver_bytes(pair, R2H, ack.data)[0] is TransportStatus.OK
        confirmed = pair.host.read_event()
        assert confirmed is not None and confirmed.type is EventType.DELIVERY_CONFIRMED

        complete_application(pair, H2R, b"newer", base_time=30)
        assert deliver_bytes(pair, H2R, old.data)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert not snapshot.application_message_pending
        events = drain_events(pair.rig)
        assert [event.type for event in events[:2]] == [
            EventType.PROTOCOL_ERROR,
            EventType.SESSION_RESET,
        ]


def test_normal_application_delivery_crosses_uint16_sequence_wrap() -> None:
    with make_pair(host_seed=0xE4000004, host_sequence=0xFFFE, rig_sequence=500) as pair:
        establish(pair)
        for index in range(3):
            complete_application(pair, H2R, bytes([0x70 + index]), base_time=20 + index * 10)
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        assert pair.host.read_event() is None
        assert pair.rig.read_application_data() is None
