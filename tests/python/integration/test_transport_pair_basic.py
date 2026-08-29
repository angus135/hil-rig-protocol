"""Basic real-native HOST/RIG integration through the public Python facade."""

from __future__ import annotations

import pytest
from hil_rig_protocol import (
    EventType,
    Failure,
    SessionState,
    TransportStatus,
)

from .transport_pair_harness import (
    TransportPairHarness,
    TransportPairProcessResult,
    TransportTestDirection,
    TransportTestHarnessError,
    drain_events,
)


def test_clean_pair_establishes_and_reports_public_session_events(
    transport_pair: TransportPairHarness,
) -> None:
    host_link, rig_link = transport_pair.connect()
    assert host_link is TransportStatus.OK
    assert rig_link is TransportStatus.OK

    establishment = transport_pair.establish_clean_session()
    assert establishment.service_steps >= 1
    assert establishment.transfers > 0

    host_snapshot = transport_pair.host.get_status()
    rig_snapshot = transport_pair.rig.get_status()
    assert host_snapshot.session_state is SessionState.ESTABLISHED
    assert rig_snapshot.session_state is SessionState.ESTABLISHED

    host_events = drain_events(transport_pair.host)
    rig_events = drain_events(transport_pair.rig)
    assert [event.type for event in host_events] == [
        EventType.LINK_STATE_CHANGED,
        EventType.SESSION_ESTABLISHED,
    ]
    assert [event.type for event in rig_events] == [
        EventType.LINK_STATE_CHANGED,
        EventType.SESSION_ESTABLISHED,
    ]
    assert all(event.status is TransportStatus.OK for event in host_events + rig_events)
    assert all(event.failure is Failure.NONE for event in host_events + rig_events)


def test_real_healthy_output_pump_allows_exact_budget_when_quiescent(
    transport_pair: TransportPairHarness,
) -> None:
    assert transport_pair.connect() == (TransportStatus.OK, TransportStatus.OK)
    process = transport_pair.process_both()
    assert process.host_status is TransportStatus.OK
    assert process.rig_status is TransportStatus.OK

    assert transport_pair._pump_healthy_outputs(1) == 1
    assert not transport_pair.host.get_status().output_pending
    assert not transport_pair.rig.get_status().output_pending


def test_session_step_limit_error_includes_public_pair_diagnostics(
    transport_pair: TransportPairHarness,
) -> None:
    transport_pair.process_both = lambda: TransportPairProcessResult(  # type: ignore[method-assign]
        TransportStatus.OK,
        TransportStatus.OK,
    )
    transport_pair._pump_healthy_outputs = lambda _limit: 0  # type: ignore[method-assign]

    with pytest.raises(TransportTestHarnessError) as exc_info:
        transport_pair.establish_clean_session(max_service_steps=1)

    message = str(exc_info.value)
    assert "within 1 service steps" in message
    assert "host_now_ms=" in message
    assert "rig_now_ms=" in message
    assert "host_snapshot=" in message
    assert "rig_snapshot=" in message
    assert "HOST_TO_RIG[" in message
    assert "RIG_TO_HOST[" in message
    assert "pending_output_count=" in message
    assert "accepted_item_count=" in message
    assert "ready_byte_count=" in message


def test_real_native_output_remains_pinned_across_partial_external_writes(
    transport_pair: TransportPairHarness,
) -> None:
    assert transport_pair.connect() == (TransportStatus.OK, TransportStatus.OK)
    process = transport_pair.process_both()
    assert process.host_status is TransportStatus.OK
    assert process.rig_status is TransportStatus.OK

    direction = TransportTestDirection.HOST_TO_RIG
    original = transport_pair.host.peek_output()
    assert original is not None
    assert len(original) > 1

    first = transport_pair.accept_output(direction, max_bytes=1)
    assert first.status is None
    assert first.handle is None
    assert first.bytes_accepted == 1
    assert first.accepted_offset == 1
    assert not first.committed
    assert transport_pair.host.get_status().output_pending
    assert transport_pair.host.peek_output() == original
    assert transport_pair.link.pending_output_count(direction) == 1
    assert transport_pair.link.pending_output_remaining(direction) == len(original) - 1

    final = transport_pair.accept_output(direction)
    assert final.status is TransportStatus.OK
    assert final.handle is not None
    assert final.bytes_accepted == len(original) - 1
    assert final.accepted_offset == len(original)
    assert final.committed
    assert not transport_pair.host.get_status().output_pending
    assert transport_pair.link.pending_output_count(direction) == 0
    assert transport_pair.link.accepted_item_count(direction) == 1


@pytest.mark.parametrize(
    ("direction", "payload"),
    [
        (TransportTestDirection.HOST_TO_RIG, b"host-to-rig"),
        (TransportTestDirection.RIG_TO_HOST, b"rig-to-host\x00\xff"),
    ],
)
def test_established_pair_delivers_opaque_application_and_confirmation(
    established_pair: TransportPairHarness,
    direction: TransportTestDirection,
    payload: bytes,
) -> None:
    if direction is TransportTestDirection.HOST_TO_RIG:
        sender = established_pair.host
        receiver = established_pair.rig
        ack_direction = TransportTestDirection.RIG_TO_HOST
    else:
        sender = established_pair.rig
        receiver = established_pair.host
        ack_direction = TransportTestDirection.HOST_TO_RIG

    assert sender.submit_application_data(payload) is TransportStatus.OK
    assert sender.get_status().reliable_delivery_pending

    data_transfer = established_pair.transfer_one_output(direction)
    assert data_transfer.accept.status is TransportStatus.OK
    assert data_transfer.delivery is not None
    assert data_transfer.delivery.status is TransportStatus.OK

    assert receiver.read_application_data() == payload
    assert receiver.read_application_data() is None

    ack_transfer = established_pair.transfer_one_output(ack_direction)
    assert ack_transfer.accept.status is TransportStatus.OK
    assert ack_transfer.delivery is not None
    assert ack_transfer.delivery.status is TransportStatus.OK

    confirmation = sender.read_event()
    assert confirmation is not None
    assert confirmation.type is EventType.DELIVERY_CONFIRMED
    assert confirmation.status is TransportStatus.OK
    assert confirmation.failure is Failure.NONE
    assert sender.read_event() is None

    assert not sender.get_status().reliable_delivery_pending
    assert receiver.get_status().session_state is SessionState.ESTABLISHED


def test_real_native_application_delivery_can_arrive_one_byte_at_a_time(
    established_pair: TransportPairHarness,
) -> None:
    direction = TransportTestDirection.HOST_TO_RIG
    payload = b"chunked-native-application\x00\xff"

    assert established_pair.host.submit_application_data(payload) is TransportStatus.OK
    accepted = established_pair.accept_output(direction)
    assert accepted.status is TransportStatus.OK
    assert accepted.handle is not None
    assert accepted.committed
    assert established_pair.link.queue_accepted_for_delivery(accepted.handle)

    delivery_calls = 0
    while established_pair.link.ready_byte_count(direction) > 0:
        delivery = established_pair.deliver_ready(direction, max_bytes=1)
        assert delivery.status is TransportStatus.OK
        assert delivery.bytes_offered == 1
        assert delivery.bytes_consumed == 1
        delivery_calls += 1

    assert delivery_calls > 1
    assert established_pair.rig.read_application_data() == payload
    assert established_pair.rig.read_application_data() is None

    ack = established_pair.transfer_one_output(TransportTestDirection.RIG_TO_HOST)
    assert ack.accept.status is TransportStatus.OK
    assert ack.delivery is not None
    assert ack.delivery.status is TransportStatus.OK

    confirmation = established_pair.host.read_event()
    assert confirmation is not None
    assert confirmation.type is EventType.DELIVERY_CONFIRMED
    assert confirmation.status is TransportStatus.OK
    assert confirmation.failure is Failure.NONE
