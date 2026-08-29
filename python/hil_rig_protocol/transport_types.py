"""Public value types for the HIL-RIG Transport binding.

The integer enum values are intentionally explicit and stable. Tests compare
all members against the compiled CFFI constants so native/Python drift fails CI.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

from . import _binding


class TransportStatus(IntEnum):
    """Result of a Transport operation."""

    OK = 0
    INVALID_ARGUMENT = 1
    BUFFER_TOO_SMALL = 2
    UNSUPPORTED_CONFIGURATION = 3
    MESSAGE_TOO_LARGE = 4
    CAPACITY_EXHAUSTED = 5
    DELIVERY_FAILED = 6
    TIMEOUT = 7
    NOT_READY = 8
    NOT_IMPLEMENTED = 9
    INTERNAL_ERROR = 10


class Role(IntEnum):
    """Logical endpoint role fixed for a Transport lifetime."""

    HOST = 0
    RIG = 1


class LinkState(IntEnum):
    """State of the caller-owned byte-stream link."""

    DISCONNECTED = 0
    CONNECTED = 1


class OperatingMode(IntEnum):
    """Caller-selected local Transport operating mode."""

    NORMAL = 0
    BULK_TRANSFER = 1
    QUIET_REAL_TIME = 2


class SessionState(IntEnum):
    """High-level Transport session state."""

    DISCONNECTED = 0
    CONNECTING = 1
    ESTABLISHED = 2
    RECOVERING = 3
    FAULT = 4


class Failure(IntEnum):
    """High-level Transport failure classification."""

    NONE = 0
    LINK_LOST = 1
    CONNECTION_TIMEOUT = 2
    DELIVERY = 3
    PROTOCOL = 4
    CAPACITY = 5
    LOCAL_RESET = 6
    INTERNAL = 7


class EventType(IntEnum):
    """High-level Transport event category."""

    NONE = 0
    SESSION_ESTABLISHED = 1
    SESSION_RESET = 2
    DELIVERY_CONFIRMED = 3
    DELIVERY_FAILED = 4
    PROTOCOL_ERROR = 5
    CAPACITY_EXHAUSTED = 6
    LINK_STATE_CHANGED = 7


_SIZE_MAX = (1 << (8 * _binding.ffi.sizeof("size_t"))) - 1
_UINT64_MAX = (1 << 64) - 1
_UINT32_MAX = (1 << 32) - 1
_UINT16_MAX = (1 << 16) - 1
_UINT8_MAX = (1 << 8) - 1


def _validate_integer(name: str, value: object, minimum: int, maximum: int) -> None:
    if type(value) is not int:
        raise TypeError(f"{name} must be an int")
    if value < minimum or value > maximum:
        raise ValueError(f"{name} must be in the range {minimum}..{maximum}")


@dataclass(frozen=True, slots=True)
class TransportConfig:
    """Immutable Transport configuration requested by Python callers.

    ``session_seed=None`` is resolved when a :class:`Transport` is created:
    HOST generates a secure nonreserved seed, while RIG resolves it to zero.
    """

    max_application_message_size: int = 512
    max_encoded_frame_size: int = 640
    session_seed: int | None = None
    initial_reliable_sequence: int = 0
    connection_timeout_ms: int = 0
    retransmit_timeout_ms: int = 0
    max_retries: int = 0

    def __post_init__(self) -> None:
        _validate_integer(
            "max_application_message_size",
            self.max_application_message_size,
            1,
            _SIZE_MAX,
        )
        _validate_integer(
            "max_encoded_frame_size",
            self.max_encoded_frame_size,
            1,
            _SIZE_MAX,
        )
        if self.session_seed is not None:
            _validate_integer("session_seed", self.session_seed, 0, _UINT64_MAX)
        _validate_integer(
            "initial_reliable_sequence",
            self.initial_reliable_sequence,
            0,
            _UINT16_MAX,
        )
        _validate_integer("connection_timeout_ms", self.connection_timeout_ms, 0, _UINT32_MAX)
        _validate_integer("retransmit_timeout_ms", self.retransmit_timeout_ms, 0, _UINT32_MAX)
        _validate_integer("max_retries", self.max_retries, 0, _UINT8_MAX)


@dataclass(frozen=True, slots=True)
class ReceiveResult:
    """Result of a future encoded-byte receive operation."""

    status: TransportStatus
    bytes_consumed: int


@dataclass(frozen=True, slots=True)
class TransportEvent:
    """Immutable Python representation of a native Transport event."""

    type: EventType
    status: TransportStatus
    failure: Failure
    required_capacity: int


@dataclass(frozen=True, slots=True)
class TransportSnapshot:
    """Immutable Python representation of a native Transport status snapshot."""

    role: Role
    link_state: LinkState
    session_state: SessionState
    operating_mode: OperatingMode | None
    output_pending: bool
    application_message_pending: bool
    event_pending: bool
    reliable_delivery_pending: bool
    last_failure: Failure


__all__ = [
    "TransportStatus",
    "Role",
    "LinkState",
    "OperatingMode",
    "SessionState",
    "Failure",
    "EventType",
    "TransportConfig",
    "ReceiveResult",
    "TransportEvent",
    "TransportSnapshot",
]
