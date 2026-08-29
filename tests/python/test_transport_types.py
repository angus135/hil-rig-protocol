"""Tests for public Transport enums and immutable value types."""

from __future__ import annotations

from dataclasses import FrozenInstanceError

import pytest

from hil_rig_protocol import (
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
from hil_rig_protocol import _binding


def test_transport_status_matches_compiled_c_constants() -> None:
    expected = {
        TransportStatus.OK: "HIL_TRANSPORT_STATUS_OK",
        TransportStatus.INVALID_ARGUMENT: "HIL_TRANSPORT_STATUS_INVALID_ARGUMENT",
        TransportStatus.BUFFER_TOO_SMALL: "HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL",
        TransportStatus.UNSUPPORTED_CONFIGURATION: "HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION",
        TransportStatus.MESSAGE_TOO_LARGE: "HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE",
        TransportStatus.CAPACITY_EXHAUSTED: "HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED",
        TransportStatus.DELIVERY_FAILED: "HIL_TRANSPORT_STATUS_DELIVERY_FAILED",
        TransportStatus.TIMEOUT: "HIL_TRANSPORT_STATUS_TIMEOUT",
        TransportStatus.NOT_READY: "HIL_TRANSPORT_STATUS_NOT_READY",
        TransportStatus.NOT_IMPLEMENTED: "HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED",
        TransportStatus.INTERNAL_ERROR: "HIL_TRANSPORT_STATUS_INTERNAL_ERROR",
    }
    for member, native_name in expected.items():
        assert int(member) == getattr(_binding.lib, native_name)


def test_role_matches_compiled_c_constants() -> None:
    assert int(Role.HOST) == _binding.lib.HIL_TRANSPORT_ROLE_HOST
    assert int(Role.RIG) == _binding.lib.HIL_TRANSPORT_ROLE_RIG


def test_link_state_matches_compiled_c_constants() -> None:
    assert int(LinkState.DISCONNECTED) == _binding.lib.HIL_TRANSPORT_LINK_STATE_DISCONNECTED
    assert int(LinkState.CONNECTED) == _binding.lib.HIL_TRANSPORT_LINK_STATE_CONNECTED


def test_operating_mode_matches_compiled_c_constants() -> None:
    assert int(OperatingMode.NORMAL) == _binding.lib.HIL_TRANSPORT_OPERATING_MODE_NORMAL
    assert int(OperatingMode.BULK_TRANSFER) == _binding.lib.HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER
    assert (
        int(OperatingMode.QUIET_REAL_TIME)
        == _binding.lib.HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME
    )


def test_session_state_matches_compiled_c_constants() -> None:
    expected = {
        SessionState.DISCONNECTED: "HIL_TRANSPORT_SESSION_STATE_DISCONNECTED",
        SessionState.CONNECTING: "HIL_TRANSPORT_SESSION_STATE_CONNECTING",
        SessionState.ESTABLISHED: "HIL_TRANSPORT_SESSION_STATE_ESTABLISHED",
        SessionState.RECOVERING: "HIL_TRANSPORT_SESSION_STATE_RECOVERING",
        SessionState.FAULT: "HIL_TRANSPORT_SESSION_STATE_FAULT",
    }
    for member, native_name in expected.items():
        assert int(member) == getattr(_binding.lib, native_name)


def test_failure_matches_compiled_c_constants() -> None:
    expected = {
        Failure.NONE: "HIL_TRANSPORT_FAILURE_NONE",
        Failure.LINK_LOST: "HIL_TRANSPORT_FAILURE_LINK_LOST",
        Failure.CONNECTION_TIMEOUT: "HIL_TRANSPORT_FAILURE_CONNECTION_TIMEOUT",
        Failure.DELIVERY: "HIL_TRANSPORT_FAILURE_DELIVERY",
        Failure.PROTOCOL: "HIL_TRANSPORT_FAILURE_PROTOCOL",
        Failure.CAPACITY: "HIL_TRANSPORT_FAILURE_CAPACITY",
        Failure.LOCAL_RESET: "HIL_TRANSPORT_FAILURE_LOCAL_RESET",
        Failure.INTERNAL: "HIL_TRANSPORT_FAILURE_INTERNAL",
    }
    for member, native_name in expected.items():
        assert int(member) == getattr(_binding.lib, native_name)


def test_event_type_matches_compiled_c_constants() -> None:
    expected = {
        EventType.NONE: "HIL_TRANSPORT_EVENT_NONE",
        EventType.SESSION_ESTABLISHED: "HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED",
        EventType.SESSION_RESET: "HIL_TRANSPORT_EVENT_SESSION_RESET",
        EventType.DELIVERY_CONFIRMED: "HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED",
        EventType.DELIVERY_FAILED: "HIL_TRANSPORT_EVENT_DELIVERY_FAILED",
        EventType.PROTOCOL_ERROR: "HIL_TRANSPORT_EVENT_PROTOCOL_ERROR",
        EventType.CAPACITY_EXHAUSTED: "HIL_TRANSPORT_EVENT_CAPACITY_EXHAUSTED",
        EventType.LINK_STATE_CHANGED: "HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED",
    }
    for member, native_name in expected.items():
        assert int(member) == getattr(_binding.lib, native_name)


@pytest.mark.parametrize(
    ("enum_type", "expected_names"),
    [
        (
            TransportStatus,
            [
                "OK",
                "INVALID_ARGUMENT",
                "BUFFER_TOO_SMALL",
                "UNSUPPORTED_CONFIGURATION",
                "MESSAGE_TOO_LARGE",
                "CAPACITY_EXHAUSTED",
                "DELIVERY_FAILED",
                "TIMEOUT",
                "NOT_READY",
                "NOT_IMPLEMENTED",
                "INTERNAL_ERROR",
            ],
        ),
        (Role, ["HOST", "RIG"]),
        (LinkState, ["DISCONNECTED", "CONNECTED"]),
        (OperatingMode, ["NORMAL", "BULK_TRANSFER", "QUIET_REAL_TIME"]),
        (SessionState, ["DISCONNECTED", "CONNECTING", "ESTABLISHED", "RECOVERING", "FAULT"]),
        (
            Failure,
            [
                "NONE",
                "LINK_LOST",
                "CONNECTION_TIMEOUT",
                "DELIVERY",
                "PROTOCOL",
                "CAPACITY",
                "LOCAL_RESET",
                "INTERNAL",
            ],
        ),
        (
            EventType,
            [
                "NONE",
                "SESSION_ESTABLISHED",
                "SESSION_RESET",
                "DELIVERY_CONFIRMED",
                "DELIVERY_FAILED",
                "PROTOCOL_ERROR",
                "CAPACITY_EXHAUSTED",
                "LINK_STATE_CHANGED",
            ],
        ),
    ],
)
def test_enum_member_sets_are_exact(enum_type: type, expected_names: list[str]) -> None:
    assert list(enum_type.__members__) == expected_names


def test_transport_config_defaults_match_native_defaults_where_role_independent() -> None:
    python_config = TransportConfig()
    native_config = _binding.ffi.new("HIL_Transport_Config_T *")
    _binding.lib.HIL_PY_TRANSPORT_Default_Config(native_config)

    assert python_config.max_application_message_size == native_config.max_application_message_size
    assert python_config.max_encoded_frame_size == native_config.max_encoded_frame_size
    assert python_config.initial_reliable_sequence == native_config.initial_reliable_sequence
    assert python_config.connection_timeout_ms == native_config.connection_timeout_ms
    assert python_config.retransmit_timeout_ms == native_config.retransmit_timeout_ms
    assert python_config.max_retries == native_config.max_retries
    assert python_config.session_seed is None
    assert native_config.session_seed == 0


@pytest.mark.parametrize(
    ("field", "minimum", "maximum"),
    [
        ("max_application_message_size", 1, (1 << (8 * _binding.ffi.sizeof("size_t"))) - 1),
        ("max_encoded_frame_size", 1, (1 << (8 * _binding.ffi.sizeof("size_t"))) - 1),
        ("session_seed", 0, (1 << 64) - 1),
        ("initial_reliable_sequence", 0, (1 << 16) - 1),
        ("connection_timeout_ms", 0, (1 << 32) - 1),
        ("retransmit_timeout_ms", 0, (1 << 32) - 1),
        ("max_retries", 0, (1 << 8) - 1),
    ],
)
def test_integer_configuration_boundaries(field: str, minimum: int, maximum: int) -> None:
    for value in (minimum, maximum):
        config = TransportConfig(**{field: value})
        assert getattr(config, field) == value


@pytest.mark.parametrize(
    ("field", "invalid_values"),
    [
        (
            "max_application_message_size",
            [0, -1, 1 << (8 * _binding.ffi.sizeof("size_t"))],
        ),
        ("max_encoded_frame_size", [0, -1, 1 << (8 * _binding.ffi.sizeof("size_t"))]),
        ("session_seed", [-1, 1 << 64]),
        ("initial_reliable_sequence", [-1, 1 << 16]),
        ("connection_timeout_ms", [-1, 1 << 32]),
        ("retransmit_timeout_ms", [-1, 1 << 32]),
        ("max_retries", [-1, 1 << 8]),
    ],
)
def test_out_of_range_configuration_values_raise_value_error(
    field: str, invalid_values: list[int]
) -> None:
    for value in invalid_values:
        with pytest.raises(ValueError):
            TransportConfig(**{field: value})


@pytest.mark.parametrize(
    "field",
    [
        "max_application_message_size",
        "max_encoded_frame_size",
        "session_seed",
        "initial_reliable_sequence",
        "connection_timeout_ms",
        "retransmit_timeout_ms",
        "max_retries",
    ],
)
@pytest.mark.parametrize("value", [True, False, 1.0, "1", object()])
def test_configuration_rejects_implicit_numeric_coercion(field: str, value: object) -> None:
    with pytest.raises(TypeError):
        TransportConfig(**{field: value})


def test_session_seed_none_is_valid_unresolved_configuration() -> None:
    assert TransportConfig(session_seed=None).session_seed is None


def test_public_dataclasses_are_frozen_and_slotted() -> None:
    values = [
        TransportConfig(),
        ReceiveResult(TransportStatus.OK, 0),
        TransportEvent(EventType.NONE, TransportStatus.OK, Failure.NONE, 0),
        TransportSnapshot(
            Role.HOST,
            LinkState.DISCONNECTED,
            SessionState.DISCONNECTED,
            None,
            False,
            False,
            False,
            False,
            Failure.NONE,
        ),
    ]

    for value in values:
        assert not hasattr(value, "__dict__")
        first_field = value.__dataclass_fields__[next(iter(value.__dataclass_fields__))]
        with pytest.raises(FrozenInstanceError):
            setattr(value, first_field.name, getattr(value, first_field.name))
