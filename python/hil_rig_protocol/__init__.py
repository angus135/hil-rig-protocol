"""Supported Python interface for the HIL-RIG protocol.

This release provides Transport value types, validated configuration, and
native lifetime ownership. Operational Transport methods are added in later
binding PRs.
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
