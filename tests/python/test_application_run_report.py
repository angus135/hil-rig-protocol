"""Run-report wire tables, terminal relationships, full-width integers and ownership."""

from __future__ import annotations

import gc
import struct
from dataclasses import fields, replace
from typing import Any

import hil_rig_protocol as p
import pytest
from hil_rig_protocol import _binding, application


@pytest.fixture
def codec() -> p.ApplicationCodec:
    return p.ApplicationCodec(p.ApplicationConfig())


def success(extension: bytes = b"") -> p.RunReport:
    return p.RunReport(
        p.TestId(bytes(range(16))),
        p.RunOutcome.SUCCESS,
        p.ExecutionOutcome.COMPLETE,
        p.RunResultStatus.COMPLETE,
        100,
        1000,
        valid_sections=0x3F,
        last_completed_boundary=100,
        result_ticks_emitted=100,
        isr_timing=p.RunIsrTiming(1, 0xFEDCBA9876543210, 3, 4, 0),
        instruction_buffer=p.RunInstructionBuffer(2, 4, 100),
        result_buffer=p.RunResultBuffer(1, 2, 3, 100, 5, 6),
        flash=p.RunFlashStatistics(
            1,
            0xFFFFFFFFFFFFFFFF,
            0x8000000000000000,
            4,
            5,
            0xFEDCBA9876543210,
            0xFFFFFFFFFFFFFFFF,
            8,
            9,
            0x8000000000000000,
            11,
            12,
            0xFEDCBA9876543210,
            14,
            15,
        ),
        extension_data=extension,
    )


def preparation_failure() -> p.RunReport:
    return p.RunReport(
        p.TestId(bytes(16)),
        p.RunOutcome.FAILED,
        p.ExecutionOutcome.NOT_STARTED,
        p.RunResultStatus.UNAVAILABLE,
        100,
        1000,
        failure_source=p.FailureSource.DRIVER_LIFECYCLE,
        failure_stage=p.FailureStage.PREPARATION,
        failure_reason=p.FailureReason.DRIVER_START_FAILED,
    )


def rejected_start() -> p.RunReport:
    return replace(
        preparation_failure(),
        run_outcome=p.RunOutcome.REJECTED,
        failure_source=p.FailureSource.EXECUTION_MANAGER,
        failure_reason=p.FailureReason.INVALID_LIFECYCLE_STATE,
    )


def test_independent_reference_layout(codec: p.ApplicationCodec) -> None:
    value = success(b"\x00\xffv1")
    body = (
        struct.pack("<H5BxH2x5I", 1, 1, 2, 1, 0, 0, 0, 0x3F, 100, 1000, 100, 100)
        + struct.pack("<IQIII", 1, 0xFEDCBA9876543210, 3, 4, 0)
        + struct.pack("<III", 2, 4, 100)
        + struct.pack("<6I", 1, 2, 3, 100, 5, 6)
        + struct.pack(
            "<IQQIIQQIIQIIQII",
            1,
            0xFFFFFFFFFFFFFFFF,
            0x8000000000000000,
            4,
            5,
            0xFEDCBA9876543210,
            0xFFFFFFFFFFFFFFFF,
            8,
            9,
            0x8000000000000000,
            11,
            12,
            0xFEDCBA9876543210,
            14,
            15,
        )
        + b"\x04\x00\xffv1"
    )
    expected = (
        bytes([p.PROTOCOL_VERSION.major, p.PROTOCOL_VERSION.minor, 1])
        + bytes(range(16))
        + bytes([35, 0])
        + len(body).to_bytes(2, "little")
        + body
    )
    assert len(expected) == 204
    assert codec.encode(value) == expected
    assert codec.decode(expected) == value


@pytest.mark.parametrize("length", [0, 1, 255])
def test_extension_sizes_and_detached_copy(codec: p.ApplicationCodec, length: int) -> None:
    value = success(bytes(range(length)))
    wire = bytearray(codec.encode(value))
    assert len(wire) == 200 + length
    decoded = codec.decode(wire)
    wire[:] = bytes(len(wire))
    gc.collect()
    assert decoded == value
    with pytest.raises(ValueError):
        replace(value, extension_data=bytes(256))
    with pytest.raises(TypeError):
        replace(value, extension_data=bytearray(b"x"))


def test_capacity_and_variable_policy() -> None:
    value = success(bytes(255))
    exact = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=455))
    assert exact.decode(exact.encode(value)) == value
    smaller = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=454))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        smaller.encode(value)
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL
    firmware = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=2302))
    assert firmware.decode(firmware.encode(value)) == value
    restricted = p.ApplicationCodec(p.ApplicationConfig(max_variable_data_size=254))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        restricted.encode(value)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize("boundary", [0, 100])
def test_first_and_last_executed_boundary(codec: p.ApplicationCodec, boundary: int) -> None:
    value = success()
    value = replace(value, isr_timing=replace(value.isr_timing, maximum_boundary=boundary))
    assert codec.decode(codec.encode(value)) == value


@pytest.mark.parametrize(
    "ticks,status",
    [
        (1, p.RunResultStatus.PARTIAL),
        (99, p.RunResultStatus.PARTIAL),
        (0, p.RunResultStatus.UNAVAILABLE),
        (100, p.RunResultStatus.UNAVAILABLE),
    ],
)
def test_aborted_partial_and_unavailable_streams(
    codec: p.ApplicationCodec, ticks: int, status: p.RunResultStatus
) -> None:
    value = replace(
        preparation_failure(),
        run_outcome=p.RunOutcome.ABORTED,
        execution_outcome=p.ExecutionOutcome.ABORTED,
        result_status=status,
        result_ticks_emitted=ticks,
        failure_source=p.FailureSource.HOST_INTERFACE,
        failure_stage=p.FailureStage.EXECUTION,
        failure_reason=p.FailureReason.HOST_ABORT,
    )
    assert codec.decode(codec.encode(value)) == value


def test_not_started_and_failed_transfer(codec: p.ApplicationCodec) -> None:
    value = preparation_failure()
    assert codec.decode(codec.encode(value)) == value
    transfer_failure = replace(
        value,
        execution_outcome=p.ExecutionOutcome.COMPLETE,
        result_ticks_emitted=100,
        failure_source=p.FailureSource.HOST_INTERFACE,
        failure_stage=p.FailureStage.TRANSFER,
        failure_reason=p.FailureReason.RESULT_TRANSFER_FAILED,
    )
    assert codec.decode(codec.encode(transfer_failure)) == transfer_failure


def test_rejected_start_round_trip(codec: p.ApplicationCodec) -> None:
    value = rejected_start()
    wire = codec.encode(value)
    assert wire[25] == p.RunOutcome.REJECTED
    assert codec.decode(wire) == value


@pytest.mark.parametrize(
    "changes",
    [
        {"execution_outcome": p.ExecutionOutcome.FAILED},
        {"result_status": p.RunResultStatus.PARTIAL},
        {"valid_sections": 3},
        {"result_ticks_emitted": 1},
        {"failure_reason": p.FailureReason.NONE},
        {"failure_source": p.FailureSource.NONE},
        {"failure_stage": p.FailureStage.NONE},
    ],
)
def test_rejected_start_relationships(
    codec: p.ApplicationCodec, changes: dict[str, object]
) -> None:
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(rejected_start(), **changes))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize(
    "changes",
    [
        {"run_outcome": p.RunOutcome.INVALID},
        {"execution_outcome": p.ExecutionOutcome.INVALID},
        {"result_status": p.RunResultStatus.INVALID},
        {"valid_sections": 0},
        {"valid_sections": 0x7F},
        {"expected_tick_count": 0},
        {"expected_tick_count": 1000001},
        {"tick_period_us": 1},
        {"last_completed_boundary": 99},
        {"last_completed_boundary": 101},
        {"result_ticks_emitted": 101},
        {"result_ticks_emitted": 99},
        {"run_outcome": p.RunOutcome.FAILED},
        {"failure_reason": p.FailureReason.HOST_ABORT},
        {"valid_sections": 0x3D},
        {"valid_sections": 0x3B},
        {"valid_sections": 0x37},
        {"valid_sections": 0x2F},
        {"valid_sections": 0x1F},
    ],
)
def test_illegal_terminal_combinations(
    codec: p.ApplicationCodec, changes: dict[str, object]
) -> None:
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(success(), **changes))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize(
    "changes",
    [
        {"valid_sections": 3},
        {"valid_sections": 5},
        {"result_ticks_emitted": 1},
        {"failure_reason": p.FailureReason.NONE},
        {"failure_source": p.FailureSource.NONE},
        {"failure_stage": p.FailureStage.NONE},
        {"result_status": p.RunResultStatus.PARTIAL},
    ],
)
def test_not_started_relationships(codec: p.ApplicationCodec, changes: dict[str, object]) -> None:
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(preparation_failure(), **changes))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize(
    "section,changes",
    [
        ("isr_timing", {"sample_count": 0}),
        ("instruction_buffer", {"sample_count": 0}),
        ("flash", {"result_pages_drained": 0}),
        ("flash", {"instruction_pages_refilled": 0}),
        ("flash", {"instruction_publish_sample_count": 0}),
        ("flash", {"service_gap_sample_count": 0}),
        ("isr_timing", {"maximum_boundary": 101}),
        ("instruction_buffer", {"minimum_boundary": 101}),
        ("result_buffer", {"peak_pending_boundary": 101}),
    ],
)
def test_sampling_rules(codec: p.ApplicationCodec, section: str, changes: dict[str, int]) -> None:
    value = success()
    invalid = replace(value, **{section: replace(getattr(value, section), **changes)})
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(invalid)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_malformed_wire_schema_and_test_id(codec: p.ApplicationCodec) -> None:
    wire = codec.encode(success(b"x"))
    for length in range(len(wire)):
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(wire[:length])
        assert caught.value.status is p.ApplicationStatus.TRUNCATED_MESSAGE
    for offset in (25, 26, 27, 28, 29, 30, 31, 33, 34, 35):
        invalid = bytearray(wire)
        invalid[offset] = 0xFF
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(invalid)
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED
    invalid = bytearray(wire)
    invalid[199] = 2
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(invalid)
    assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE
    invalid = bytearray(wire)
    invalid[23] = 2
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(invalid)
    assert caught.value.status is p.ApplicationStatus.UNSUPPORTED_MESSAGE
    invalid = bytearray(wire)
    invalid[2:19] = bytes(17)
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(invalid)
    assert caught.value.status is p.ApplicationStatus.INCONSISTENT_TEST_ID


def test_exact_representation_and_u64_parity() -> None:
    for record in (
        p.RunIsrTiming(),
        p.RunInstructionBuffer(),
        p.RunResultBuffer(),
        p.RunFlashStatistics(),
    ):
        for field in fields(record):
            with pytest.raises(TypeError):
                replace(record, **{field.name: True})
            with pytest.raises(ValueError):
                replace(record, **{field.name: -1})
    value = success()
    for name in (
        "test_id",
        "run_outcome",
        "execution_outcome",
        "result_status",
        "failure_source",
        "failure_stage",
        "failure_reason",
        "isr_timing",
        "instruction_buffer",
        "result_buffer",
        "flash",
    ):
        with pytest.raises(TypeError):
            replace(value, **{name: 0})
    with pytest.raises(ValueError):
        p.RunIsrTiming(total_cycles=1 << 64)
    ffi, lib = _binding.ffi, _binding.lib
    for public, native in (
        (p.RunIsrTiming, "Run_Isr_Timing"),
        (p.RunInstructionBuffer, "Run_Instruction_Buffer"),
        (p.RunResultBuffer, "Run_Result_Buffer"),
        (p.RunFlashStatistics, "Run_Flash_Statistics"),
    ):
        native_fields = dict(ffi.typeof(f"HIL_Application_{native}_T").fields)
        assert set(native_fields) == {f.name for f in fields(public)}
        for field in fields(public):
            expected = ffi.typeof(
                "uint64_t"
                if field.name
                in {
                    "total_cycles",
                    "result_bytes_drained",
                    "result_drain_total_cycles",
                    "instruction_bytes_refilled",
                    "instruction_refill_total_cycles",
                    "instruction_publish_total_cycles",
                    "service_gap_total_cycles",
                }
                else "uint32_t"
            )
            assert native_fields[field.name].type == expected
    for enum, prefix in (
        (p.RunOutcome, "RUN_OUTCOME"),
        (p.ExecutionOutcome, "EXECUTION_OUTCOME"),
        (p.RunResultStatus, "RUN_RESULT_STATUS"),
        (p.RunReportSection, "RUN_REPORT_VALID"),
    ):
        for member in enum:
            assert getattr(lib, "HIL_APPLICATION_" + prefix + "_" + member.name) == member.value
    assert lib.HIL_APPLICATION_MESSAGE_TYPE_RUN_REPORT == 35


def test_extension_owner_survives_native_encode(
    codec: p.ApplicationCodec, monkeypatch: Any
) -> None:
    native_encode = application._native_encode

    def collect_then_encode(*args: Any) -> Any:
        gc.collect()
        return native_encode(*args)

    monkeypatch.setattr(application, "_native_encode", collect_then_encode)
    value = success(bytes(range(255)))
    assert codec.decode(codec.encode(value)) == value
