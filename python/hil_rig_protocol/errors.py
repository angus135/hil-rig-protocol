"""Public exception hierarchy for the HIL-RIG protocol bindings."""

from __future__ import annotations

from .application_types import ApplicationStatus
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
    """The native Transport reported an internal invariant failure.

    ``bytes_consumed`` preserves the accepted receive prefix when an internal
    failure is reported by ``Transport.receive_bytes()``. It is ``None`` for
    internal failures from other Transport operations.
    """

    def __init__(
        self,
        message: str,
        *,
        status: TransportStatus | None = None,
        bytes_consumed: int | None = None,
    ) -> None:
        super().__init__(message, status=status)
        self.bytes_consumed: int | None = bytes_consumed


class ApplicationError(ProtocolError):
    """Application failure with the exact native status, when one exists."""

    def __init__(self, message: str, *, status: ApplicationStatus | None = None) -> None:
        super().__init__(message)
        self.status = status


class ApplicationConfigurationError(ApplicationError, ValueError):
    """Native initialization rejected the requested codec configuration."""


class ApplicationEncodeError(ApplicationError):
    """Native validation or encoding rejected a represented message."""


class ApplicationDecodeError(ApplicationError):
    """Encoded data is invalid or outside the supported Python subset."""


class ApplicationBindingError(ApplicationError):
    """The private binding returned an unknown status or inconsistent result."""


class ApplicationOwnershipError(ApplicationError):
    """The codec was accessed from a thread other than its creating thread."""


class ApplicationInternalError(ApplicationError):
    """Native Application code reported an internal invariant failure."""


__all__ = [
    "ApplicationError",
    "ApplicationConfigurationError",
    "ApplicationEncodeError",
    "ApplicationDecodeError",
    "ApplicationBindingError",
    "ApplicationOwnershipError",
    "ApplicationInternalError",
    "ProtocolError",
    "TransportError",
    "TransportConfigurationError",
    "TransportCreationError",
    "TransportBindingError",
    "TransportClosedError",
    "TransportOwnershipError",
    "TransportInternalError",
]
