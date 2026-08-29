"""Python public-facade equivalents of C Transport recovery integration tests."""

from __future__ import annotations

from hil_rig_protocol import (
    EventType,
    LinkState,
    Role,
    SessionState,
    Transport,
    TransportConfig,
    TransportStatus,
)

from .parity_helpers import (
    H2R,
    R2H,
    complete_application,
    deliver_bytes,
    establish,
    initialize_connected,
    make_pair,
    take_output,
)
from .transport_pair_harness import drain_events


def _rig_reset_and_reestablish(pair, first_now_ms: int) -> None:
    assert pair.rig.reset() is TransportStatus.OK
    reset = take_output(pair, R2H, now_ms=first_now_ms)
    assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
    pair.set_both_times(first_now_ms + 1)
    pair.establish_clean_session(max_service_steps=32, max_transfers_per_step=16)
    assert pair.host.get_status().session_state is SessionState.ESTABLISHED
    assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_rig_delivery_exhaustion_resets_peer_and_reestablishes() -> None:
    with make_pair(
        host_seed=0xA123, host_sequence=10, rig_sequence=500, timeout_ms=10, max_retries=0
    ) as pair:
        establish(pair)
        assert pair.rig.submit_application_data(b"\x09\x08\x07") is TransportStatus.OK
        take_output(pair, R2H, now_ms=20)  # dropped by the simulated external link
        pair.set_rig_time(31)
        assert pair.process_rig() is TransportStatus.DELIVERY_FAILED
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert snapshot.output_pending

        reset = take_output(pair, R2H, now_ms=32)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_rig_time(33)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        pair.set_both_times(34)
        pair.establish_clean_session()
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_rig_explicit_reset_notifies_host_and_reestablishes() -> None:
    with make_pair(host_seed=0xD123, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.rig.reset() is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert snapshot.output_pending
        reset = take_output(pair, R2H, now_ms=20)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_both_times(21)
        pair.establish_clean_session()
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED


def test_unrecorded_older_session_traffic_recovers_and_then_delivers_application() -> None:
    with make_pair(host_seed=0xD223, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"123") is TransportStatus.OK
        old_application = take_output(pair, H2R, now_ms=20).data

        _rig_reset_and_reestablish(pair, 21)
        drain_events(pair.host)
        drain_events(pair.rig)
        _rig_reset_and_reestablish(pair, 40)
        drain_events(pair.host)
        drain_events(pair.rig)

        assert deliver_bytes(pair, H2R, old_application)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert snapshot.output_pending
        assert not snapshot.application_message_pending

        reset = take_output(pair, R2H, now_ms=60)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_both_times(61)
        pair.establish_clean_session()
        drain_events(pair.host)
        drain_events(pair.rig)
        complete_application(pair, H2R, b"ABCD", base_time=80)


def test_in_flight_peer_traffic_cannot_clear_pending_recovery_reset() -> None:
    with make_pair(
        host_seed=0xA123, host_sequence=10, rig_sequence=500, timeout_ms=10, max_retries=0
    ) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"\x01\x02\x03") is TransportStatus.OK
        delayed_host_application = take_output(pair, H2R, now_ms=20).data
        assert pair.rig.submit_application_data(b"\x04\x05\x06") is TransportStatus.OK
        take_output(pair, R2H, now_ms=21)
        pair.set_rig_time(31)
        assert pair.process_rig() is TransportStatus.DELIVERY_FAILED
        assert pair.rig.get_status().session_state is SessionState.RECOVERING
        assert pair.rig.get_status().output_pending

        assert deliver_bytes(pair, H2R, delayed_host_application)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert snapshot.output_pending

        reset = take_output(pair, R2H, now_ms=32)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        pair.set_rig_time(33)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.CONNECTING


def test_explicit_reset_after_delivery_failure_keeps_recovery_reset() -> None:
    with make_pair(
        host_seed=0xA1A3, host_sequence=10, rig_sequence=500, timeout_ms=10, max_retries=0
    ) as pair:
        establish(pair)
        assert pair.rig.submit_application_data(b"\x11\x22") is TransportStatus.OK
        take_output(pair, R2H, now_ms=20)
        pair.set_rig_time(30)
        assert pair.process_rig() is TransportStatus.DELIVERY_FAILED
        assert pair.rig.reset() is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert snapshot.output_pending
        reset = take_output(pair, R2H, now_ms=31)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING


def test_repeated_explicit_reset_preserves_peer_synchronization() -> None:
    with make_pair(host_seed=0xA223, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.rig.reset() is TransportStatus.OK
        assert pair.rig.reset() is TransportStatus.OK
        first = pair.rig.peek_output()
        assert first is not None
        assert pair.rig.reset() is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.RECOVERING
        assert snapshot.output_pending
        reset = take_output(pair, R2H, now_ms=20)
        assert reset.data == first
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING


def test_reset_and_replacement_initiate_can_share_one_receive_chunk() -> None:
    with make_pair(host_seed=0xB123, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.reset() is TransportStatus.OK
        reset = take_output(pair, H2R, now_ms=20).data
        pair.set_host_time(21)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=22).data
        assert deliver_bytes(pair, H2R, reset + initiate)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.output_pending
        pair.set_rig_time(23)
        assert pair.process_rig() is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.output_pending
        assert snapshot.reliable_delivery_pending


def test_application_completes_host_handshake_when_final_ack_is_lost() -> None:
    with make_pair(host_seed=0xC123, host_sequence=10, rig_sequence=500) as pair:
        initialize_connected(pair)
        pair.set_host_time(1)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=2)
        assert deliver_bytes(pair, H2R, initiate.data)[0] is TransportStatus.OK
        pair.set_rig_time(3)
        assert pair.process_rig() is TransportStatus.OK
        response = take_output(pair, R2H, now_ms=4)
        assert deliver_bytes(pair, R2H, response.data)[0] is TransportStatus.OK
        pair.set_host_time(5)
        assert pair.process_host() is TransportStatus.OK
        confirm = take_output(pair, H2R, now_ms=6)
        assert deliver_bytes(pair, H2R, confirm.data)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.CONNECTING
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        take_output(pair, R2H, now_ms=7)  # drop final ACK

        assert pair.rig.submit_application_data(b"\x01\x03\x05\x07") is TransportStatus.OK
        application = take_output(pair, R2H, now_ms=8)
        assert deliver_bytes(pair, R2H, application.data)[0] is TransportStatus.OK
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.host.read_application_data() == b"\x01\x03\x05\x07"
        ack = take_output(pair, H2R, now_ms=9)
        assert deliver_bytes(pair, H2R, ack.data)[0] is TransportStatus.OK
        assert not pair.rig.get_status().reliable_delivery_pending


def test_recovery_reset_commit_then_receive_initiate_does_not_require_process_first() -> None:
    with make_pair(host_seed=0xE123, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.rig.reset() is TransportStatus.OK
        reset = take_output(pair, R2H, now_ms=20)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_host_time(21)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=22)
        assert deliver_bytes(pair, H2R, initiate.data)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.output_pending
        pair.set_rig_time(23)
        assert pair.process_rig() is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.output_pending
        assert snapshot.reliable_delivery_pending


def test_delayed_old_application_before_replacement_initiate_does_not_abort_handshake() -> None:
    with make_pair(host_seed=0xE223, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"!\"#") is TransportStatus.OK
        old_application = take_output(pair, H2R, now_ms=20).data
        assert pair.rig.reset() is TransportStatus.OK
        reset = take_output(pair, R2H, now_ms=21)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_host_time(22)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=23).data
        assert deliver_bytes(pair, H2R, old_application + initiate)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.application_message_pending
        pair.set_rig_time(24)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().output_pending


def test_partial_old_frame_then_replacement_initiate_advances_between_frames() -> None:
    with make_pair(
        host_seed=0xE2A3, host_sequence=10, rig_sequence=500, timeout_ms=0, max_retries=0
    ) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"*+,") is TransportStatus.OK
        old_application = take_output(pair, H2R, now_ms=20).data
        assert len(old_application) > 1 and old_application[-1] == 0
        assert pair.rig.reset() is TransportStatus.OK
        partial = pair.rig.receive_bytes(old_application[:-1])
        assert partial.status is TransportStatus.OK
        assert partial.bytes_consumed == len(old_application) - 1
        reset = take_output(pair, R2H, now_ms=21)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_host_time(22)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=23).data
        assert deliver_bytes(pair, H2R, old_application[-1:] + initiate)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.application_message_pending
        assert not snapshot.output_pending
        pair.set_rig_time(24)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().output_pending
        assert pair.rig.get_status().reliable_delivery_pending


def test_discarded_old_body_then_replacement_initiate_advances_between_frames() -> None:
    with make_pair(
        host_seed=0xE2B3, host_sequence=10, rig_sequence=500, timeout_ms=0, max_retries=0
    ) as pair:
        establish(pair)
        assert pair.rig.reset() is TransportStatus.OK
        oversized_body = b"\x55" * pair.rig.config.max_encoded_frame_size
        result = pair.rig.receive_bytes(oversized_body)
        assert result.status is TransportStatus.OK
        assert result.bytes_consumed == len(oversized_body)
        reset = take_output(pair, R2H, now_ms=20)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_host_time(21)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=22).data
        assert deliver_bytes(pair, H2R, b"\x00" + initiate)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.output_pending
        pair.set_rig_time(23)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().output_pending
        assert pair.rig.get_status().reliable_delivery_pending


def test_deferred_stale_diagnostic_after_reset_commit_does_not_enter_fault() -> None:
    with make_pair(host_seed=0xE323, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"12") is TransportStatus.OK
        old_application = take_output(pair, H2R, now_ms=20).data
        assert pair.rig.reset() is TransportStatus.OK
        for _ in range(4):
            assert deliver_bytes(pair, H2R, old_application)[0] is TransportStatus.OK
        result = pair.rig.receive_bytes(old_application)
        assert result.status is TransportStatus.CAPACITY_EXHAUSTED
        assert result.bytes_consumed == len(old_application)
        reset = take_output(pair, R2H, now_ms=21)
        assert reset.data
        event = pair.rig.read_event()
        assert event is not None and event.type is EventType.PROTOCOL_ERROR
        pair.set_rig_time(22)
        assert pair.process_rig() is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert snapshot.session_state is not SessionState.FAULT


def test_unbound_rig_rejects_irrelevant_application_without_starting_recovery() -> None:
    with make_pair(host_seed=0xE423, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.host.submit_application_data(b"AB") is TransportStatus.OK
        unrelated = take_output(pair, H2R, now_ms=20).data
        with Transport(
            Role.RIG,
            TransportConfig(session_seed=0, initial_reliable_sequence=700),
        ) as waiting_rig:
            assert waiting_rig.notify_link_state(LinkState.CONNECTED, 0) is TransportStatus.OK
            drain_events(waiting_rig)
            result = waiting_rig.receive_bytes(unrelated)
            assert result.status is TransportStatus.OK
            assert result.bytes_consumed == len(unrelated)
            snapshot = waiting_rig.get_status()
            assert snapshot.session_state is SessionState.CONNECTING
            assert not snapshot.output_pending
            assert not snapshot.application_message_pending


def test_recently_abandoned_initiate_is_rejected_until_physical_reconnect() -> None:
    with make_pair(host_seed=0xE523, host_sequence=10, rig_sequence=500) as pair:
        initialize_connected(pair)
        pair.set_host_time(1)
        assert pair.process_host() is TransportStatus.OK
        old_initiate = take_output(pair, H2R, now_ms=2).data
        assert deliver_bytes(pair, H2R, old_initiate)[0] is TransportStatus.OK
        pair.set_rig_time(3)
        assert pair.process_rig() is TransportStatus.OK
        response = take_output(pair, R2H, now_ms=4)
        assert deliver_bytes(pair, R2H, response.data)[0] is TransportStatus.OK
        pair.set_host_time(5)
        assert pair.process_host() is TransportStatus.OK
        confirm = take_output(pair, H2R, now_ms=6)
        assert deliver_bytes(pair, H2R, confirm.data)[0] is TransportStatus.OK
        final_ack = take_output(pair, R2H, now_ms=7)
        assert deliver_bytes(pair, R2H, final_ack.data)[0] is TransportStatus.OK
        drain_events(pair.host)
        drain_events(pair.rig)

        assert pair.rig.reset() is TransportStatus.OK
        take_output(pair, R2H, now_ms=20)  # RESET is committed but intentionally not delivered.
        assert deliver_bytes(pair, H2R, old_initiate)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.output_pending

        assert pair.rig.notify_link_state(LinkState.DISCONNECTED, 21) is TransportStatus.OK
        drain_events(pair.rig)
        assert pair.rig.notify_link_state(LinkState.CONNECTED, 22) is TransportStatus.OK
        drain_events(pair.rig)
        assert deliver_bytes(pair, H2R, old_initiate)[0] is TransportStatus.OK
        pair.set_rig_time(23)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().session_state is SessionState.CONNECTING
        assert pair.rig.get_status().output_pending


def test_delayed_old_ack_before_replacement_initiate_does_not_abort_handshake() -> None:
    with make_pair(host_seed=0xE623, host_sequence=10, rig_sequence=500) as pair:
        establish(pair)
        assert pair.rig.submit_application_data(b"ab") is TransportStatus.OK
        old_application = take_output(pair, R2H, now_ms=20)
        assert deliver_bytes(pair, R2H, old_application.data)[0] is TransportStatus.OK
        old_ack = take_output(pair, H2R, now_ms=21).data
        assert pair.rig.reset() is TransportStatus.OK
        reset = take_output(pair, R2H, now_ms=22)
        assert deliver_bytes(pair, R2H, reset.data)[0] is TransportStatus.OK
        pair.set_host_time(23)
        assert pair.process_host() is TransportStatus.OK
        initiate = take_output(pair, H2R, now_ms=24).data
        assert deliver_bytes(pair, H2R, old_ack + initiate)[0] is TransportStatus.OK
        snapshot = pair.rig.get_status()
        assert snapshot.session_state is SessionState.CONNECTING
        assert not snapshot.application_message_pending
        pair.set_rig_time(25)
        assert pair.process_rig() is TransportStatus.OK
        assert pair.rig.get_status().output_pending
