"""Deterministic public-API HOST/RIG Transport integration harness.

The harness models only the caller-owned byte-stream boundary around two real
:class:`hil_rig_protocol.Transport` objects. It deliberately does not model or
inspect Transport frames, handshake messages, acknowledgements, COBS, CRCs,
session identifiers, or any other protocol-private state.

Transport output remains pinned while the simulated external writer accepts it
in one or more pieces. The link commits the native item only after every output
byte has been externally accepted. Once committed, complete output is copied
into link-owned storage. Delivery removes only the exact prefix reported by
``receive_bytes()`` and retains the unconsumed suffix for a later call.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import Enum, auto
from typing import Protocol

from hil_rig_protocol import (
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

_UINT32_MAX = (1 << 32) - 1


class TransportTestDirection(Enum):
    """Direction of opaque bytes through the simulated duplex link."""

    HOST_TO_RIG = auto()
    RIG_TO_HOST = auto()


@dataclass(frozen=True, slots=True)
class TransportTestOutputHandle:
    """Stable identity for one complete output item accepted by the link."""

    direction: TransportTestDirection
    ordinal: int


@dataclass(frozen=True, slots=True)
class TransportTestOutputItem:
    """One complete immutable output item owned by the simulated link."""

    handle: TransportTestOutputHandle
    data: bytes


@dataclass(frozen=True, slots=True)
class TransportLinkAcceptResult:
    """Result of one simulated external-write acceptance operation."""

    status: TransportStatus | None
    handle: TransportTestOutputHandle | None
    size: int
    bytes_accepted: int
    accepted_offset: int
    committed: bool


@dataclass(frozen=True, slots=True)
class TransportLinkDeliveryResult:
    """Result of one caller-owned ready-stream delivery operation."""

    status: TransportStatus | None
    bytes_offered: int
    bytes_consumed: int


@dataclass(frozen=True, slots=True)
class TransportPairProcessResult:
    """Results of servicing both endpoints once at their independent clocks."""

    host_status: TransportStatus
    rig_status: TransportStatus


@dataclass(frozen=True, slots=True)
class TransportPairTransferResult:
    """Result of accepting and immediately delivering one complete output."""

    accept: TransportLinkAcceptResult
    delivery: TransportLinkDeliveryResult | None


@dataclass(frozen=True, slots=True)
class TransportSessionEstablishmentResult:
    """Summary of a successful bounded healthy-path establishment pump."""

    service_steps: int
    transfers: int


class TransportTestHarnessError(RuntimeError):
    """Failure of a healthy-path test helper, separate from protocol statuses."""

    def __init__(self, message: str, *, status: TransportStatus | None = None) -> None:
        super().__init__(message)
        self.status = status


class _ReceiveEndpoint(Protocol):
    @property
    def role(self) -> Role: ...

    def receive_bytes(self, data: bytes) -> ReceiveResult: ...


class _OutputEndpoint(Protocol):
    @property
    def role(self) -> Role: ...

    def peek_output(self) -> bytes | None: ...

    def commit_output(self, now_ms: int) -> TransportStatus: ...


@dataclass(slots=True)
class _PendingExternalWrite:
    data: bytes
    accepted_offset: int = 0


@dataclass(slots=True)
class _DirectionState:
    pending_external_write: _PendingExternalWrite | None
    accepted: deque[TransportTestOutputItem]
    held: deque[TransportTestOutputItem]
    ready_bytes: bytearray

    @classmethod
    def create(cls) -> _DirectionState:
        return cls(
            pending_external_write=None,
            accepted=deque(),
            held=deque(),
            ready_bytes=bytearray(),
        )


class TransportTestLink:
    """Deterministic full-duplex byte-stream boundary for integration tests.

    The link models caller-owned partial external writes separately from native
    Transport ownership. A peeked item remains pinned until its complete byte
    string has been externally accepted; only then is ``commit_output()``
    called and the item moved into link-owned accepted storage.

    Once committed, accepted items can be moved into a byte stream and offered
    to the peer using arbitrary receive chunk sizes. Exact unconsumed suffixes
    are retained and zero-byte receive can be invoked explicitly.

    Complete committed items retain stable opaque handles so tests can delay,
    drop, duplicate, release, or bytewise-corrupt external traffic without
    decoding Transport frames or depending on protocol-private structure.
    """

    def __init__(self) -> None:
        self._states = {
            TransportTestDirection.HOST_TO_RIG: _DirectionState.create(),
            TransportTestDirection.RIG_TO_HOST: _DirectionState.create(),
        }
        self._next_ordinal = 0

    @staticmethod
    def output_direction_for_role(role: Role) -> TransportTestDirection:
        if role is Role.HOST:
            return TransportTestDirection.HOST_TO_RIG
        if role is Role.RIG:
            return TransportTestDirection.RIG_TO_HOST
        raise TransportTestHarnessError(f"unsupported endpoint role {role!r}")

    @staticmethod
    def input_direction_for_role(role: Role) -> TransportTestDirection:
        if role is Role.HOST:
            return TransportTestDirection.RIG_TO_HOST
        if role is Role.RIG:
            return TransportTestDirection.HOST_TO_RIG
        raise TransportTestHarnessError(f"unsupported endpoint role {role!r}")

    def accept_output(
        self,
        sender: _OutputEndpoint,
        now_ms: int,
        max_bytes: int | None = None,
    ) -> TransportLinkAcceptResult:
        """Accept at most ``max_bytes`` of one pinned output item.

        ``None`` accepts the complete remaining suffix. Partial acceptance keeps
        the native item pinned and records a caller-owned offset. Every later
        acceptance re-peeks and verifies the complete output remains byte-for-
        byte stable. Native output is committed exactly once, and only after
        the final external byte has been accepted.
        """

        if max_bytes is not None:
            if type(max_bytes) is not int:
                raise TypeError("max_bytes must be an int or None")
            if max_bytes < 0:
                raise ValueError("max_bytes must be non-negative")

        direction = self.output_direction_for_role(sender.role)
        state = self._states[direction]
        output = sender.peek_output()

        if state.pending_external_write is None:
            if output is None:
                return TransportLinkAcceptResult(
                    status=None,
                    handle=None,
                    size=0,
                    bytes_accepted=0,
                    accepted_offset=0,
                    committed=False,
                )
            pending = _PendingExternalWrite(data=bytes(output))
            state.pending_external_write = pending
        else:
            pending = state.pending_external_write
            if output is None:
                raise TransportTestHarnessError(
                    f"{direction.name} pinned output disappeared during partial external write"
                )
            if bytes(output) != pending.data:
                raise TransportTestHarnessError(
                    f"{direction.name} pinned output changed during partial external write"
                )

        remaining = len(pending.data) - pending.accepted_offset
        accepted_now = remaining if max_bytes is None else min(remaining, max_bytes)
        pending.accepted_offset += accepted_now

        if pending.accepted_offset < len(pending.data):
            return TransportLinkAcceptResult(
                status=None,
                handle=None,
                size=len(pending.data),
                bytes_accepted=accepted_now,
                accepted_offset=pending.accepted_offset,
                committed=False,
            )

        status = sender.commit_output(now_ms)
        if status is not TransportStatus.OK:
            return TransportLinkAcceptResult(
                status=status,
                handle=None,
                size=len(pending.data),
                bytes_accepted=accepted_now,
                accepted_offset=pending.accepted_offset,
                committed=False,
            )

        handle = TransportTestOutputHandle(direction, self._next_ordinal)
        self._next_ordinal += 1
        state.accepted.append(TransportTestOutputItem(handle=handle, data=pending.data))
        state.pending_external_write = None
        return TransportLinkAcceptResult(
            status=TransportStatus.OK,
            handle=handle,
            size=len(pending.data),
            bytes_accepted=accepted_now,
            accepted_offset=len(pending.data),
            committed=True,
        )


    def take_next_accepted(
        self, direction: TransportTestDirection
    ) -> TransportTestOutputItem | None:
        """Remove and return the oldest complete committed output item."""

        state = self._states[direction]
        return state.accepted.popleft() if state.accepted else None

    def take_accepted(
        self, handle: TransportTestOutputHandle
    ) -> TransportTestOutputItem | None:
        """Remove one exact committed item by stable link identity."""

        state = self._states[handle.direction]
        for index, item in enumerate(state.accepted):
            if item.handle == handle:
                del state.accepted[index]
                return item
        return None

    def drop_accepted(self, handle: TransportTestOutputHandle) -> bool:
        """Drop one exact committed output item."""

        return self.take_accepted(handle) is not None

    def drop_next_accepted(self, direction: TransportTestDirection) -> bool:
        """Drop the oldest committed output item in one direction."""

        return self.take_next_accepted(direction) is not None

    def duplicate_accepted(
        self, handle: TransportTestOutputHandle
    ) -> TransportTestOutputHandle | None:
        """Duplicate one exact committed item while preserving the original."""

        state = self._states[handle.direction]
        for index, item in enumerate(state.accepted):
            if item.handle == handle:
                duplicate_handle = TransportTestOutputHandle(
                    handle.direction, self._next_ordinal
                )
                self._next_ordinal += 1
                duplicate = TransportTestOutputItem(duplicate_handle, item.data)
                state.accepted.insert(index + 1, duplicate)
                return duplicate_handle
        return None

    def duplicate_next_accepted(
        self, direction: TransportTestDirection
    ) -> TransportTestOutputHandle | None:
        """Duplicate the oldest committed item in one direction."""

        state = self._states[direction]
        if not state.accepted:
            return None
        return self.duplicate_accepted(state.accepted[0].handle)

    def hold_accepted(self, handle: TransportTestOutputHandle) -> bool:
        """Move one exact committed item into delayed storage."""

        item = self.take_accepted(handle)
        if item is None:
            return False
        self._states[handle.direction].held.append(item)
        return True

    def hold_next_accepted(self, direction: TransportTestDirection) -> bool:
        """Delay the oldest committed item in one direction."""

        state = self._states[direction]
        if not state.accepted:
            return False
        return self.hold_accepted(state.accepted[0].handle)

    def release_held(self, handle: TransportTestOutputHandle) -> bool:
        """Return one exact delayed item to the committed-item queue."""

        state = self._states[handle.direction]
        for index, item in enumerate(state.held):
            if item.handle == handle:
                del state.held[index]
                state.accepted.append(item)
                return True
        return False

    def release_oldest_held(self, direction: TransportTestDirection) -> bool:
        """Return the oldest delayed item to the committed-item queue."""

        state = self._states[direction]
        if not state.held:
            return False
        state.accepted.append(state.held.popleft())
        return True

    def corrupt_accepted_byte(
        self, handle: TransportTestOutputHandle, byte_offset: int, xor_mask: int
    ) -> bool:
        """XOR one byte of one exact committed opaque item."""

        if type(byte_offset) is not int or type(xor_mask) is not int:
            raise TypeError("byte_offset and xor_mask must be ints")
        if byte_offset < 0 or xor_mask < 0 or xor_mask > 0xFF:
            raise ValueError("invalid corruption offset or mask")
        state = self._states[handle.direction]
        for index, item in enumerate(state.accepted):
            if item.handle == handle:
                if byte_offset >= len(item.data):
                    return False
                data = bytearray(item.data)
                data[byte_offset] ^= xor_mask
                state.accepted[index] = TransportTestOutputItem(item.handle, bytes(data))
                return True
        return False

    def corrupt_next_accepted_byte(
        self, direction: TransportTestDirection, byte_offset: int, xor_mask: int
    ) -> bool:
        """XOR one byte of the oldest committed item."""

        state = self._states[direction]
        if not state.accepted:
            return False
        return self.corrupt_accepted_byte(
            state.accepted[0].handle, byte_offset, xor_mask
        )

    def queue_next_accepted_for_delivery(
        self, direction: TransportTestDirection
    ) -> bool:
        """Move the oldest committed item into the direction byte stream."""

        state = self._states[direction]
        if not state.accepted:
            return False
        return self.queue_accepted_for_delivery(state.accepted[0].handle)

    def queue_all_accepted_for_delivery(
        self, direction: TransportTestDirection
    ) -> int:
        """Join every committed item in a direction into one ready byte stream."""

        count = 0
        while self.queue_next_accepted_for_delivery(direction):
            count += 1
        return count

    def inject_ready_bytes(
        self, direction: TransportTestDirection, data: bytes | bytearray | memoryview
    ) -> None:
        """Inject caller-owned opaque raw bytes into a ready byte stream."""

        self._states[direction].ready_bytes.extend(bytes(data))

    def accepted_item(
        self, handle: TransportTestOutputHandle
    ) -> TransportTestOutputItem | None:
        """Return one committed item without changing ownership or queue order."""

        for item in self._states[handle.direction].accepted:
            if item.handle == handle:
                return item
        return None

    def queue_accepted_for_delivery(self, handle: TransportTestOutputHandle) -> bool:
        """Move one exact accepted item into its direction's ready byte stream."""

        state = self._states[handle.direction]
        for index, item in enumerate(state.accepted):
            if item.handle == handle:
                del state.accepted[index]
                state.ready_bytes.extend(item.data)
                return True
        return False

    def deliver_ready(
        self,
        receiver: _ReceiveEndpoint,
        max_bytes: int | None = None,
    ) -> TransportLinkDeliveryResult:
        """Offer ready bytes and retain exactly the unconsumed caller-owned suffix."""

        direction = self.input_direction_for_role(receiver.role)
        state = self._states[direction]
        if max_bytes is not None:
            if type(max_bytes) is not int:
                raise TypeError("max_bytes must be an int or None")
            if max_bytes < 0:
                raise ValueError("max_bytes must be non-negative")

        available = len(state.ready_bytes)
        offered_size = available if max_bytes is None else min(available, max_bytes)
        if offered_size == 0:
            return TransportLinkDeliveryResult(
                status=None,
                bytes_offered=0,
                bytes_consumed=0,
            )

        offered = bytes(state.ready_bytes[:offered_size])
        receive = receiver.receive_bytes(offered)
        if receive.bytes_consumed < 0 or receive.bytes_consumed > offered_size:
            raise TransportTestHarnessError(
                "receiver violated bytes_consumed prefix contract",
                status=receive.status,
            )

        del state.ready_bytes[: receive.bytes_consumed]
        return TransportLinkDeliveryResult(
            status=receive.status,
            bytes_offered=offered_size,
            bytes_consumed=receive.bytes_consumed,
        )

    def deliver_zero(self, receiver: _ReceiveEndpoint) -> TransportLinkDeliveryResult:
        """Call the real receive path with no new bytes to resume retained work."""

        receive = receiver.receive_bytes(b"")
        if receive.bytes_consumed != 0:
            raise TransportTestHarnessError(
                "zero-byte receive reported nonzero consumption",
                status=receive.status,
            )
        return TransportLinkDeliveryResult(
            status=receive.status,
            bytes_offered=0,
            bytes_consumed=0,
        )

    def clear(self) -> None:
        """Discard all simulated traffic while preserving handle uniqueness."""

        for state in self._states.values():
            state.pending_external_write = None
            state.accepted.clear()
            state.held.clear()
            state.ready_bytes.clear()

    def pending_output_count(self, direction: TransportTestDirection) -> int:
        return int(self._states[direction].pending_external_write is not None)

    def pending_output_size(self, direction: TransportTestDirection) -> int:
        pending = self._states[direction].pending_external_write
        return 0 if pending is None else len(pending.data)

    def pending_output_accepted_offset(self, direction: TransportTestDirection) -> int:
        pending = self._states[direction].pending_external_write
        return 0 if pending is None else pending.accepted_offset

    def pending_output_remaining(self, direction: TransportTestDirection) -> int:
        pending = self._states[direction].pending_external_write
        if pending is None:
            return 0
        return len(pending.data) - pending.accepted_offset

    def accepted_item_count(self, direction: TransportTestDirection) -> int:
        return len(self._states[direction].accepted)

    def held_item_count(self, direction: TransportTestDirection) -> int:
        return len(self._states[direction].held)

    def ready_byte_count(self, direction: TransportTestDirection) -> int:
        return len(self._states[direction].ready_bytes)


class TransportPairHarness:
    """Own two real public Transports, a simulated link, and deterministic clocks."""

    DEFAULT_HOST_CONFIG = TransportConfig(
        session_seed=0x81230000,
        initial_reliable_sequence=10,
        retransmit_timeout_ms=10,
        max_retries=2,
    )
    DEFAULT_RIG_CONFIG = TransportConfig(
        session_seed=0,
        initial_reliable_sequence=500,
        retransmit_timeout_ms=10,
        max_retries=2,
    )

    def __init__(
        self,
        *,
        host_config: TransportConfig = DEFAULT_HOST_CONFIG,
        rig_config: TransportConfig = DEFAULT_RIG_CONFIG,
        host_now_ms: int = 0,
        rig_now_ms: int = 0,
    ) -> None:
        self.host = Transport(Role.HOST, host_config)
        try:
            self.rig = Transport(Role.RIG, rig_config)
        except BaseException:
            self.host.close()
            raise
        self.link = TransportTestLink()
        self.host_now_ms = self._validate_clock(host_now_ms)
        self.rig_now_ms = self._validate_clock(rig_now_ms)
        self._closed = False

    def __enter__(self) -> TransportPairHarness:
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> bool:
        self.close()
        return False

    def close(self) -> None:
        if self._closed:
            return
        self.rig.close()
        self.host.close()
        self._closed = True

    def connect(self) -> tuple[TransportStatus, TransportStatus]:
        """Report both physical links connected using each endpoint's current time."""

        host_status = self.host.notify_link_state(LinkState.CONNECTED, self.host_now_ms)
        rig_status = self.rig.notify_link_state(LinkState.CONNECTED, self.rig_now_ms)
        return host_status, rig_status

    def process_host(
        self, operating_mode: OperatingMode = OperatingMode.NORMAL
    ) -> TransportStatus:
        return self.host.process(self.host_now_ms, operating_mode)

    def process_rig(
        self, operating_mode: OperatingMode = OperatingMode.NORMAL
    ) -> TransportStatus:
        return self.rig.process(self.rig_now_ms, operating_mode)

    def process_both(
        self, operating_mode: OperatingMode = OperatingMode.NORMAL
    ) -> TransportPairProcessResult:
        """Always service both peers once, even when the first returns non-OK."""

        host_status = self.process_host(operating_mode)
        rig_status = self.process_rig(operating_mode)
        return TransportPairProcessResult(host_status, rig_status)

    def accept_output(
        self,
        direction: TransportTestDirection,
        max_bytes: int | None = None,
    ) -> TransportLinkAcceptResult:
        """Accept part or all of one output using the direction's sender clock."""

        sender, _, sender_now = self._endpoints_for_direction(direction)
        return self.link.accept_output(sender, sender_now, max_bytes=max_bytes)

    def deliver_ready(
        self,
        direction: TransportTestDirection,
        max_bytes: int | None = None,
    ) -> TransportLinkDeliveryResult:
        """Deliver ready bytes to the direction's receiver."""

        _, receiver, _ = self._endpoints_for_direction(direction)
        return self.link.deliver_ready(receiver, max_bytes=max_bytes)

    def deliver_zero(
        self, direction: TransportTestDirection
    ) -> TransportLinkDeliveryResult:
        """Invoke zero-byte receive at the direction's receiver."""

        _, receiver, _ = self._endpoints_for_direction(direction)
        return self.link.deliver_zero(receiver)

    def transfer_one_output(
        self, direction: TransportTestDirection
    ) -> TransportPairTransferResult:
        """Accept, commit, queue, and immediately deliver one exact output item."""

        accept = self.accept_output(direction)
        if accept.handle is None:
            return TransportPairTransferResult(accept=accept, delivery=None)
        if not self.link.queue_accepted_for_delivery(accept.handle):
            raise TransportTestHarnessError("newly accepted output item could not be found")
        delivery = self.deliver_ready(direction)
        return TransportPairTransferResult(accept=accept, delivery=delivery)

    def establish_clean_session(
        self,
        *,
        max_service_steps: int = 32,
        max_transfers_per_step: int = 16,
    ) -> TransportSessionEstablishmentResult:
        """Drive a bounded healthy caller loop until both public snapshots establish."""

        if max_service_steps <= 0 or max_transfers_per_step <= 0:
            raise ValueError("service and transfer bounds must be positive")

        transfer_count = 0
        for step in range(1, max_service_steps + 1):
            process = self.process_both()
            self._require_healthy_status(process.host_status, "HOST process")
            self._require_healthy_status(process.rig_status, "RIG process")

            transfer_count += self._pump_healthy_outputs(max_transfers_per_step)

            host_snapshot = self.host.get_status()
            rig_snapshot = self.rig.get_status()
            if (
                host_snapshot.session_state is SessionState.ESTABLISHED
                and rig_snapshot.session_state is SessionState.ESTABLISHED
            ):
                return TransportSessionEstablishmentResult(step, transfer_count)

            self.advance_both_times()

        raise TransportTestHarnessError(
            "session did not establish within "
            f"{max_service_steps} service steps; {self._format_diagnostics()}"
        )

    def set_host_time(self, now_ms: int) -> None:
        self.host_now_ms = self._validate_clock(now_ms)

    def set_rig_time(self, now_ms: int) -> None:
        self.rig_now_ms = self._validate_clock(now_ms)

    def set_both_times(self, now_ms: int) -> None:
        value = self._validate_clock(now_ms)
        self.host_now_ms = value
        self.rig_now_ms = value

    def advance_host_time(self, delta_ms: int = 1) -> None:
        self.host_now_ms = self._advance_clock(self.host_now_ms, delta_ms)

    def advance_rig_time(self, delta_ms: int = 1) -> None:
        self.rig_now_ms = self._advance_clock(self.rig_now_ms, delta_ms)

    def advance_both_times(self, delta_ms: int = 1) -> None:
        self.advance_host_time(delta_ms)
        self.advance_rig_time(delta_ms)

    def _pump_healthy_outputs(self, max_transfers: int) -> int:
        if type(max_transfers) is not int:
            raise TypeError("max_transfers must be an int")
        if max_transfers <= 0:
            raise ValueError("max_transfers must be positive")

        transfer_count = 0
        directions = (
            TransportTestDirection.HOST_TO_RIG,
            TransportTestDirection.RIG_TO_HOST,
        )

        while True:
            transferred_this_sweep = False
            for direction in directions:
                sender, _, _ = self._endpoints_for_direction(direction)
                if not sender.get_status().output_pending:
                    continue

                if transfer_count >= max_transfers:
                    raise TransportTestHarnessError(
                        "healthy output pump exceeded transfer limit "
                        f"{max_transfers}; {self._format_diagnostics()}"
                    )

                transfer = self.transfer_one_output(direction)
                if transfer.accept.status is None:
                    raise TransportTestHarnessError(
                        "snapshot reported pending output but peek returned no output"
                    )
                self._require_healthy_status(
                    transfer.accept.status,
                    f"{direction.name} output commit",
                )
                if transfer.delivery is None or transfer.delivery.status is None:
                    raise TransportTestHarnessError(
                        "accepted output did not reach the peer receive path"
                    )
                self._require_healthy_status(
                    transfer.delivery.status,
                    f"{direction.name} receive",
                )
                transfer_count += 1
                transferred_this_sweep = True

            if not transferred_this_sweep:
                return transfer_count

    def _format_diagnostics(self) -> str:
        host_snapshot = self.host.get_status()
        rig_snapshot = self.rig.get_status()
        link_parts: list[str] = []
        for direction in (
            TransportTestDirection.HOST_TO_RIG,
            TransportTestDirection.RIG_TO_HOST,
        ):
            link_parts.append(
                f"{direction.name}["
                f"pending_output_count={self.link.pending_output_count(direction)}, "
                f"pending_output_size={self.link.pending_output_size(direction)}, "
                "pending_output_accepted_offset="
                f"{self.link.pending_output_accepted_offset(direction)}, "
                f"pending_output_remaining={self.link.pending_output_remaining(direction)}, "
                f"accepted_item_count={self.link.accepted_item_count(direction)}, "
                f"held_item_count={self.link.held_item_count(direction)}, "
                f"ready_byte_count={self.link.ready_byte_count(direction)}]"
            )
        return (
            f"host_now_ms={self.host_now_ms}; rig_now_ms={self.rig_now_ms}; "
            f"host_snapshot={host_snapshot!r}; rig_snapshot={rig_snapshot!r}; "
            + "; ".join(link_parts)
        )

    def _endpoints_for_direction(
        self, direction: TransportTestDirection
    ) -> tuple[Transport, Transport, int]:
        if direction is TransportTestDirection.HOST_TO_RIG:
            return self.host, self.rig, self.host_now_ms
        if direction is TransportTestDirection.RIG_TO_HOST:
            return self.rig, self.host, self.rig_now_ms
        raise TransportTestHarnessError(f"unsupported link direction {direction!r}")

    @staticmethod
    def _require_healthy_status(status: TransportStatus, operation: str) -> None:
        if status is not TransportStatus.OK:
            raise TransportTestHarnessError(
                f"{operation} returned {status.name} during healthy servicing",
                status=status,
            )

    @staticmethod
    def _validate_clock(now_ms: int) -> int:
        if type(now_ms) is not int:
            raise TypeError("test clock must be an int")
        if now_ms < 0 or now_ms > _UINT32_MAX:
            raise ValueError(f"test clock must be in the range 0..{_UINT32_MAX}")
        return now_ms

    @staticmethod
    def _advance_clock(now_ms: int, delta_ms: int) -> int:
        if type(delta_ms) is not int:
            raise TypeError("clock delta must be an int")
        if delta_ms < 0 or delta_ms > _UINT32_MAX:
            raise ValueError(f"clock delta must be in the range 0..{_UINT32_MAX}")
        return (now_ms + delta_ms) & _UINT32_MAX


def drain_events(transport: Transport) -> list[TransportEvent]:
    """Explicitly drain one endpoint's public event FIFO."""

    events: list[TransportEvent] = []
    while True:
        event = transport.read_event()
        if event is None:
            return events
        events.append(event)
