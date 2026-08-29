"""Tests for the deliberate public package surface."""

from __future__ import annotations

import hil_rig_protocol
from hil_rig_protocol import (
    ProtocolError,
    Role,
    Transport,
    TransportConfig,
    TransportConfigurationError,
    TransportError,
)


EXPECTED_EXPORTS = {
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
}


def test_public_exports_are_deliberate() -> None:
    assert set(hil_rig_protocol.__all__) == EXPECTED_EXPORTS
    assert "_binding" not in hil_rig_protocol.__all__
    assert "_native" not in hil_rig_protocol.__all__
    assert "ffi" not in hil_rig_protocol.__all__
    assert "lib" not in hil_rig_protocol.__all__

    for name in EXPECTED_EXPORTS:
        assert getattr(hil_rig_protocol, name) is not None


def test_transport_does_not_expose_raw_native_state() -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    try:
        for attribute in ("handle", "native_handle", "_handle", "ffi", "lib"):
            assert not hasattr(transport, attribute)
        assert transport.role is Role.HOST
        assert transport.config.session_seed == 1
        assert transport.closed is False
    finally:
        transport.close()


def test_exception_hierarchy_includes_value_error_for_configuration() -> None:
    assert issubclass(TransportError, ProtocolError)
    assert issubclass(TransportConfigurationError, TransportError)
    assert issubclass(TransportConfigurationError, ValueError)


def test_pr4_operational_methods_are_not_exposed_yet() -> None:
    for name in (
        "reset",
        "notify_link_state",
        "submit_application_data",
        "receive_bytes",
        "process",
        "peek_output",
        "commit_output",
        "read_application_data",
        "read_event",
        "get_status",
    ):
        assert not hasattr(Transport, name)
