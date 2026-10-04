"""Public arbitrary containers use the native codec and own decoded bytes."""

from __future__ import annotations

import gc
from dataclasses import replace
from typing import Any

import hil_rig_protocol as p
import pytest
from hil_rig_protocol import _binding, application


@pytest.fixture
def codec() -> p.ApplicationCodec:
    return p.ApplicationCodec(p.ApplicationConfig())


def test_control_reference_bytes_and_optional_id(codec: p.ApplicationCodec) -> None:
    value = p.ArbitraryControl(0x01020304, 0xA1B2C3D4)
    wire = codec.encode(value)
    assert wire == bytes(
        [0, p.PROTOCOL_VERSION.minor, 0]
        + [0] * 16
        + [64, 0, 8, 0, 4, 3, 2, 1, 0xD4, 0xC3, 0xB2, 0xA1]
    )
    assert codec.decode(wire) == value
    contextual = replace(value, test_id=p.TestId(bytes(range(16))))
    assert codec.decode(codec.encode(contextual)) == contextual
    assert codec.decode(codec.encode(p.ArbitraryControl(0, 0xFFFFFFFF))) == p.ArbitraryControl(
        0, 0xFFFFFFFF
    )


@pytest.mark.parametrize("size", [0, 1, 255, 256, 4067])
def test_binary_data_round_trip_and_detached_copy(codec: p.ApplicationCodec, size: int) -> None:
    payload = bytes(i % 256 for i in range(size))
    value = p.ArbitraryData(1234, payload)
    wire = bytearray(codec.encode(value))
    assert len(wire) == 29 + size
    assert wire[27:29] == size.to_bytes(2, "little")
    assert codec.decode(wire) == value
    decoded = codec.decode(wire)
    wire[-1] ^= 0xFF
    assert decoded == value
    contextual = replace(value, test_id=p.TestId(bytes(range(16))))
    assert codec.decode(codec.encode(contextual)) == contextual


@pytest.mark.parametrize(
    "factory,field,bad",
    [
        (p.ArbitraryControl, "control_id", True),
        (p.ArbitraryControl, "control_id", -1),
        (p.ArbitraryControl, "value", 1 << 32),
        (p.ArbitraryData, "data_id", True),
        (p.ArbitraryData, "data_id", 1 << 32),
    ],
)
def test_exact_u32_representation(factory: type, field: str, bad: object) -> None:
    value = p.ArbitraryControl(0, 0) if factory is p.ArbitraryControl else p.ArbitraryData(0, b"")
    with pytest.raises((TypeError, ValueError)):
        replace(value, **{field: bad})


def test_payload_exact_type_and_message_capacity(codec: p.ApplicationCodec) -> None:
    for bad in (bytearray(b"x"), memoryview(b"x"), "x"):
        with pytest.raises(TypeError):
            p.ArbitraryData(1, bad)
    with pytest.raises(ValueError):
        p.ArbitraryData(1, bytes(65536))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(p.ArbitraryData(1, bytes(4068)))
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL


def test_absolute_complete_message_boundary() -> None:
    codec = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=65535))
    value = p.ArbitraryData(0xFFFFFFFF, bytes(65506))
    wire = codec.encode(value)
    assert len(wire) == 65535
    assert codec.decode(wire) == value
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(p.ArbitraryData(1, bytes(65507)))
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL


def test_malformed_internal_length_and_native_field_parity(codec: p.ApplicationCodec) -> None:
    ffi, lib = _binding.ffi, _binding.lib
    assert lib.HIL_APPLICATION_MESSAGE_TYPE_ARBITRARY_CONTROL == 64
    assert lib.HIL_APPLICATION_MESSAGE_TYPE_ARBITRARY_DATA == 65
    assert ffi.sizeof("HIL_Application_Arbitrary_Control_T") == 8
    assert ffi.typeof("HIL_Application_Arbitrary_Data_T").fields[0][0] == "data_id"
    wire = bytearray(codec.encode(p.ArbitraryData(1, b"\x00\x01\xff")))
    wire[27] = 4
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE


@pytest.mark.parametrize("value", [p.ArbitraryControl(0, 0), p.ArbitraryData(0, b"\x00\xff")])
def test_all_truncated_prefixes_and_extra_bytes(
    codec: p.ApplicationCodec, value: p.ApplicationMessage
) -> None:
    wire = codec.encode(value)
    for size in range(len(wire)):
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(wire[:size])
        assert caught.value.status is p.ApplicationStatus.TRUNCATED_MESSAGE
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire + b"\x00")
    assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE


def test_firmware_capacity_and_variable_policy() -> None:
    codec = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=2302))
    value = p.ArbitraryData(1, bytes(2273))
    assert codec.decode(codec.encode(value)) == value
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(p.ArbitraryData(1, bytes(2274)))
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL
    restrictive = p.ApplicationCodec(p.ApplicationConfig(max_variable_data_size=1))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        restrictive.encode(p.ArbitraryData(1, b"xx"))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_encode_span_owner_survives_native_call(
    codec: p.ApplicationCodec, monkeypatch: Any
) -> None:
    original = application._native_encode

    def collect_then_encode(*args: Any) -> Any:
        gc.collect()
        return original(*args)

    monkeypatch.setattr(application, "_native_encode", collect_then_encode)
    value = p.ArbitraryData(1234, bytes(range(256)))
    assert codec.decode(codec.encode(value)) == value
