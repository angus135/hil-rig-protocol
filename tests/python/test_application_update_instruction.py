"""Variable-message CFFI parity, representation, storage ownership and malformed input."""

from __future__ import annotations

import gc
import pickle
import struct
from dataclasses import FrozenInstanceError, replace

import hil_rig_protocol as p
import pytest
from hil_rig_protocol import _binding, application

ffi, lib = _binding.ffi, _binding.lib
TEST_ID = p.TestId(bytes(range(16)))


@pytest.fixture(params=[False, True], ids=["update", "result"])
def result_family(request):
    return request.param


@pytest.fixture
def codec():
    return p.ApplicationCodec(p.ApplicationConfig())


def message(result_family, entries=((p.PeripheralType.UART, 0, b"\x00\xff\x80"),), **kwargs):
    if result_family:
        return p.VariableTestResult(
            TEST_ID, records=tuple(p.CapturedRecord(*entry) for entry in entries), **kwargs
        )
    return p.UpdateInstruction(
        TEST_ID, operations=tuple(p.LogicalOperation(*entry) for entry in entries), **kwargs
    )


def golden(result_family, entries, *, tick=0, flags=0, condition=0, detail=0):
    """Independent wire oracle; never call the codec to construct expected bytes."""
    if result_family:
        body = struct.pack("<IBBBBI", tick, len(entries), condition, flags, 0, detail)
    else:
        body = struct.pack("<IBBH", tick, len(entries), flags, 0)
    for peripheral, channel, data in entries:
        body += struct.pack("<BBH", peripheral, channel, len(data)) + data
        body += bytes((-len(data)) % 4)
    return (
        bytes((p.PROTOCOL_VERSION.major, p.PROTOCOL_VERSION.minor, 1))
        + TEST_ID.bytes
        + struct.pack("<BBH", 34 if result_family else 21, 0, len(body))
        + body
    )


def representative(result_family):
    return (
        (p.PeripheralType.DIGITAL_INPUT if result_family else p.PeripheralType.DIGITAL_OUTPUT,
         0, b"\xff\x03"),
        (p.PeripheralType.ANALOG_INPUT if result_family else p.PeripheralType.ANALOG_OUTPUT,
         1, struct.pack("<I", 0x12345678)),
        (p.PeripheralType.PWM_INPUT if result_family else p.PeripheralType.PWM_OUTPUT,
         1, struct.pack("<IH", 0x11223344, 10000)),
        (p.PeripheralType.UART, 0, b"\x00\xff\x80\x01\x02"),
        (p.PeripheralType.SPI, 1, b"\x12\x34\x56" if result_family
         else b"\x02\x01\x02\x12\x34\x56"),
        (p.PeripheralType.CAN, 1, struct.pack("<HB8sB", 0x7ff, 8, bytes(range(8)), 0)),
    )


def test_representative_golden_and_native_facades(codec, result_family):
    entries = representative(result_family)
    value = message(result_family, entries, tick_number=0x20304, flags=1)
    wire = golden(result_family, entries, tick=0x20304, flags=1)
    assert codec.encode(value) == wire
    assert codec.decode(wire) == value
    native, owners = application._build_message(value)
    context = ffi.new("HIL_Application_Context_T *")
    config = ffi.new("HIL_Application_Config_T *")
    assert lib.HIL_APPLICATION_Default_Config(config) == p.ApplicationStatus.OK
    assert lib.HIL_APPLICATION_Init(context, config) == p.ApplicationStatus.OK
    assert lib.HIL_APPLICATION_Validate_Message(context, native) == p.ApplicationStatus.OK
    required = ffi.new("size_t *")
    assert lib.HIL_APPLICATION_Encoded_Size(context, native, required) == p.ApplicationStatus.OK
    assert required[0] == len(wire)
    assert lib.HIL_APPLICATION_Validate_Encoded_Message(
        context, wire, len(wire), required
    ) == p.ApplicationStatus.OK
    ctype = (
        "HIL_Application_Captured_Record_T" if result_family
        else "HIL_Application_Logical_Operation_T"
    )
    alignment = lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT
    descriptors = len(entries) * ffi.sizeof(ctype)
    assert required[0] == ((descriptors + alignment - 1) // alignment) * alignment + sum(
        len(data) for _, _, data in entries
    )
    assert owners  # Descriptor array and every payload remain owned through native calls.


@pytest.mark.parametrize("size", range(1, 256))
def test_every_uart_length_and_padding(codec, result_family, size):
    entries = ((p.PeripheralType.UART, 1, bytes(range(size))),)
    value = message(result_family, entries)
    wire = golden(result_family, entries)
    assert codec.encode(value) == wire
    assert codec.decode(wire) == value


@pytest.mark.parametrize("condition", [p.ResultCondition.OK, p.ResultCondition.PARTIAL,
                                        p.ResultCondition.EXECUTION_PROBLEM])
@pytest.mark.parametrize("flags", [0, 1])
@pytest.mark.parametrize("detail", [0, 0xffffffff])
def test_empty_results(codec, condition, flags, detail):
    value = message(True, (), condition=condition, flags=flags, problem_detail=detail)
    wire = golden(True, (), condition=condition, flags=flags, detail=detail)
    if condition is p.ResultCondition.OK and detail:
        with pytest.raises(p.ApplicationEncodeError) as caught:
            codec.encode(value)
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED
        with pytest.raises(p.ApplicationDecodeError):
            codec.decode(wire)
    else:
        assert codec.encode(value) == wire
        assert codec.decode(wire) == value


def test_chunks_and_stateless_ticks(codec, result_family):
    for tick, flags in [(9, 1), (9, 1), (9, 0), (10, 0), (0, 0)]:
        value = message(result_family, tick_number=tick, flags=flags)
        assert codec.decode(codec.encode(value)) == value


def test_encode_keeps_all_borrowed_owners_alive(codec, result_family, monkeypatch):
    value = message(result_family, representative(result_family))
    size_query = application._native_encoded_size
    encode = application._native_encode

    def collect_before_size(*args):
        gc.collect()
        return size_query(*args)

    def collect_before_encode(*args):
        gc.collect()
        return encode(*args)

    monkeypatch.setattr(application, "_native_encoded_size", collect_before_size)
    monkeypatch.setattr(application, "_native_encode", collect_before_encode)
    assert codec.encode(value) == golden(result_family, representative(result_family))


def test_exact_message_size_limit(result_family):
    value = message(result_family)
    wire = golden(result_family, ((p.PeripheralType.UART, 0, b"\x00\xff\x80"),))
    exact = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=len(wire)))
    assert exact.encode(value) == wire
    assert exact.decode(wire) == value
    short = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=len(wire) - 1))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        short.encode(value)
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL
    with pytest.raises(p.ApplicationDecodeError) as caught:
        short.decode(wire)
    assert caught.value.status is p.ApplicationStatus.INVALID_LENGTH


@pytest.mark.parametrize("field,value", [("flags", 2), ("flags", 255),
                                          ("tick_number", 1000000),
                                          ("tick_number", 0xffffffff)])
def test_native_message_semantics(codec, result_family, field, value):
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(message(result_family, **{field: value}))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_empty_update_rejected(codec):
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(message(False, ()))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize("channel", [2, 255])
def test_native_channel_boundary(codec, result_family, channel):
    value = message(result_family, ((p.PeripheralType.UART, channel, b"x"),))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(value)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_duplicate_and_wrong_direction(codec, result_family):
    entry = (p.PeripheralType.UART, 0, b"x")
    wrong = p.PeripheralType.DIGITAL_OUTPUT if result_family else p.PeripheralType.DIGITAL_INPUT
    for entries in [(entry, entry), ((wrong, 0, b"\0\0"),), ((p.PeripheralType.I2C, 0, b"x"),)]:
        with pytest.raises(p.ApplicationEncodeError) as caught:
            codec.encode(message(result_family, entries))
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(golden(result_family, entries))
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_spi_packet_validation_and_raw_result(codec):
    for payload in [b"\x00", b"\x02\x00\x01x", b"\x02\x01\x00x", b"\x01\x02x"]:
        with pytest.raises(p.ApplicationEncodeError):
            codec.encode(message(False, ((p.PeripheralType.SPI, 0, payload),)))
        value = message(True, ((p.PeripheralType.SPI, 0, payload),))
        assert codec.decode(codec.encode(value)) == value


def test_every_truncation_and_bad_padding(codec, result_family):
    entries = ((p.PeripheralType.UART, 0, b"abcde"),)
    wire = golden(result_family, entries)
    for length in range(len(wire)):
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(wire[:length])
        assert caught.value.status is p.ApplicationStatus.TRUNCATED_MESSAGE
    for length in range(23, len(wire)):
        truncated = bytearray(wire[:length])
        truncated[21:23] = struct.pack("<H", length - 23)
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(truncated)
        assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE
    for offset in range(len(wire) - 3, len(wire)):
        malformed = bytearray(wire)
        malformed[offset] = 1
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(malformed)
        assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE


@pytest.mark.parametrize("length", [0, 256, 65535])
def test_invalid_wire_span_length(codec, result_family, length):
    wire = bytearray(golden(result_family, ((p.PeripheralType.UART, 0, b"x"),)))
    offset = 23 + (12 if result_family else 8) + 2
    wire[offset:offset + 2] = struct.pack("<H", length)
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE


@pytest.mark.parametrize("field", ["presence", "subtype", "reserved", "flags"])
def test_invalid_wire_header(codec, result_family, field):
    wire = bytearray(codec.encode(message(result_family)))
    offset, value, status = {
        "presence": (2, 0, p.ApplicationStatus.INCONSISTENT_TEST_ID),
        "subtype": (20, 1, p.ApplicationStatus.INVALID_SUBTYPE),
        "reserved": (30 if result_family else 29, 1, p.ApplicationStatus.MALFORMED_MESSAGE),
        "flags": (29 if result_family else 28, 2, p.ApplicationStatus.VALIDATION_FAILED),
    }[field]
    wire[offset] = value
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is status


def test_configured_decode_span_limit(result_family):
    codec = p.ApplicationCodec(p.ApplicationConfig(max_variable_data_size=3))
    assert codec.decode(golden(result_family, ((p.PeripheralType.UART, 0, b"abc"),)))
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(golden(result_family, ((p.PeripheralType.UART, 0, b"abcd"),)))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_decode_storage_larger_than_wire(codec, result_family):
    entries = representative(result_family)
    value = message(result_family, entries)
    wire = codec.encode(value)
    descriptor = (
        "HIL_Application_Captured_Record_T" if result_family
        else "HIL_Application_Logical_Operation_T"
    )
    assert len(entries) * ffi.sizeof(descriptor) + sum(len(e[2]) for e in entries) > len(wire)
    assert codec.decode(wire) == value


def test_snapshot_and_detached_record_storage(codec, result_family, monkeypatch):
    value = message(result_family, representative(result_family))
    source = bytearray(codec.encode(value))
    query = application._native_storage_size
    read = application._read_message

    def snapshot_query(*args):
        status = query(*args)
        source[:] = bytes(len(source))
        gc.collect()
        return status

    def detached_read(native, storage, capacity):
        decoded = read(native, storage, capacity)
        ffi.buffer(storage, capacity)[:] = bytes(capacity)
        return decoded

    monkeypatch.setattr(application, "_native_storage_size", snapshot_query)
    monkeypatch.setattr(application, "_read_message", detached_read)
    decoded = codec.decode(source)
    gc.collect()
    assert decoded == value
    assert pickle.loads(pickle.dumps(decoded)) == value
    with pytest.raises(FrozenInstanceError):
        decoded.flags = 1


@pytest.mark.parametrize("corruption", ["array", "span", "size", "count"])
def test_native_pointer_invariants(codec, result_family, monkeypatch, corruption):
    wire = codec.encode(message(result_family))
    decode = application._native_decode

    def corrupt(*args):
        status = decode(*args)
        native = args[3]
        body = native.body.variable_test_result if result_family else native.body.update_instruction
        field = "records" if result_family else "operations"
        records = getattr(body, field)
        if corruption == "array":
            setattr(body, field, ffi.NULL)
        elif corruption == "count":
            setattr(body, "record_count" if result_family else "operation_count", 255)
        else:
            # Public pointers are const; mutate the owned allocation to simulate a broken decoder.
            ctype = (
                "HIL_Application_Captured_Record_T" if result_family
                else "HIL_Application_Logical_Operation_T"
            )
            record = ffi.cast(f"{ctype} *", args[4])[0]
            span = record.data if result_family else record.payload
            if corruption == "span":
                span.data = ffi.NULL
            else:
                span.size = 255
        return status

    monkeypatch.setattr(application, "_native_decode", corrupt)
    with pytest.raises(p.ApplicationBindingError):
        codec.decode(wire)


def test_impossible_native_storage_size(codec, result_family, monkeypatch):
    wire = codec.encode(message(result_family))

    def impossible_query(context, data, size, required):
        required[0] = 1 << 30
        return p.ApplicationStatus.OK

    monkeypatch.setattr(application, "_native_storage_size", impossible_query)
    with pytest.raises(p.ApplicationBindingError, match="impossible size"):
        codec.decode(wire)


def test_empty_result_pointer_invariant(codec, monkeypatch):
    wire = codec.encode(message(True, ()))
    decode = application._native_decode
    owner = ffi.new("HIL_Application_Captured_Record_T[]", 1)

    def corrupt(*args):
        status = decode(*args)
        args[3].body.variable_test_result.records = owner
        return status

    monkeypatch.setattr(application, "_native_decode", corrupt)
    with pytest.raises(p.ApplicationBindingError):
        codec.decode(wire)


@pytest.mark.parametrize(
    "factory,field", [(p.LogicalOperation, "payload"), (p.CapturedRecord, "data")]
)
def test_record_representation(factory, field):
    value = factory(p.PeripheralType.UART, 255, bytes(255))
    assert len(getattr(value, field)) == 255
    for invalid in [bytearray(b"x"), memoryview(b"x"), "x", None]:
        with pytest.raises(TypeError):
            factory(p.PeripheralType.UART, 0, invalid)
    for invalid in [-1, 256]:
        with pytest.raises(ValueError):
            factory(p.PeripheralType.UART, invalid, b"x")
    for invalid in [True, 1.0]:
        with pytest.raises(TypeError):
            factory(p.PeripheralType.UART, invalid, b"x")
    with pytest.raises(TypeError):
        factory(16, 0, b"x")
    with pytest.raises(ValueError):
        factory(p.PeripheralType.UART, 0, bytes(256))


def test_message_representation(result_family):
    value = message(result_family)
    field = "records" if result_family else "operations"
    entries = getattr(value, field)
    for invalid in [list(entries), (object(),), (None,)]:
        with pytest.raises(TypeError):
            replace(value, **{field: invalid})
    assert len(getattr(replace(value, **{field: entries * 255}), field)) == 255
    with pytest.raises(ValueError):
        replace(value, **{field: entries * 256})
    for name, invalid in [("flags", -1), ("flags", 256), ("tick_number", -1),
                          ("tick_number", 1 << 32)]:
        with pytest.raises(ValueError):
            replace(value, **{name: invalid})
    with pytest.raises(TypeError):
        replace(value, test_id=None)
    with pytest.raises(TypeError):
        replace(value, flags=True)


def test_result_condition_and_detail_representation(codec):
    value = message(True)
    with pytest.raises(TypeError):
        replace(value, condition=0)
    for detail in [-1, 1 << 32]:
        with pytest.raises(ValueError):
            replace(value, problem_detail=detail)
    with pytest.raises(TypeError):
        replace(value, problem_detail=True)
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(value, condition=p.ResultCondition.RESERVED))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_peripheral_enum_matches_native():
    for member in p.PeripheralType:
        assert member.value == getattr(lib, f"HIL_APPLICATION_PERIPHERAL_{member.name}")
    assert lib.HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION == 21
    assert lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT == 34
    assert lib.HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK == 0
    assert lib.HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS == 1
    assert lib.HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK == 0
    assert lib.HIL_APPLICATION_RESULT_FLAG_HAS_MORE_CHUNKS == 1
