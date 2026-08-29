"""Structural tests for the deterministic Python Transport test link."""

from __future__ import annotations

from dataclasses import dataclass, field
import pytest

from hil_rig_protocol import ReceiveResult, Role, TransportStatus

from .transport_pair_harness import (
    TransportLinkAcceptResult,
    TransportLinkDeliveryResult,
    TransportPairHarness,
    TransportPairTransferResult,
    TransportTestDirection,
    TransportTestHarnessError,
    TransportTestLink,
)


@dataclass
class _FakeSender:
    role: Role = Role.HOST
    output: bytes | None = b"abcdef"
    commits: list[int] = field(default_factory=list)
    peeks: int = 0

    def peek_output(self) -> bytes | None:
        self.peeks += 1
        return self.output

    def commit_output(self, now_ms: int) -> TransportStatus:
        self.commits.append(now_ms)
        self.output = None
        return TransportStatus.OK


@dataclass
class _PartialReceiver:
    role: Role = Role.RIG
    offered: list[bytes] = field(default_factory=list)

    def receive_bytes(self, data: bytes) -> ReceiveResult:
        self.offered.append(bytes(data))
        if len(self.offered) == 1:
            return ReceiveResult(TransportStatus.CAPACITY_EXHAUSTED, 2)
        return ReceiveResult(TransportStatus.OK, len(data))


@dataclass(frozen=True, slots=True)
class _PumpSnapshot:
    output_pending: bool


@dataclass
class _PumpEndpoint:
    output_pending: bool

    def get_status(self) -> _PumpSnapshot:
        return _PumpSnapshot(output_pending=self.output_pending)


def _make_fake_pump_pair(
    *, host_pending: bool, rig_pending: bool
) -> tuple[TransportPairHarness, list[TransportTestDirection]]:
    pair = object.__new__(TransportPairHarness)
    pair.host = _PumpEndpoint(host_pending)  # type: ignore[assignment]
    pair.rig = _PumpEndpoint(rig_pending)  # type: ignore[assignment]
    pair.link = TransportTestLink()
    pair.host_now_ms = 11
    pair.rig_now_ms = 22
    calls: list[TransportTestDirection] = []

    def transfer(direction: TransportTestDirection) -> TransportPairTransferResult:
        calls.append(direction)
        endpoint = (
            pair.host
            if direction is TransportTestDirection.HOST_TO_RIG
            else pair.rig
        )
        endpoint.output_pending = False  # type: ignore[attr-defined]
        return TransportPairTransferResult(
            accept=TransportLinkAcceptResult(
                status=TransportStatus.OK,
                handle=None,
                size=1,
                bytes_accepted=1,
                accepted_offset=1,
                committed=True,
            ),
            delivery=TransportLinkDeliveryResult(
                status=TransportStatus.OK,
                bytes_offered=1,
                bytes_consumed=1,
            ),
        )

    pair.transfer_one_output = transfer  # type: ignore[method-assign]
    return pair, calls


def test_link_partial_external_write_keeps_output_pinned_until_complete() -> None:
    link = TransportTestLink()
    sender = _FakeSender()
    direction = TransportTestDirection.HOST_TO_RIG

    first = link.accept_output(sender, now_ms=17, max_bytes=2)
    assert first.status is None
    assert first.handle is None
    assert first.size == 6
    assert first.bytes_accepted == 2
    assert first.accepted_offset == 2
    assert not first.committed
    assert sender.commits == []
    assert link.pending_output_count(direction) == 1
    assert link.pending_output_size(direction) == 6
    assert link.pending_output_accepted_offset(direction) == 2
    assert link.pending_output_remaining(direction) == 4

    second = link.accept_output(sender, now_ms=18, max_bytes=2)
    assert second.status is None
    assert second.handle is None
    assert second.bytes_accepted == 2
    assert second.accepted_offset == 4
    assert not second.committed
    assert sender.commits == []

    final = link.accept_output(sender, now_ms=19, max_bytes=2)
    assert final.status is TransportStatus.OK
    assert final.handle is not None
    assert final.bytes_accepted == 2
    assert final.accepted_offset == 6
    assert final.committed
    assert sender.commits == [19]
    assert sender.peeks == 3
    assert link.pending_output_count(direction) == 0
    assert link.accepted_item_count(direction) == 1


def test_link_rejects_changed_output_during_partial_external_write() -> None:
    link = TransportTestLink()
    sender = _FakeSender()

    first = link.accept_output(sender, now_ms=1, max_bytes=2)
    assert not first.committed
    sender.output = b"abcXYZ"

    with pytest.raises(TransportTestHarnessError, match="pinned output changed"):
        link.accept_output(sender, now_ms=2, max_bytes=2)

    assert sender.commits == []


def test_link_retains_exact_unconsumed_receive_suffix() -> None:
    link = TransportTestLink()
    sender = _FakeSender()
    receiver = _PartialReceiver()

    accepted = link.accept_output(sender, now_ms=17)
    assert accepted.status is TransportStatus.OK
    assert accepted.handle is not None
    assert accepted.size == 6
    assert accepted.bytes_accepted == 6
    assert accepted.accepted_offset == 6
    assert accepted.committed
    assert sender.commits == [17]
    assert link.accepted_item_count(TransportTestDirection.HOST_TO_RIG) == 1

    assert link.queue_accepted_for_delivery(accepted.handle)
    first = link.deliver_ready(receiver)
    assert first.status is TransportStatus.CAPACITY_EXHAUSTED
    assert first.bytes_offered == 6
    assert first.bytes_consumed == 2
    assert receiver.offered == [b"abcdef"]
    assert link.ready_byte_count(TransportTestDirection.HOST_TO_RIG) == 4

    second = link.deliver_ready(receiver)
    assert second.status is TransportStatus.OK
    assert second.bytes_offered == 4
    assert second.bytes_consumed == 4
    assert receiver.offered == [b"abcdef", b"cdef"]
    assert link.ready_byte_count(TransportTestDirection.HOST_TO_RIG) == 0


def test_link_zero_delivery_calls_receive_even_without_ready_bytes() -> None:
    link = TransportTestLink()

    @dataclass
    class Receiver:
        role: Role = Role.RIG
        calls: int = 0

        def receive_bytes(self, data: bytes) -> ReceiveResult:
            self.calls += 1
            assert data == b""
            return ReceiveResult(TransportStatus.NOT_READY, 0)

    receiver = Receiver()
    result = link.deliver_zero(receiver)
    assert result.status is TransportStatus.NOT_READY
    assert result.bytes_offered == 0
    assert result.bytes_consumed == 0
    assert receiver.calls == 1


def test_healthy_output_pump_allows_exact_budget_when_output_becomes_quiescent() -> None:
    pair, calls = _make_fake_pump_pair(host_pending=True, rig_pending=False)

    assert pair._pump_healthy_outputs(1) == 1
    assert calls == [TransportTestDirection.HOST_TO_RIG]


def test_healthy_output_pump_counts_each_direction_against_budget() -> None:
    pair, calls = _make_fake_pump_pair(host_pending=True, rig_pending=True)

    with pytest.raises(TransportTestHarnessError) as exc_info:
        pair._pump_healthy_outputs(1)

    assert calls == [TransportTestDirection.HOST_TO_RIG]
    assert pair.rig.output_pending  # type: ignore[attr-defined]
    message = str(exc_info.value)
    assert "transfer limit 1" in message
    assert "host_now_ms=11" in message
    assert "rig_now_ms=22" in message
    assert "host_snapshot=" in message
    assert "rig_snapshot=" in message
    assert "pending_output_count=" in message
    assert "ready_byte_count=" in message


def test_pair_clocks_advance_independently_with_uint32_wrap() -> None:
    uint32_max = (1 << 32) - 1
    with TransportPairHarness(host_now_ms=uint32_max, rig_now_ms=10) as pair:
        pair.advance_host_time()
        assert pair.host_now_ms == 0
        assert pair.rig_now_ms == 10

        pair.advance_rig_time(5)
        assert pair.host_now_ms == 0
        assert pair.rig_now_ms == 15
