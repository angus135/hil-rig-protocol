"""Public exception hierarchy for the HIL-RIG Transport binding."""

from __future__ import annotations

from .transport_types import TransportStatus


class ProtocolError(Exception):
    """Base class for errors raised by the HIL-RIG protocol package."""


class TransportError(ProtocolError):
    """Base class for Transport-specific errors.

    ``status`` is populated when the exception is caused by a native Transport
    result. Python-side validation and ownership errors have no native status.
    """

    def __init__(self, message: str, *, status: TransportStatus | None = None) -> None:
        super().__init__(message)
        self.status = status


class TransportConfigurationError(TransportError, ValueError):
    """The requested Transport configuration is invalid or unsupported."""


class TransportCreationError(TransportError):
    """Native Transport creation failed for a reason not classified more specifically."""


class TransportBindingError(TransportError):
    """The private Python/native binding returned an inconsistent result."""


class TransportClosedError(TransportError):
    """An operation requiring a live Transport was attempted after close()."""


class TransportOwnershipError(TransportError):
    """A live Transport was accessed from a thread other than its owner."""


class TransportInternalError(TransportError):
    """The native Transport reported an internal invariant failure."""


__all__ = [
    "ProtocolError",
    "TransportError",
    "TransportConfigurationError",
    "TransportCreationError",
    "TransportBindingError",
    "TransportClosedError",
    "TransportOwnershipError",
    "TransportInternalError",
]
