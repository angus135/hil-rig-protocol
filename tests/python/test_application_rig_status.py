"""Status snapshots, stable provenance and GET_STATUS use the native wire contract."""

from __future__ import annotations

from dataclasses import replace

import hil_rig_protocol as p
import pytest
from hil_rig_protocol import _binding


@pytest.fixture
def codec() -> p.ApplicationCodec:
    return p.ApplicationCodec(p.ApplicationConfig())


def ready() -> p.RigStatus:
    return p.RigStatus(p.StatusOrigin.QUERY_RESPONSE, p.RigState.IDLE, flags=5)


def test_reference_layout_and_query(codec: p.ApplicationCodec) -> None:
    value = ready()
    wire = codec.encode(value)
    assert wire == bytes(
        [p.PROTOCOL_VERSION.major, p.PROTOCOL_VERSION.minor, 0]
        + [0] * 16
        + [50, 0, 12, 0, 1, 0, 1, 2, 5, 0, 0, 0, 0, 0, 0, 0]
    )
    assert codec.decode(wire) == value
    notification = replace(value, origin=p.StatusOrigin.NOTIFICATION)
    assert codec.decode(codec.encode(notification)) == notification
    query = p.GlobalControl(p.GlobalControlCommand.GET_STATUS)
    query_wire = codec.encode(query)
    assert len(query_wire) == 28
    assert query_wire[23:] == b"\x02\0\0\0\0"
    assert codec.decode(query_wire) == query
    rejection = p.ApplicationResponse(
        test_id=None,
        scope=p.ResponseScope.GLOBAL_CONTROL,
        outcome=p.ResponseOutcome.REJECTED,
        reason=p.ResponseReason.HARDWARE_NOT_READY,
        global_control_command=p.GlobalControlCommand.GET_STATUS,
    )
    assert codec.decode(codec.encode(rejection)) == rejection


@pytest.mark.parametrize("state", list(p.RigState))
def test_state_values_and_optional_active_id(codec: p.ApplicationCodec, state: p.RigState) -> None:
    value = p.RigStatus(p.StatusOrigin.NOTIFICATION, state, test_id=p.TestId(bytes(range(16))))
    if state is p.RigState.INVALID:
        with pytest.raises(p.ApplicationEncodeError) as caught:
            codec.encode(value)
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED
    else:
        assert codec.decode(codec.encode(value)) == value


@pytest.mark.parametrize("reason", list(p.FailureReason))
def test_all_defined_failure_reasons(codec: p.ApplicationCodec, reason: p.FailureReason) -> None:
    value = p.RigStatus(
        p.StatusOrigin.QUERY_RESPONSE,
        p.RigState.RECOVERING,
        failure_reason=reason,
        failure_source=p.FailureSource.NONE
        if reason is p.FailureReason.NONE
        else p.FailureSource.HOST_INTERFACE,
        failure_stage=p.FailureStage.NONE
        if reason is p.FailureReason.NONE
        else p.FailureStage.CLEANUP,
    )
    assert codec.decode(codec.encode(value)) == value


@pytest.mark.parametrize(
    "changes",
    [
        {"flags": 1},
        {"flags": 7},
        {"flags": 13},
        {"flags": 16},
        {"state": p.RigState.RUNNING},
        {"test_id": p.TestId(bytes(16))},
        {"failure_source": p.FailureSource.HOST_INTERFACE},
        {"failure_stage": p.FailureStage.CLEANUP},
        {"failure_reason": p.FailureReason.HOST_ABORT},
    ],
)
def test_ready_invariants(codec: p.ApplicationCodec, changes: dict[str, object]) -> None:
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(ready(), **changes))
    assert caught.value.status in (
        p.ApplicationStatus.VALIDATION_FAILED,
        p.ApplicationStatus.INCONSISTENT_TEST_ID,
    )


@pytest.mark.parametrize("offset", [25, 26, 27, 31, 32, 33])
def test_unknown_wire_values(codec: p.ApplicationCodec, offset: int) -> None:
    wire = bytearray(codec.encode(ready()))
    wire[offset] = 0xFF
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_schema_truncation_and_reserved_flags(codec: p.ApplicationCodec) -> None:
    wire = bytearray(codec.encode(ready()))
    for length in range(len(wire)):
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(wire[:length])
        assert caught.value.status is p.ApplicationStatus.TRUNCATED_MESSAGE
    wire[23] = 2
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is p.ApplicationStatus.UNSUPPORTED_MESSAGE
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(ready(), schema_version=2))
    assert caught.value.status is p.ApplicationStatus.UNSUPPORTED_MESSAGE
    with pytest.raises(p.ApplicationEncodeError):
        codec.encode(p.GlobalControl(p.GlobalControlCommand.GET_STATUS, flags=1))


@pytest.mark.parametrize(
    "field,value",
    [
        ("origin", 1),
        ("state", 2),
        ("flags", True),
        ("flags", 1 << 32),
        ("schema_version", True),
        ("schema_version", 1 << 16),
        ("test_id", bytes(16)),
        ("failure_source", 0),
        ("failure_stage", 0),
        ("failure_reason", 0),
    ],
)
def test_exact_representation(field: str, value: object) -> None:
    with pytest.raises((TypeError, ValueError)):
        replace(ready(), **{field: value})


def test_cffi_enum_and_struct_parity() -> None:
    ffi, lib = _binding.ffi, _binding.lib
    for enum, prefix in (
        (p.StatusOrigin, "HIL_APPLICATION_STATUS_ORIGIN_"),
        (p.RigState, "HIL_APPLICATION_RIG_STATE_"),
        (p.RigStatusFlag, "HIL_APPLICATION_RIG_STATUS_"),
        (p.FailureSource, "HIL_APPLICATION_FAILURE_SOURCE_"),
        (p.FailureStage, "HIL_APPLICATION_FAILURE_STAGE_"),
        (p.FailureReason, "HIL_APPLICATION_FAILURE_REASON_"),
    ):
        for value in enum:
            assert getattr(lib, prefix + value.name) == value.value
    fields = dict(ffi.typeof("HIL_Application_Rig_Status_T").fields)
    assert set(fields) == {
        "schema_version",
        "origin",
        "state",
        "flags",
        "failure_source",
        "failure_stage",
        "failure_reason",
    }
    assert fields["schema_version"].type == ffi.typeof("uint16_t")
    assert fields["flags"].type == ffi.typeof("uint32_t")
    assert lib.HIL_APPLICATION_MESSAGE_TYPE_RIG_STATUS == 50
    assert lib.HIL_APPLICATION_GLOBAL_CONTROL_GET_STATUS == 2
