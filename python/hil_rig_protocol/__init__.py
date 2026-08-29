"""Supported Python interface for the HIL-RIG protocol.

This release provides the complete caller-driven Transport facade, including
validated configuration, native lifetime ownership, encoded I/O servicing,
opaque Application data, events, and status snapshots.
"""

from .errors import (
    ProtocolError,
    TransportBindingError,
    TransportClosedError,
    TransportConfigurationError,
    TransportCreationError,
    TransportError,
    TransportInternalError,
    TransportOwnershipError,
)
from .transport import Transport
from .transport_types import (
    EventType,
    Failure,
    LinkState,
    OperatingMode,
    ReceiveResult,
    Role,
    SessionState,
    TransportConfig,
    TransportEvent,
    TransportSnapshot,
    TransportStatus,
)

__all__ = [
    "ProtocolError",
    "TransportError",
    "TransportConfigurationError",
    "TransportCreationError",
    "TransportBindingError",
    "TransportClosedError",
    "TransportOwnershipError",
    "TransportInternalError",
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
    "Transport",
]
