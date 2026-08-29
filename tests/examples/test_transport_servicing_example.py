"""Contract and real-native tests for the caller-owned servicing example."""

from __future__ import annotations

import importlib.util
import os
import sys
from pathlib import Path

import pytest
from hil_rig_protocol import (
    EventType,
    Failure,
    LinkState,
    OperatingMode,
    ReceiveResult,
    Role,
    SessionState,
    Transport,
    TransportConfig,
    TransportEvent,
    TransportStatus,
)


def _load_example():
    root = Path(os.environ.get("HIL_RIG_PROTOCOL_PROJECT_ROOT", Path(__file__).parents[2]))
    path = root / "examples" / "python" / "transport_servicing.py"
    spec = importlib.util.spec_from_file_location("transport_servicing_example", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class FakeTransport:
    def __init__(self) -> None:
        self.receive_results = [
            ReceiveResult(TransportStatus.CAPACITY_EXHAUSTED, 2),
            ReceiveResult(TransportStatus.NOT_READY, 0),
        ]
        self.received: list[bytes] = []
        self.output = b"abcdef"
        self.commits: list[int] = []
        self.commit_status = TransportStatus.OK
        self.commit_exception: Exception | None = None
        self.reset_status = TransportStatus.OK
        self.events = [
            TransportEvent(EventType.LINK_STATE_CHANGED, TransportStatus.OK, Failure.NONE, 0)
        ]
        self.messages = [b"opaque"]

    def reset(self) -> TransportStatus:
        return self.reset_status

    def notify_link_state(self, link_state: LinkState, now_ms: int) -> TransportStatus:
        del link_state, now_ms
        return TransportStatus.OK

    def receive_bytes(self, data: bytes | bytearray) -> ReceiveResult:
        self.received.append(bytes(data))
        return self.receive_results.pop(0)

    def peek_output(self) -> bytes | None:
        return self.output

    def commit_output(self, now_ms: int) -> TransportStatus:
        self.commits.append(now_ms)
        if self.commit_exception is not None:
            raise self.commit_exception
        return self.commit_status

    def read_event(self) -> TransportEvent | None:
        return self.events.pop(0) if self.events else None

    def read_application_data(self) -> bytes | None:
        return self.messages.pop(0) if self.messages else None


def test_partial_receive_retains_exact_suffix_and_represents_zero_byte_receive() -> None:
    example = _load_example()
    native = FakeTransport()
    service = example.TransportServicer(native)

    result = service.offer_received(b"abcd")
    assert result.bytes_consumed == 2
    assert service.incoming == bytearray(b"cd")
    assert native.received == [b"abcd"]

    service.incoming.clear()
    assert service.offer_received() == ReceiveResult(TransportStatus.NOT_READY, 0)
    assert native.received[-1] == b""


def test_partial_external_write_commits_once_only_after_final_acceptance() -> None:
    example = _load_example()
    native = FakeTransport()
    service = example.TransportServicer(native)
    offered: list[bytes] = []

    def accept_two(data: bytes) -> int:
        offered.append(data)
        return min(2, len(data))

    assert service.service_output(10, accept_two).commit_status is None
    assert service.pending_output_offset == 2
    assert native.commits == []
    assert service.service_output(11, accept_two).commit_status is None
    final = service.service_output(12, accept_two)

    assert offered == [b"abcdef", b"cdef", b"ef"]
    assert final.commit_status is TransportStatus.OK
    assert native.commits == [12]
    assert service.pending_output is None


def test_completed_external_write_is_not_resent_when_commit_is_not_ready() -> None:
    example = _load_example()
    native = FakeTransport()
    native.commit_status = TransportStatus.NOT_READY
    service = example.TransportServicer(native)
    offered: list[bytes] = []

    result = service.service_output(10, lambda data: offered.append(data) or len(data))

    assert result == example.OutputServiceResult(6, TransportStatus.NOT_READY)
    assert offered == [b"abcdef"]
    assert service.pending_output is None
    assert service.pending_output_offset == 0


def test_unexpected_commit_status_raises_after_discarding_accepted_output_state() -> None:
    example = _load_example()
    native = FakeTransport()
    native.commit_status = TransportStatus.INVALID_ARGUMENT
    service = example.TransportServicer(native)

    with pytest.raises(example.TransportServicingError, match="commit_output"):
        service.service_output(10, len)

    assert service.pending_output is None
    assert service.pending_output_offset == 0


def test_commit_exception_propagates_after_discarding_accepted_output_state() -> None:
    example = _load_example()
    native = FakeTransport()
    service = example.TransportServicer(native)
    offered: list[bytes] = []
    commit_error = RuntimeError("commit failed")
    native.commit_exception = commit_error

    with pytest.raises(RuntimeError) as raised:
        service.service_output(10, lambda data: offered.append(data) or len(data))

    assert raised.value is commit_error
    assert offered == [b"abcdef"]
    assert native.commits == [10]
    assert service.pending_output is None
    assert service.pending_output_offset == 0

    native.output = None
    subsequent_offers: list[bytes] = []
    assert service.service_output(11, lambda data: subsequent_offers.append(data) or len(data)) == (
        example.OutputServiceResult(0, None)
    )
    assert subsequent_offers == []


def test_disconnect_and_reset_invalidate_all_caller_owned_byte_state() -> None:
    example = _load_example()
    native = FakeTransport()
    service = example.TransportServicer(native)
    service.incoming.extend(b"stale-input")
    service.pending_output = b"stale-output"
    service.pending_output_offset = 4

    assert service.notify_disconnected(10) is TransportStatus.OK
    assert service.incoming == bytearray()
    assert service.pending_output is None
    assert service.pending_output_offset == 0

    service.incoming.extend(b"again")
    service.pending_output = b"again-output"
    service.pending_output_offset = 2
    assert service.reset() is TransportStatus.OK
    assert service.incoming == bytearray()
    assert service.pending_output is None
    assert service.pending_output_offset == 0


def test_events_and_application_data_are_explicitly_drained() -> None:
    example = _load_example()
    native = FakeTransport()
    drained = example.TransportServicer(native).drain()
    assert len(drained.events) == 1
    assert drained.application_messages == (b"opaque",)


def _config(role: Role) -> TransportConfig:
    return TransportConfig(
        session_seed=0x123456789ABCDEF if role is Role.HOST else 0,
        initial_reliable_sequence=10 if role is Role.HOST else 500,
        retransmit_timeout_ms=10,
        max_retries=2,
    )


def _drain_events(transport: Transport) -> list[TransportEvent]:
    events: list[TransportEvent] = []
    while (event := transport.read_event()) is not None:
        events.append(event)
    return events


def _transfer_direct(sender: Transport, receiver_offer, now_ms: int) -> bytes | None:
    output = sender.peek_output()
    if output is None:
        return None
    assert sender.commit_output(now_ms) is TransportStatus.OK
    result = receiver_offer(output)
    assert result.status is TransportStatus.OK
    assert result.bytes_consumed == len(output)
    return output


def _transfer_servicer_output(service, receiver: Transport, now_ms: int) -> bytes | None:
    offered = bytearray()
    result = service.service_output(now_ms, lambda data: offered.extend(data) or len(data))
    if result.accepted == 0 and result.commit_status is None:
        return None
    assert result.commit_status is TransportStatus.OK
    received = receiver.receive_bytes(bytes(offered))
    assert received.status is TransportStatus.OK
    assert received.bytes_consumed == len(offered)
    return bytes(offered)


def _establish(service, host: Transport, rig: Transport, *, start_ms: int) -> int:
    for step in range(32):
        now_ms = start_ms + step
        assert service.process(now_ms, OperatingMode.NORMAL) in (
            TransportStatus.OK,
            TransportStatus.NOT_READY,
        )
        assert rig.process(now_ms, OperatingMode.NORMAL) in (
            TransportStatus.OK,
            TransportStatus.NOT_READY,
        )
        _transfer_servicer_output(service, rig, now_ms)
        _transfer_direct(rig, service.offer_received, now_ms)
        if (
            host.get_status().session_state is SessionState.ESTABLISHED
            and rig.get_status().session_state is SessionState.ESTABLISHED
        ):
            return now_ms + 1
    raise AssertionError("real Transport pair did not establish")


def _connect_pair(service, rig: Transport, now_ms: int) -> int:
    assert service.notify_connected(now_ms) is TransportStatus.OK
    assert rig.notify_link_state(LinkState.CONNECTED, now_ms) is TransportStatus.OK
    _drain_events(service.transport)
    _drain_events(rig)
    return _establish(service, service.transport, rig, start_ms=now_ms + 1)


def _hard_reconnect(service, rig: Transport, now_ms: int) -> int:
    # A real physical owner stops/quiesces the driver and flushes its old RX/TX
    # queues before calling notify_disconnected(). The in-memory test has no
    # separate driver queues, so invalidating the servicer is the relevant step.
    assert service.notify_disconnected(now_ms) is TransportStatus.OK
    assert rig.notify_link_state(LinkState.DISCONNECTED, now_ms) is TransportStatus.OK
    host_disconnect = _drain_events(service.transport)
    rig_disconnect = _drain_events(rig)
    assert any(event.type is EventType.LINK_STATE_CHANGED for event in host_disconnect)
    assert any(event.type is EventType.SESSION_RESET for event in host_disconnect)
    assert any(event.type is EventType.LINK_STATE_CHANGED for event in rig_disconnect)
    assert any(event.type is EventType.SESSION_RESET for event in rig_disconnect)

    assert service.notify_connected(now_ms + 1) is TransportStatus.OK
    assert rig.notify_link_state(LinkState.CONNECTED, now_ms + 1) is TransportStatus.OK
    _drain_events(service.transport)
    _drain_events(rig)
    return _establish(service, service.transport, rig, start_ms=now_ms + 2)


def test_real_partial_output_does_not_cross_disconnect_and_fresh_output_progresses() -> None:
    example = _load_example()
    host = Transport(Role.HOST, _config(Role.HOST))
    rig = Transport(Role.RIG, _config(Role.RIG))
    service = example.TransportServicer(host)
    try:
        now_ms = _connect_pair(service, rig, 0)
        _drain_events(host)
        _drain_events(rig)

        assert host.submit_application_data(b"old-connection") is TransportStatus.OK
        old_frame = host.peek_output()
        assert old_frame is not None and len(old_frame) > 4
        old_writes: list[bytes] = []
        first = service.service_output(
            now_ms,
            lambda data: old_writes.append(bytes(data[:3])) or min(3, len(data)),
        )
        assert first.commit_status is None
        assert old_writes == [old_frame[:3]]
        stale_suffix = old_frame[3:]

        now_ms = _hard_reconnect(service, rig, now_ms + 1)
        assert host.get_status().session_state is SessionState.ESTABLISHED
        assert rig.get_status().session_state is SessionState.ESTABLISHED

        assert host.submit_application_data(b"fresh-connection") is TransportStatus.OK
        post_reconnect_offers: list[bytes] = []
        result = service.service_output(
            now_ms,
            lambda data: post_reconnect_offers.append(bytes(data)) or len(data),
        )
        assert result.commit_status is TransportStatus.OK
        assert post_reconnect_offers
        assert stale_suffix not in post_reconnect_offers
        received = rig.receive_bytes(post_reconnect_offers[0])
        assert received.status is TransportStatus.OK
        assert rig.read_application_data() == b"fresh-connection"
    finally:
        service.close()
        rig.close()


def test_real_partial_incoming_frame_is_discarded_across_hard_reconnect() -> None:
    example = _load_example()
    host = Transport(Role.HOST, _config(Role.HOST))
    rig = Transport(Role.RIG, _config(Role.RIG))
    service = example.TransportServicer(host)
    try:
        now_ms = _connect_pair(service, rig, 0)
        _drain_events(host)
        _drain_events(rig)

        assert rig.submit_application_data(b"stale-prefix") is TransportStatus.OK
        old_frame = rig.peek_output()
        assert old_frame is not None and len(old_frame) > 4
        assert rig.commit_output(now_ms) is TransportStatus.OK
        prefix = old_frame[: len(old_frame) // 2]
        partial = service.offer_received(prefix)
        assert partial.status is TransportStatus.OK
        assert partial.bytes_consumed == len(prefix)
        assert host.read_application_data() is None

        now_ms = _hard_reconnect(service, rig, now_ms + 1)
        assert host.get_status().session_state is SessionState.ESTABLISHED
        assert rig.get_status().session_state is SessionState.ESTABLISHED

        assert rig.submit_application_data(b"fresh-after-prefix") is TransportStatus.OK
        fresh = _transfer_direct(rig, service.offer_received, now_ms)
        assert fresh is not None
        assert host.read_application_data() == b"fresh-after-prefix"
        events = _drain_events(host) + _drain_events(rig)
        assert not any(event.type is EventType.PROTOCOL_ERROR for event in events)
        assert host.get_status().session_state is not SessionState.FAULT
        assert rig.get_status().session_state is not SessionState.FAULT
    finally:
        service.close()
        rig.close()


def test_real_local_reset_discards_partial_output_and_recovery_stays_out_of_fault() -> None:
    example = _load_example()
    host = Transport(Role.HOST, _config(Role.HOST))
    rig = Transport(Role.RIG, _config(Role.RIG))
    service = example.TransportServicer(host)
    try:
        now_ms = _connect_pair(service, rig, 0)
        _drain_events(host)
        _drain_events(rig)

        assert host.submit_application_data(b"before-reset") is TransportStatus.OK
        old_frame = host.peek_output()
        assert old_frame is not None and len(old_frame) > 4
        service.service_output(now_ms, lambda data: min(3, len(data)))
        stale_suffix = old_frame[3:]

        assert service.reset() is TransportStatus.OK
        reset_offers: list[bytes] = []
        reset_result = service.service_output(
            now_ms + 1,
            lambda data: reset_offers.append(bytes(data)) or len(data),
        )
        assert reset_result.commit_status is TransportStatus.OK
        assert reset_offers
        assert stale_suffix not in reset_offers
        reset_receive = rig.receive_bytes(reset_offers[0])
        assert reset_receive.status is TransportStatus.OK

        _drain_events(host)
        _drain_events(rig)
        now_ms = _establish(service, host, rig, start_ms=now_ms + 2)
        assert host.get_status().session_state is SessionState.ESTABLISHED
        assert rig.get_status().session_state is SessionState.ESTABLISHED
        assert host.get_status().session_state is not SessionState.FAULT
        assert rig.get_status().session_state is not SessionState.FAULT

        assert host.submit_application_data(b"after-reset") is TransportStatus.OK
        fresh = _transfer_servicer_output(service, rig, now_ms)
        assert fresh is not None
        assert rig.read_application_data() == b"after-reset"
        events = _drain_events(host) + _drain_events(rig)
        assert not any(event.status is TransportStatus.INTERNAL_ERROR for event in events)
        assert not any(event.failure is Failure.INTERNAL for event in events)
    finally:
        service.close()
        rig.close()
