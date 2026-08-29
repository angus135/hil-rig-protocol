"""Bounded example of caller-owned servicing for one Python Transport.

This is a synchronous servicing demonstration, not a physical-driver
implementation. A real integration must quiesce the driver and discard its old
receive/transmit queues before reporting a physical disconnect. The servicer
then invalidates its own caller-owned byte state before notifying Transport.

Asynchronous integrations need the same boundary plus a connection-generation
(or equivalent) check so a late write/read completion from an earlier physical
connection cannot be applied to a replacement session.
"""

from __future__ import annotations

import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from hil_rig_protocol import (
    LinkState,
    OperatingMode,
    ReceiveResult,
    Role,
    Transport,
    TransportConfig,
    TransportEvent,
    TransportStatus,
)

UINT32_MASK = (1 << 32) - 1
WriteSome = Callable[[bytes], int]


class _TransportPort(Protocol):
    def reset(self) -> TransportStatus: ...

    def notify_link_state(self, link_state: LinkState, now_ms: int) -> TransportStatus: ...

    def receive_bytes(self, data: bytes | bytearray) -> ReceiveResult: ...

    def process(self, now_ms: int, operating_mode: OperatingMode) -> TransportStatus: ...

    def peek_output(self) -> bytes | None: ...

    def commit_output(self, now_ms: int) -> TransportStatus: ...

    def read_event(self) -> TransportEvent | None: ...

    def read_application_data(self) -> bytes | None: ...

    def close(self) -> None: ...


@dataclass(frozen=True, slots=True)
class DrainResult:
    """Values explicitly removed from the Transport's bounded queues."""

    events: tuple[TransportEvent, ...]
    application_messages: tuple[bytes, ...]


@dataclass(frozen=True, slots=True)
class OutputServiceResult:
    """One external-write attempt and any resulting Transport commit status."""

    accepted: int
    commit_status: TransportStatus | None


class TransportServicingError(RuntimeError):
    """Unexpected native status while servicing a correctly used Transport."""

    def __init__(self, operation: str, status: TransportStatus) -> None:
        super().__init__(f"{operation} returned unexpected status {status.name}")
        self.operation = operation
        self.status = status


def monotonic_now_ms() -> int:
    """Convert caller-selected monotonic time to Transport's wrapped uint32 domain."""
    return int(time.monotonic() * 1000) & UINT32_MASK


def _require_status(
    operation: str,
    status: TransportStatus,
    allowed: tuple[TransportStatus, ...],
) -> TransportStatus:
    if status not in allowed:
        raise TransportServicingError(operation, status)
    return status


class TransportServicer:
    """Small state holder showing responsibilities outside the binding."""

    def __init__(self, transport: _TransportPort) -> None:
        self.transport = transport
        self.incoming = bytearray()
        self.pending_output: bytes | None = None
        self.pending_output_offset = 0

    def invalidate_external_state(self) -> None:
        """Discard caller-owned bytes that cannot cross a link/reset boundary.

        This does not notify or reset Transport and it does not flush a physical
        driver. A real owner must first stop/quiesce its driver and discard old
        driver RX/TX queues, then call the relevant disconnect/reset operation.
        """

        self.incoming.clear()
        self.pending_output = None
        self.pending_output_offset = 0

    def notify_connected(self, now_ms: int) -> TransportStatus:
        status = self.transport.notify_link_state(LinkState.CONNECTED, now_ms)
        return _require_status(
            "notify_link_state(CONNECTED)",
            status,
            (TransportStatus.OK, TransportStatus.CAPACITY_EXHAUSTED),
        )

    def notify_disconnected(self, now_ms: int) -> TransportStatus:
        """Invalidate old caller bytes, then report physical disconnection.

        The caller must stop/quiesce the physical driver and flush its old
        receive/transmit queues before invoking this method.
        """

        self.invalidate_external_state()
        status = self.transport.notify_link_state(LinkState.DISCONNECTED, now_ms)
        return _require_status(
            "notify_link_state(DISCONNECTED)",
            status,
            (TransportStatus.OK, TransportStatus.CAPACITY_EXHAUSTED),
        )

    def reset(self) -> TransportStatus:
        """Discard caller-owned session bytes before the defined local reset."""

        self.invalidate_external_state()
        status = self.transport.reset()
        return _require_status("reset", status, (TransportStatus.OK,))

    def offer_received(self, data: bytes = b"") -> ReceiveResult:
        """Offer all retained bytes and delete only the accepted prefix."""
        self.incoming.extend(data)
        offered: bytes | bytearray = self.incoming if self.incoming else b""
        result = self.transport.receive_bytes(offered)
        _require_status(
            "receive_bytes",
            result.status,
            (
                TransportStatus.OK,
                TransportStatus.NOT_READY,
                TransportStatus.CAPACITY_EXHAUSTED,
            ),
        )
        del self.incoming[: result.bytes_consumed]
        return result

    def process(self, now_ms: int, mode: OperatingMode) -> TransportStatus:
        status = self.transport.process(now_ms, mode)
        return _require_status(
            "process",
            status,
            (
                TransportStatus.OK,
                TransportStatus.NOT_READY,
                TransportStatus.CAPACITY_EXHAUSTED,
                TransportStatus.DELIVERY_FAILED,
            ),
        )

    def drain(self) -> DrainResult:
        """Explicitly release every currently pending event and Application value."""
        events: list[TransportEvent] = []
        while (event := self.transport.read_event()) is not None:
            events.append(event)

        messages: list[bytes] = []
        while (message := self.transport.read_application_data()) is not None:
            messages.append(message)
        return DrainResult(tuple(events), tuple(messages))

    def service_output(self, now_ms: int, write_some: WriteSome) -> OutputServiceResult:
        """Offer only the remaining suffix and commit once after full acceptance."""
        if self.pending_output is None:
            self.pending_output = self.transport.peek_output()
            self.pending_output_offset = 0
        if self.pending_output is None:
            return OutputServiceResult(0, None)

        remaining = self.pending_output[self.pending_output_offset :]
        accepted = write_some(remaining)
        if type(accepted) is not int or not 0 <= accepted <= len(remaining):
            raise ValueError("write_some must return an accepted count within the offered suffix")
        self.pending_output_offset += accepted
        if self.pending_output_offset != len(self.pending_output):
            return OutputServiceResult(accepted, None)

        # Every byte is already accepted by the external writer at this point.
        # Clear caller-owned output state regardless of commit result so a
        # NOT_READY or fault cannot cause those bytes to be retransmitted by the
        # integration layer. Native Transport remains responsible for its own
        # protocol retry/recovery state.
        try:
            commit_status = self.transport.commit_output(now_ms)
        finally:
            self.pending_output = None
            self.pending_output_offset = 0
        _require_status(
            "commit_output",
            commit_status,
            (TransportStatus.OK, TransportStatus.NOT_READY),
        )
        return OutputServiceResult(accepted, commit_status)

    def close(self) -> None:
        self.invalidate_external_state()
        self.transport.close()


def main() -> None:
    """Exercise a bounded servicing slice without opening physical I/O."""
    # The library defaults keep retry timing disabled for deterministic callers.
    # A hardware owner should choose a non-zero timeout/retry policy appropriate
    # to its link. 100 ms / 3 retries is illustrative only: a real timeout must
    # cover physical transmission, OS/driver buffering, scheduler latency, and
    # the peer's expected response time.
    transport = Transport(
        Role.HOST,
        TransportConfig(
            session_seed=1,
            retransmit_timeout_ms=100,
            max_retries=3,
        ),
    )
    service = TransportServicer(transport)
    now_ms = monotonic_now_ms()
    try:
        connected = service.notify_connected(now_ms)
        if connected is TransportStatus.CAPACITY_EXHAUSTED:
            service.drain()

        service.offer_received()  # Zero-byte receive can resume retained native work.
        process_status = service.process(now_ms, OperatingMode.NORMAL)
        if process_status is TransportStatus.CAPACITY_EXHAUSTED:
            service.drain()
        elif process_status is TransportStatus.DELIVERY_FAILED:
            raise RuntimeError("delivery failed; perform caller-defined recovery")

        accepted_bytes = bytearray()

        def write_some(remaining: bytes) -> int:
            accepted = min(3, len(remaining))
            accepted_bytes.extend(remaining[:accepted])
            return accepted

        # A real owner would call this when its writer becomes ready. The bound
        # keeps this example from becoming an automatic polling/retry loop.
        for _ in range(16):
            result = service.service_output(now_ms, write_some)
            if result.commit_status is TransportStatus.NOT_READY:
                # The external bytes were already accepted. Do not resend them;
                # continue by observing/draining current Transport state.
                service.drain()
                break
            if service.pending_output is None or result.accepted == 0:
                break

        # Real physical-disconnect ordering:
        #   1. stop/quiesce the physical driver;
        #   2. flush/discard its old receive and transmit queues;
        #   3. notify_disconnected() invalidates this servicer's retained bytes;
        #   4. Transport observes DISCONNECTED;
        #   5. re-establish the physical driver;
        #   6. notify_connected() reports CONNECTED;
        #   7. resume fresh-session servicing.
        service.notify_disconnected(now_ms)
        service.drain()

        # Do not invalidate caller state merely because SESSION_RESET was read.
        # Peer RESET and replacement-session traffic can validly be processed in
        # one receive call. Internal FAULT is different: normal valid operation
        # should treat it as a test/integration failure and repair it only with
        # the defined local service.reset() operation.
    finally:
        service.close()


if __name__ == "__main__":
    main()
