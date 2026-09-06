"""Public Application codec, native failures and wrapper-owned invariants."""

from __future__ import annotations

import copy
import gc
import pickle
from concurrent.futures import ThreadPoolExecutor
from dataclasses import fields, is_dataclass, replace

import hil_rig_protocol as p
import pytest
from application_fixtures import configuration, instruction, result
from hil_rig_protocol import _binding, application
from test_native_application import context, encode, message

ffi, lib = _binding.ffi, _binding.lib


@pytest.fixture
def codec():
    return p.ApplicationCodec(p.ApplicationConfig())


@pytest.mark.parametrize(
    "factory,family,size",
    [
        (configuration, "configuration", 226),
        (instruction, "instruction", 73),
        (result, "result", 62),
    ],
)
def test_populated_round_trip_matches_independent_native_fixture(codec, factory, family, size):
    public = factory()
    native, owner = message(family)
    wire, count = encode(context(), native)
    expected = bytes(ffi.buffer(wire, count))
    assert codec.encode(public) == expected
    assert len(expected) == size
    assert codec.decode(expected) == public
    assert owner == ffi.NULL


@pytest.mark.parametrize("size", [0, 7, 255])
def test_configuration_extension_round_trip(codec, size):
    public = configuration(bytes(range(size)))
    encoded = codec.encode(public)
    assert len(encoded) == 226 + size
    assert codec.decode(encoded) == public
    native, owner = message("configuration", public.extension_data)
    wire, count = encode(context(), native)
    assert encoded == bytes(ffi.buffer(wire, count))
    assert (owner == ffi.NULL) == (size == 0)


def test_can_filters_round_trip_and_application_version(codec):
    public = configuration()
    assert public.can[0].filter_id != public.can[1].filter_id
    assert public.can[0].filter_mask != public.can[1].filter_mask
    encoded = codec.encode(public)
    assert encoded[:2] == bytes((0, 1))
    assert codec.decode(encoded).can == public.can


def test_can_filter_python_representation_is_uint16_not_protocol_width():
    value = p.CANConfig(enabled=True, bit_rate=500000, filter_id=0xFFFF, filter_mask=0xFFFF)
    assert value.filter_id == 0xFFFF
    assert value.filter_mask == 0xFFFF
    with pytest.raises(ValueError):
        p.CANConfig(filter_id=0x10000)
    with pytest.raises(ValueError):
        p.CANConfig(filter_mask=0x10000)


def test_native_rejects_eleven_bit_can_filter_overflow(codec):
    for field in ("filter_id", "filter_mask"):
        invalid_can = replace(configuration().can[0], **{field: 0x800})
        invalid = replace(configuration(), can=(invalid_can, configuration().can[1]))
        with pytest.raises(p.ApplicationEncodeError) as caught:
            codec.encode(invalid)
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize(
    "field,value",
    [("bit_rate", 1), ("capture_limit_bytes", 1), ("filter_id", 1), ("filter_mask", 1)],
)
def test_disabled_can_requires_all_remaining_fields_zero(codec, field, value):
    disabled = p.CANConfig(**{field: value})
    invalid = replace(configuration(), can=(disabled, configuration().can[1]))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(invalid)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_nonzero_can_filter_id_with_zero_mask_is_valid(codec):
    channel = p.CANConfig(enabled=True, bit_rate=500000, filter_id=0x321, filter_mask=0)
    public = replace(configuration(), can=(channel, configuration().can[1]))
    assert codec.decode(codec.encode(public)) == public


@pytest.mark.parametrize("condition", list(p.ResultCondition))
def test_every_result_condition(codec, condition):
    public = replace(result(), condition=condition)
    if condition is p.ResultCondition.RESERVED:
        with pytest.raises(p.ApplicationEncodeError) as caught:
            codec.encode(public)
        assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED
    else:
        assert codec.decode(codec.encode(public)) == public


def test_defaults_direction_neutrality_and_statelessness(codec):
    assert codec.config == p.ApplicationConfig()
    public = p.TestConfiguration(p.TestId(bytes(16)), p.TickDuration(10), 1)
    for value in (result(), instruction(), public, result(), instruction()):
        assert codec.decode(codec.encode(value)) == value
    assert codec.config is codec.config
    for name in ("context", "_context", "ffi", "lib", "close", "reset", "__enter__"):
        assert not hasattr(codec, name)


@pytest.mark.parametrize(
    "config,status",
    [
        (p.ApplicationConfig(max_encoded_message_size=0), p.ApplicationStatus.BUFFER_TOO_SMALL),
        (p.ApplicationConfig(max_encoded_message_size=65559), p.ApplicationStatus.INVALID_LENGTH),
        (p.ApplicationConfig(max_variable_data_size=256), p.ApplicationStatus.INVALID_COUNT),
        (p.ApplicationConfig(max_variable_transfers_per_tick=9), p.ApplicationStatus.INVALID_COUNT),
        (p.ApplicationConfig(max_expected_tick_count=1000001), p.ApplicationStatus.INVALID_LENGTH),
    ],
)
def test_native_initialization_failures(config, status):
    with pytest.raises(p.ApplicationConfigurationError) as caught:
        p.ApplicationCodec(config)
    assert caught.value.status is status
    assert isinstance(caught.value, (p.ProtocolError, ValueError))


@pytest.mark.parametrize("bad", [None, {}, 512, p.TransportConfig()])
def test_exact_config_required(bad):
    with pytest.raises(TypeError):
        p.ApplicationCodec(bad)


def test_configuration_and_message_subclasses_rejected(codec):
    class Config(p.ApplicationConfig):
        pass

    class Instruction(p.TestInstruction):
        pass

    with pytest.raises(TypeError):
        p.ApplicationCodec(Config())
    with pytest.raises(TypeError):
        codec.encode(Instruction(p.TestId(bytes(16))))


@pytest.mark.parametrize("bad", [None, {}, b"", 1, p.TestId(bytes(16))])
def test_exact_supported_messages_only(codec, bad):
    with pytest.raises(TypeError):
        codec.encode(bad)


def change(value, path, replacement):
    head, *tail = path.split(".")
    if type(value) is tuple:
        index = int(head)
        updated = replacement if not tail else change(value[index], ".".join(tail), replacement)
        return (*value[:index], updated, *value[index + 1 :])
    updated = replacement if not tail else change(getattr(value, head), ".".join(tail), replacement)
    return replace(value, **{head: updated})


@pytest.mark.parametrize(
    "path,value",
    [
        ("tick_duration_us.microseconds", 999),
        ("expected_tick_count", 0),
        ("flags", 1),
        ("digital_in.9.voltage_level", p.PeripheralVoltage.RESERVED),
        ("digital_out.9.voltage_level", p.PeripheralVoltage.INVALID),
        ("digital_out.9.enabled", False),
        ("pwm_in.1.voltage_level", p.PeripheralVoltage.RESERVED),
        ("pwm_out.1.initial_period_nanoseconds", 0),
        ("pwm_out.1.initial_duty_cycle_permyriad", 10001),
        ("can.1.bit_rate", 0),
        ("can.1.capture_limit_bytes", 256),
        ("can.1.filter_id", 0x800),
        ("can.1.filter_mask", 0x800),
        ("spi.1.role", p.BusRole.RESERVED),
        ("spi.1.data_width", p.SPIDataWidth.RESERVED),
        ("spi.1.bit_order", p.SPIBitOrder.RESERVED),
        ("spi.1.clock_polarity", p.SPIClockPolarity.RESERVED),
        ("spi.1.clock_phase", p.SPIClockPhase.RESERVED),
        ("uart.0.rx_enabled", False),
        ("uart.1.electrical_mode", p.UARTElectricalMode.RESERVED),
        ("uart.1.word_length", p.UARTWordLength.RESERVED),
        ("uart.1.parity", p.UARTParity.RESERVED),
        ("uart.1.stop_bits", p.UARTStopBits.RESERVED),
        ("i2c.0.own_address_7bit", 2),
        ("i2c.1.own_address_7bit", 128),
        ("i2c.1.voltage_level", p.I2CVoltage.RESERVED),
        ("i2c.1.pull_up", p.I2CPullUp.RESERVED),
    ],
)
def test_native_configuration_semantics(codec, path, value):
    invalid = change(configuration(), path, value)
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(invalid)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize(
    "factory,changes",
    [
        (instruction, {"tick_number": 1000000}),
        (instruction, {"pwm_outputs": (p.PWMOutputValue(0, 1),) * 2}),
        (result, {"pwm_inputs": (p.PWMInputValue(5, 10001),) * 2}),
        (result, {"tick_number": 1000000}),
    ],
)
def test_native_fixed_semantics(codec, factory, changes):
    with pytest.raises(p.ApplicationEncodeError) as caught:
        codec.encode(replace(factory(), **changes))
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


def test_policy_limits_are_native(codec):
    small = p.ApplicationCodec(p.ApplicationConfig(max_encoded_message_size=73))
    with pytest.raises(p.ApplicationEncodeError) as caught:
        small.encode(configuration())
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL
    with pytest.raises(p.ApplicationDecodeError) as caught:
        small.decode(codec.encode(configuration()))
    assert caught.value.status is p.ApplicationStatus.INVALID_LENGTH
    restricted = p.ApplicationCodec(p.ApplicationConfig(max_variable_data_size=3))
    public = p.TestConfiguration(
        p.TestId(bytes(16)), p.TickDuration(1000), 1, extension_data=b"four"
    )
    with pytest.raises(p.ApplicationEncodeError) as caught:
        restricted.encode(public)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED


@pytest.mark.parametrize("factory", [configuration, instruction, result])
def test_all_truncated_prefixes_and_trailing_data(codec, factory):
    wire = codec.encode(factory())
    for length in range(len(wire)):
        with pytest.raises(p.ApplicationDecodeError) as caught:
            codec.decode(wire[:length])
        assert caught.value.status is p.ApplicationStatus.TRUNCATED_MESSAGE
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire + b"\x00")
    assert caught.value.status is p.ApplicationStatus.MALFORMED_MESSAGE
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire + bytes(512))
    assert caught.value.status is p.ApplicationStatus.INVALID_LENGTH


@pytest.mark.parametrize(
    "offset,value,status",
    [
        (19, 255, p.ApplicationStatus.INVALID_MESSAGE_TYPE),
        (19, 254, p.ApplicationStatus.INVALID_MESSAGE_TYPE),
        (20, 1, p.ApplicationStatus.INVALID_SUBTYPE),
        (20, 255, p.ApplicationStatus.INVALID_SUBTYPE),
        (2, 0, p.ApplicationStatus.INCONSISTENT_TEST_ID),
        (0, 255, p.ApplicationStatus.UNSUPPORTED_MESSAGE),
    ],
)
def test_invalid_wire_envelope(codec, offset, value, status):
    wire = bytearray(codec.encode(instruction()))
    wire[offset] = value
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is status


@pytest.mark.parametrize(
    "family",
    [
        "VARIABLE_INSTRUCTION_DATA",
        "VARIABLE_RESULT_DATA",
        "EXECUTION_CONTROL",
        "GLOBAL_CONTROL",
        "RESPONSE",
        "ERROR",
    ],
)
def test_deferred_families(codec, family):
    wire = bytearray(codec.encode(instruction()))
    wire[19] = getattr(lib, "HIL_APPLICATION_MESSAGE_TYPE_" + family)
    # Match the authoritative fixed body widths so deferred validation is reached.
    if family in ("EXECUTION_CONTROL", "GLOBAL_CONTROL", "RESPONSE"):
        size = 13 if family == "RESPONSE" else 5
        wire[21:23] = size.to_bytes(2, "little")
        wire[23:] = bytes(size)
        if family != "RESPONSE":
            wire[23] = 1  # START or RESET_APPLICATION from application_control.h.
    if family in ("GLOBAL_CONTROL", "ERROR"):
        wire[2] = 0
        wire[3:19] = bytes(16)
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    expected = (
        None
        if family in ("EXECUTION_CONTROL", "GLOBAL_CONTROL")
        else p.ApplicationStatus.NOT_IMPLEMENTED
    )
    assert caught.value.status is expected


@pytest.mark.parametrize(
    "wire",
    [
        # Golden vectors from tests/c/application/test_application_codec.cpp.
        bytes.fromhex("00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 01 02 00 01 01"),
        bytes.fromhex(
            "00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 02 "
            "01 0f 00 00 00 01 00 00 00 02 00 03 00 04 00 01 aa 00"
        ),
    ],
)
def test_native_system_info_is_deferred_in_python(codec, wire):
    required = ffi.new("size_t *")
    expected = p.ApplicationStatus.OK if wire[19] == 1 else p.ApplicationStatus.NOT_IMPLEMENTED
    assert (
        lib.HIL_APPLICATION_Validate_Encoded_Message(context(), wire, len(wire), required)
        == expected
    )
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is (None if expected is p.ApplicationStatus.OK else expected)


def assert_python_owned(value):
    if is_dataclass(value):
        assert not hasattr(value, "__dict__")
        for field in fields(value):
            assert_python_owned(getattr(value, field.name))
    elif type(value) is tuple:
        for item in value:
            assert_python_owned(item)
    else:
        assert type(value) in (int, bool, bytes) or type(value) in {
            p.PeripheralVoltage,
            p.BusRole,
            p.SPIDataWidth,
            p.SPIBitOrder,
            p.SPIClockPolarity,
            p.SPIClockPhase,
            p.UARTElectricalMode,
            p.UARTWordLength,
            p.UARTParity,
            p.UARTStopBits,
            p.I2CVoltage,
            p.I2CPullUp,
            p.ResultCondition,
        }


def test_mutable_input_snapshot_and_detached_storage(codec, monkeypatch):
    expected = configuration(bytes(range(255)))
    wire = bytearray(codec.encode(expected))
    original_query = application._native_storage_size
    original_read = application._read_message

    def query(*args):
        status = original_query(*args)
        wire[:] = bytes(len(wire))
        gc.collect()
        return status

    def read(native, storage, capacity):
        decoded = original_read(native, storage, capacity)
        # The native allocation is still alive here. Overwrite both the span
        # storage and body before returning the already-converted Python object.
        ffi.buffer(storage, capacity)[:] = bytes(capacity)
        native.test_id.bytes[0:16] = bytes(16)
        native.body.test_configuration.expected_tick_count = 0
        return decoded

    monkeypatch.setattr(application, "_native_storage_size", query)
    monkeypatch.setattr(application, "_read_message", read)
    decoded = codec.decode(wire)
    assert decoded == expected
    assert_python_owned(decoded)
    gc.collect()
    for _ in range(10):
        assert codec.decode(codec.encode(expected)) == decoded
    assert decoded.extension_data == bytes(range(255))


@pytest.mark.parametrize("buffer_type", [bytes, bytearray, memoryview])
def test_buffer_inputs(codec, buffer_type):
    expected = instruction()
    assert codec.decode(buffer_type(codec.encode(expected))) == expected


def test_buffer_convention(codec):
    with pytest.raises(TypeError):
        codec.decode([1, 2])
    with pytest.raises(TypeError):
        codec.decode("message")
    with pytest.raises(BufferError):
        codec.decode(memoryview(b"abcdef")[::2])


@pytest.mark.parametrize(
    "operation", [lambda c: c.config, lambda c: c.encode(instruction()), lambda c: c.decode(b"")]
)
def test_thread_ownership(codec, operation):
    with ThreadPoolExecutor(max_workers=1) as executor:
        with pytest.raises(p.ApplicationOwnershipError) as caught:
            executor.submit(operation, codec).result()
    assert caught.value.status is None
    assert codec.decode(codec.encode(instruction())) == instruction()


@pytest.mark.parametrize("operation", [copy.copy, copy.deepcopy, pickle.dumps])
def test_codec_cannot_be_copied_or_pickled(codec, operation):
    with pytest.raises(TypeError):
        operation(codec)


@pytest.mark.parametrize(
    "name",
    [
        "_native_init",
        "_native_encoded_size",
        "_native_encode",
        "_native_storage_size",
        "_native_decode",
    ],
)
@pytest.mark.parametrize(
    "status,error",
    [
        (9876, p.ApplicationBindingError),
        (p.ApplicationStatus.INVALID_ARGUMENT, p.ApplicationBindingError),
        (p.ApplicationStatus.INTERNAL_ERROR, p.ApplicationInternalError),
        (p.ApplicationStatus.UNINITIALIZED, p.ApplicationBindingError),
    ],
)
def test_native_status_invariants(codec, monkeypatch, name, status, error):
    wire = codec.encode(instruction())
    monkeypatch.setattr(application, name, lambda *args: status)
    with pytest.raises(error) as caught:
        if name == "_native_init":
            p.ApplicationCodec(p.ApplicationConfig())
        elif name in ("_native_encoded_size", "_native_encode"):
            codec.encode(instruction())
        else:
            codec.decode(wire)
    assert caught.value.status is (None if status == 9876 else status)


@pytest.mark.parametrize("name", ["_native_encode", "_native_decode"])
def test_unexpected_buffer_too_small(codec, monkeypatch, name):
    wire = codec.encode(instruction())
    monkeypatch.setattr(application, name, lambda *args: p.ApplicationStatus.BUFFER_TOO_SMALL)
    with pytest.raises(p.ApplicationBindingError) as caught:
        if name in ("_native_encoded_size", "_native_encode"):
            codec.encode(instruction())
        else:
            codec.decode(wire)
    assert caught.value.status is p.ApplicationStatus.BUFFER_TOO_SMALL


@pytest.mark.parametrize(
    "name", ["_native_encoded_size", "_native_encode", "_native_storage_size", "_native_decode"]
)
def test_size_and_storage_invariants(codec, monkeypatch, name):
    wire = codec.encode(configuration(b"abc"))
    original = getattr(application, name)

    def inconsistent(*args):
        status = original(*args)
        args[-1][0] += 1
        return status

    monkeypatch.setattr(application, name, inconsistent)
    with pytest.raises(p.ApplicationBindingError):
        if name in ("_native_encoded_size", "_native_encode"):
            codec.encode(configuration(b"abc"))
        else:
            codec.decode(wire)


@pytest.mark.parametrize(
    "path,value",
    [
        ("type", 254),
        ("has_test_id", 0),
        ("subtype", 1),
        ("body.test_configuration.digital_in.0.enabled", 2),
        ("body.test_configuration.digital_in.0.voltage_level", 254),
        ("body.test_configuration.extension_data.size", 2),
        ("body.test_configuration.extension_data.data", ffi.NULL),
    ],
)
def test_impossible_decoded_message(codec, monkeypatch, path, value):
    from test_native_application import set_field

    original = application._native_decode
    wire = codec.encode(configuration(b"abc"))

    def inconsistent(*args):
        status = original(*args)
        set_field(args[3], path, value)
        return status

    monkeypatch.setattr(application, "_native_decode", inconsistent)
    with pytest.raises(p.ApplicationBindingError):
        codec.decode(wire)


def test_raw_cffi_exceptions_are_hidden(codec, monkeypatch):
    def broken(*args):
        ffi.new("not_a_real_C_type")

    monkeypatch.setattr(application, "_native_encode", broken)
    with pytest.raises(p.ApplicationBindingError) as caught:
        codec.encode(instruction())
    assert caught.value.__suppress_context__


@pytest.mark.parametrize("size", [0, 513])
def test_impossible_encoded_size_query(codec, monkeypatch, size):
    def query(context, message, required):
        required[0] = size
        return p.ApplicationStatus.OK

    monkeypatch.setattr(application, "_native_encoded_size", query)
    with pytest.raises(p.ApplicationBindingError):
        codec.encode(instruction())


def test_fixed_decode_cannot_publish_extension_storage(codec, monkeypatch):
    original = application._native_decode
    wire = codec.encode(configuration(b"extension"))

    def decode(*args):
        status = original(*args)
        args[3].type = lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
        return status

    monkeypatch.setattr(application, "_native_decode", decode)
    with pytest.raises(p.ApplicationBindingError):
        codec.decode(wire)


def test_contiguous_typed_buffer_snapshot(codec):
    expected = configuration(b"\x00\x00")
    # Buffer means raw nbytes, not a sequence of coerced integer elements.
    view = memoryview(bytearray(codec.encode(expected))).cast("I")
    assert codec.decode(view) == expected
    view.release()


def test_required_configuration_argument():
    with pytest.raises(TypeError):
        p.ApplicationCodec()


def test_decoded_native_body_validation_status(codec):
    wire = bytearray(codec.encode(instruction()))
    wire[27] = 2  # First digital output, after the 23-byte envelope and uint32 tick.
    with pytest.raises(p.ApplicationDecodeError) as caught:
        codec.decode(wire)
    assert caught.value.status is p.ApplicationStatus.VALIDATION_FAILED
