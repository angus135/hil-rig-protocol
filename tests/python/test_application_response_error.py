"""Application Response and Error public binding coverage."""

from __future__ import annotations

import gc
from dataclasses import FrozenInstanceError

import hil_rig_protocol as p
import pytest
from hil_rig_protocol import _binding


@pytest.fixture
def codec() -> p.ApplicationCodec:
    return p.ApplicationCodec(p.ApplicationConfig())


def test_response_exact_golden_and_every_scope(codec: p.ApplicationCodec) -> None:
    test_id = p.TestId(bytes(range(0x10, 0x20)))
    response = p.ApplicationResponse(
        test_id=test_id,
        scope=p.ResponseScope.TICK,
        outcome=p.ResponseOutcome.FAILED,
        reason=p.ResponseReason.INTERNAL_FAILURE,
        tick_number=0x12345678,
        control_command=p.ControlCommand.ABORT,
        global_control_command=p.GlobalControlCommand.INVALID,
        detail=0x89ABCDEF,
    )
    expected = bytes.fromhex(
        "000301101112131415161718191a1b1c1d1e1f30000d00020409785634120200efcdab89"
    )
    assert codec.encode(response) == expected
    assert codec.decode(expected) == response

    for scope in (
        p.ResponseScope.TEST_CONFIGURATION,
        p.ResponseScope.TICK,
        p.ResponseScope.COMPLETE_TEST,
        p.ResponseScope.EXECUTION_CONTROL,
        p.ResponseScope.GLOBAL_CONTROL,
    ):
        value = p.ApplicationResponse(
            test_id=None if scope is p.ResponseScope.GLOBAL_CONTROL else test_id,
            scope=scope,
            outcome=p.ResponseOutcome.ACCEPTED,
            reason=p.ResponseReason.NONE,
        )
        assert codec.decode(codec.encode(value)) == value


def test_response_accepts_all_defined_values_without_semantic_matrix(
    codec: p.ApplicationCodec,
) -> None:
    test_id = p.TestId(bytes(16))
    for outcome in (
        p.ResponseOutcome.ACCEPTED,
        p.ResponseOutcome.REJECTED,
        p.ResponseOutcome.COMPLETED,
        p.ResponseOutcome.FAILED,
    ):
        for reason in p.ResponseReason:
            if reason is p.ResponseReason.RESERVED:
                continue
            unusual = p.ApplicationResponse(
                test_id=test_id,
                scope=p.ResponseScope.TEST_CONFIGURATION,
                outcome=outcome,
                reason=reason,
                tick_number=0xFFFFFFFF,
                control_command=p.ControlCommand.START,
                global_control_command=p.GlobalControlCommand.RESET_APPLICATION,
                detail=0xFFFFFFFF,
            )
            assert codec.decode(codec.encode(unusual)) == unusual


def test_response_test_id_rules_and_reserved_enums(codec: p.ApplicationCodec) -> None:
    test_id = p.TestId(bytes(16))
    for value in (
        p.ApplicationResponse(
            None, p.ResponseScope.TICK, p.ResponseOutcome.ACCEPTED, p.ResponseReason.NONE
        ),
        p.ApplicationResponse(
            test_id,
            p.ResponseScope.GLOBAL_CONTROL,
            p.ResponseOutcome.COMPLETED,
            p.ResponseReason.NONE,
        ),
        p.ApplicationResponse(
            test_id, p.ResponseScope.RESERVED, p.ResponseOutcome.ACCEPTED, p.ResponseReason.NONE
        ),
        p.ApplicationResponse(
            test_id, p.ResponseScope.TICK, p.ResponseOutcome.RESERVED, p.ResponseReason.NONE
        ),
        p.ApplicationResponse(
            test_id, p.ResponseScope.TICK, p.ResponseOutcome.ACCEPTED, p.ResponseReason.RESERVED
        ),
    ):
        with pytest.raises(p.ApplicationEncodeError):
            codec.encode(value)


def test_error_exact_golden_and_three_structural_forms(codec: p.ApplicationCodec) -> None:
    test_id = p.TestId(bytes(range(0x10, 0x20)))
    global_error = p.ApplicationErrorMessage(
        test_id=None,
        category=p.ErrorCategory.PROTOCOL,
        recoverable=True,
        detail=0x01020304,
        diagnostic_data=b"\xde\xad\xbe\xef",
    )
    expected = bytes.fromhex(
        "0003000000000000000000000000000000000031001000050100000000000403020104deadbeef"
    )
    assert codec.encode(global_error) == expected
    assert codec.decode(expected) == global_error

    for value in (
        global_error,
        p.ApplicationErrorMessage(test_id, p.ErrorCategory.TIMEOUT, False),
        p.ApplicationErrorMessage(test_id, p.ErrorCategory.EXECUTION, True, tick_number=7),
    ):
        assert codec.decode(codec.encode(value)) == value


@pytest.mark.parametrize("size", [0, 7, 255])
def test_error_diagnostic_boundaries_and_python_ownership(
    codec: p.ApplicationCodec, size: int
) -> None:
    diagnostic = bytes(range(size))
    value = p.ApplicationErrorMessage(
        p.TestId(bytes(16)),
        p.ErrorCategory.INTERNAL,
        True,
        tick_number=4,
        diagnostic_data=diagnostic,
    )
    mutable_wire = bytearray(codec.encode(value))
    decoded = codec.decode(mutable_wire)
    del mutable_wire
    gc.collect()
    assert decoded == value
    assert type(decoded.diagnostic_data) is bytes
    assert decoded.diagnostic_data == diagnostic


def test_error_rejects_tick_without_test_id_and_policy_overflow(
    codec: p.ApplicationCodec,
) -> None:
    tick_without_id = p.ApplicationErrorMessage(
        None, p.ErrorCategory.EXECUTION, True, tick_number=1
    )
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(tick_without_id)
    assert caught.value.status is p.ApplicationStatus.INCONSISTENT_TEST_ID

    restricted = p.ApplicationCodec(p.ApplicationConfig(max_variable_data_size=3))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        restricted.encode(
            p.ApplicationErrorMessage(
                None, p.ErrorCategory.HARDWARE, False, diagnostic_data=b"1234"
            )
        )
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_types_are_immutable_and_wire_error_name_does_not_shadow_exception() -> None:
    response = p.ApplicationResponse(
        None,
        p.ResponseScope.GLOBAL_CONTROL,
        p.ResponseOutcome.COMPLETED,
        p.ResponseReason.NONE,
    )
    error = p.ApplicationErrorMessage(None, p.ErrorCategory.HARDWARE, False)
    with pytest.raises(FrozenInstanceError):
        response.detail = 1
    with pytest.raises(FrozenInstanceError):
        error.detail = 1
    assert issubclass(p.ApplicationError, Exception)
    assert p.ApplicationErrorMessage is not p.ApplicationError


def test_cffi_union_and_enum_members_are_publicly_available() -> None:
    body_fields = dict(_binding.ffi.typeof("HIL_Application_Message_T").fields)["body"].type.fields
    assert {name for name, _ in body_fields} >= {"response", "error"}
    assert _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_RESPONSE == 48
    assert _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_ERROR == 49
