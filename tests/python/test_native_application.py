"""Native Application contract tests; C is the only codec and validator."""

from __future__ import annotations

import gc

import pytest
from hil_rig_protocol import _binding

ffi, lib = _binding.ffi, _binding.lib


def context(**limits):
    config = ffi.new("HIL_Application_Config_T *")
    assert lib.HIL_APPLICATION_Default_Config(config) == lib.HIL_APPLICATION_STATUS_OK
    for field, value in limits.items():
        setattr(config, field, value)
    result = ffi.new("HIL_Application_Context_T *")
    assert lib.HIL_APPLICATION_Init(result, config) == lib.HIL_APPLICATION_STATUS_OK
    return result


def make_test_id():
    result = ffi.new("HIL_Application_Test_Id_T *")
    result.bytes[0:16] = bytes((i * 17 + 9) % 256 for i in range(16))
    return result[0]


def message(family, extension=b"", condition=None):
    result = ffi.new("HIL_Application_Message_T *")
    result.type = getattr(lib, "HIL_APPLICATION_MESSAGE_TYPE_TEST_" + family.upper())
    result.subtype = lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
    result.has_test_id = 1
    result.test_id = make_test_id()
    body = getattr(result.body, "test_" + family)
    owner = ffi.NULL
    if family == "configuration":
        body.tick_duration_us.microseconds = 1000
        body.expected_tick_count = 987654
        body.flags = 0  # Reserved by the public C contract.
        voltages = (
            lib.HIL_APPLICATION_PERIPHERAL_CONFIG_3V3,
            lib.HIL_APPLICATION_PERIPHERAL_CONFIG_5V,
            lib.HIL_APPLICATION_PERIPHERAL_CONFIG_12V,
            lib.HIL_APPLICATION_PERIPHERAL_CONFIG_24V,
        )
        for name in ("digital_in", "digital_out", "analog_in", "analog_out", "pwm_in", "pwm_out"):
            for i, channel in enumerate(getattr(body, name)):
                channel.enabled = 1 if not name.startswith("analog") else i % 2
                if not name.startswith("analog"):
                    channel.voltage_level = voltages[i % len(voltages)]
                if name == "digital_out":
                    channel.initial_high = i % 2
                if name == "pwm_out":
                    channel.initial_period_nanoseconds = 123456 + i * 76543
                    channel.initial_duty_cycle_permyriad = 1234 + i * 4321
        for i, channel in enumerate(body.can):
            channel.enabled = 1
            channel.bit_rate = 125000 * (i + 1)
            channel.termination_enabled = i % 2
            channel.capture_limit_bytes = 71 + i
        for i, channel in enumerate(body.spi):
            channel.enabled = 1
            channel.bit_rate = 234567 * (i + 1)
            channel.role = (
                lib.HIL_APPLICATION_BUS_ROLE_MASTER,
                lib.HIL_APPLICATION_BUS_ROLE_SLAVE,
            )[i]
            channel.data_width = (
                lib.HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS,
                lib.HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS,
            )[i]
            channel.bit_order = (
                lib.HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST,
                lib.HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST,
            )[i]
            channel.clock_polarity = (
                lib.HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW,
                lib.HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH,
            )[i]
            channel.clock_phase = (
                lib.HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE,
                lib.HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE,
            )[i]
            channel.capture_limit_bytes = 81 + i
        for i, channel in enumerate(body.uart):
            channel.enabled = 1
            channel.baud_rate = 57600 * (i + 1)
            channel.electrical_mode = (
                lib.HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V,
                lib.HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232,
            )[i]
            channel.word_length = (
                lib.HIL_APPLICATION_UART_WORD_LENGTH_8_BITS,
                lib.HIL_APPLICATION_UART_WORD_LENGTH_9_BITS,
            )[i]
            channel.parity = (
                lib.HIL_APPLICATION_UART_PARITY_EVEN,
                lib.HIL_APPLICATION_UART_PARITY_ODD,
            )[i]
            channel.stop_bits = (
                lib.HIL_APPLICATION_UART_STOP_BITS_1,
                lib.HIL_APPLICATION_UART_STOP_BITS_2,
            )[i]
            channel.rx_enabled = 1
            channel.tx_enabled = i
            channel.capture_limit_bytes = 91 + i
        for i, channel in enumerate(body.i2c):
            channel.enabled = 1
            channel.bit_rate = 100000 * (i + 1)
            channel.role = (
                lib.HIL_APPLICATION_BUS_ROLE_MASTER,
                lib.HIL_APPLICATION_BUS_ROLE_SLAVE,
            )[i]
            channel.own_address_7bit = 0 if i == 0 else 0x53
            channel.voltage_level = (
                lib.HIL_APPLICATION_I2C_VOLTAGE_3V3,
                lib.HIL_APPLICATION_I2C_VOLTAGE_5V,
            )[i]
            channel.pull_up = (
                lib.HIL_APPLICATION_I2C_PULL_UP_2K2,
                lib.HIL_APPLICATION_I2C_PULL_UP_10K,
            )[i]
            channel.capture_limit_bytes = 101 + i
        if extension:
            owner = ffi.new("uint8_t[]", extension)
            body.extension_data.data = owner
        body.extension_data.size = len(extension)
    else:
        body.tick_number = 876543
        suffix = "outputs" if family == "instruction" else "inputs"
        for i, channel in enumerate(getattr(body, "digital_" + suffix)):
            channel.high = (i + 1) % 2
        for i, channel in enumerate(getattr(body, "analog_" + suffix)):
            channel.microvolts = 0xF1234567 - i * 7654321
        for i, channel in enumerate(getattr(body, "pwm_" + suffix)):
            channel.period_nanoseconds = 0xE1234567 - i * 1234567
            channel.duty_cycle_permyriad = 1234 + i * 4321
        if family == "result":
            body.condition = (
                lib.HIL_APPLICATION_RESULT_CONDITION_OK if condition is None else condition
            )
            body.problem_detail = 0xD1234567
    return result, owner


def encode(ctx, msg):
    assert lib.HIL_APPLICATION_Validate_Message(ctx, msg) == lib.HIL_APPLICATION_STATUS_OK
    size = ffi.new("size_t *", 999)
    assert lib.HIL_APPLICATION_Encoded_Size(ctx, msg, size) == lib.HIL_APPLICATION_STATUS_OK
    wire = ffi.new("uint8_t[]", size[0])
    written = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Encode_Message(ctx, msg, wire, size[0], written)
        == lib.HIL_APPLICATION_STATUS_OK
    )
    assert written[0] == size[0]
    return wire, size[0]


def required_storage(ctx, wire, size):
    required = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Decode_Storage_Size(ctx, wire, size, required)
        == lib.HIL_APPLICATION_STATUS_OK
    )
    validated = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Validate_Encoded_Message(ctx, wire, size, validated)
        == lib.HIL_APPLICATION_STATUS_OK
    )
    assert validated[0] == required[0]
    return required[0]


def aligned_storage(capacity):
    assert ffi.alignof("max_align_t") % lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT == 0
    if capacity == 0:
        return ffi.NULL, ffi.NULL
    width = ffi.sizeof("max_align_t")
    owner = ffi.new("max_align_t[]", (capacity + width - 1) // width)
    return owner, ffi.cast("uint8_t *", owner)


def decode(ctx, wire, size):
    capacity = required_storage(ctx, wire, size)
    owner, storage = aligned_storage(capacity)
    result = ffi.new("HIL_Application_Message_T *")
    used = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Decode_Message(ctx, wire, size, result, storage, capacity, used)
        == lib.HIL_APPLICATION_STATUS_OK
    )
    assert used[0] == capacity
    return result, owner, capacity


def native_value(value):
    """Compare public fields recursively, never native padding or wire offsets."""
    if isinstance(value, int):
        return value
    ctype = ffi.typeof(value)
    if ctype.cname == "HIL_Application_Byte_Span_T":
        return bytes(ffi.buffer(value.data, value.size)) if value.size else b""
    if ctype.kind == "array":
        return [native_value(item) for item in value]
    assert ctype.kind == "struct", ctype
    return {name: native_value(getattr(value, name)) for name, _ in ctype.fields}


def assert_message_equal(left, right, family):
    for name in ("type", "subtype", "has_test_id", "test_id"):
        assert native_value(getattr(left, name)) == native_value(getattr(right, name))
    field = "test_" + family
    assert native_value(getattr(left.body, field)) == native_value(getattr(right.body, field))


@pytest.mark.parametrize("extension_size", [0, 7, 255])
def test_configuration_round_trip(extension_size):
    ctx = context()
    extension = bytes((i * 37 + 11) % 256 for i in range(extension_size))
    msg, source_owner = message("configuration", extension)
    wire, size = encode(ctx, msg)
    assert size == 220 + extension_size
    if extension_size == 255:
        assert size == 475
    decoded, owner, used = decode(ctx, wire, size)
    assert used == extension_size
    assert_message_equal(msg, decoded, "configuration")
    span = decoded.body.test_configuration.extension_data
    if extension_size:
        assert span.data == ffi.cast("uint8_t *", owner)
        assert span.data != source_owner
        # Neither encoded input nor the original span owns the decoded bytes.
        ffi.buffer(wire, size)[:] = b"\x00" * size
        del wire, msg, source_owner
        gc.collect()
        assert bytes(ffi.buffer(span.data, span.size)) == extension
    else:
        assert owner == ffi.NULL
        assert span.data == ffi.NULL


@pytest.mark.parametrize(
    "family,condition,size",
    [
        ("instruction", None, 73),
        ("result", lib.HIL_APPLICATION_RESULT_CONDITION_OK, 62),
        ("result", lib.HIL_APPLICATION_RESULT_CONDITION_PARTIAL, 62),
        ("result", lib.HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM, 62),
    ],
)
def test_fixed_round_trip(family, condition, size):
    ctx = context()
    msg, source_owner = message(family, condition=condition)
    wire, written = encode(ctx, msg)
    assert written == size
    decoded, owner, used = decode(ctx, wire, written)
    assert used == 0
    assert owner == source_owner == ffi.NULL
    assert_message_equal(msg, decoded, family)


def test_defaults_and_smaller_limits():
    config = ffi.new("HIL_Application_Config_T *")
    assert lib.HIL_APPLICATION_Default_Config(config) == lib.HIL_APPLICATION_STATUS_OK
    assert native_value(config[0]) == {
        "max_encoded_message_size": 512,
        "max_variable_data_size": 255,
        "max_variable_transfers_per_tick": 8,
        "max_expected_tick_count": 1000000,
    }
    context()
    ctx = context(
        max_encoded_message_size=73,
        max_variable_data_size=0,
        max_variable_transfers_per_tick=0,
        max_expected_tick_count=2,
    )
    msg, _ = message("instruction")
    msg.body.test_instruction.tick_number = 1
    wire, size = encode(ctx, msg)
    decoded, owner, _ = decode(ctx, wire, size)
    assert_message_equal(msg, decoded, "instruction")
    assert owner == ffi.NULL
    msg.body.test_instruction.tick_number = 2
    assert (
        lib.HIL_APPLICATION_Validate_Message(ctx, msg)
        == lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED
    )


@pytest.mark.parametrize(
    "field,value,status",
    [
        ("max_encoded_message_size", 24, lib.HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL),
        ("max_encoded_message_size", 65559, lib.HIL_APPLICATION_STATUS_INVALID_LENGTH),
        ("max_variable_data_size", 256, lib.HIL_APPLICATION_STATUS_INVALID_COUNT),
        ("max_variable_transfers_per_tick", 9, lib.HIL_APPLICATION_STATUS_INVALID_COUNT),
        ("max_expected_tick_count", 1000001, lib.HIL_APPLICATION_STATUS_INVALID_LENGTH),
    ],
)
def test_invalid_initialization(field, value, status):
    ctx = context()
    config = ffi.new("HIL_Application_Config_T *")
    assert lib.HIL_APPLICATION_Default_Config(config) == lib.HIL_APPLICATION_STATUS_OK
    setattr(config, field, value)
    assert lib.HIL_APPLICATION_Init(ctx, config) == status
    msg, _ = message("instruction")
    assert (
        lib.HIL_APPLICATION_Validate_Message(ctx, msg) == lib.HIL_APPLICATION_STATUS_UNINITIALIZED
    )


def assert_decode_failure(ctx, wire, size, status, capacity=0):
    owner, storage = aligned_storage(capacity)
    result = ffi.new("HIL_Application_Message_T *")
    result.type = lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
    used = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Decode_Message(ctx, wire, size, result, storage, capacity, used)
        == status
    )
    assert used[0] == 0
    assert result.type == lib.HIL_APPLICATION_MESSAGE_TYPE_INVALID
    return owner


@pytest.mark.parametrize("family", ["configuration", "instruction", "result"])
def test_undersized_encode_and_malformed_complete_messages(family):
    ctx = context()
    msg, source_owner = message(family)
    wire, size = encode(ctx, msg)
    short = ffi.new("uint8_t[]", size - 1)
    written = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Encode_Message(ctx, msg, short, size - 1, written)
        == lib.HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL
    )
    assert written[0] == 0
    assert source_owner == ffi.NULL
    for data, length, status in (
        (
            wire,
            lib.HIL_APPLICATION_HEADER_SIZE_BYTES - 1,
            lib.HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE,
        ),
        (wire, size - 1, lib.HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE),
        (
            ffi.new("uint8_t[]", bytes(ffi.buffer(wire, size)) + b"\x00"),
            size + 1,
            lib.HIL_APPLICATION_STATUS_MALFORMED_MESSAGE,
        ),
    ):
        for function in (
            lib.HIL_APPLICATION_Decode_Storage_Size,
            lib.HIL_APPLICATION_Validate_Encoded_Message,
        ):
            required = ffi.new("size_t *", 999)
            assert function(ctx, data, length, required) == status
            assert required[0] == 0
        assert_decode_failure(ctx, data, length, status)


def test_insufficient_extension_storage():
    ctx = context()
    msg, source_owner = message("configuration", bytes(range(255)))
    wire, size = encode(ctx, msg)
    assert required_storage(ctx, wire, size) == 255
    assert_decode_failure(ctx, wire, size, lib.HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL, 254)
    assert_decode_failure(ctx, wire, size, lib.HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL)
    assert source_owner != ffi.NULL


@pytest.mark.parametrize(
    "family,path,value,status",
    [
        ("instruction", "type", 254, lib.HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE),
        ("instruction", "subtype", 254, lib.HIL_APPLICATION_STATUS_INVALID_SUBTYPE),
        ("instruction", "has_test_id", 0, lib.HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID),
        ("instruction", "has_test_id", 2, lib.HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID),
        (
            "instruction",
            "body.test_instruction.tick_number",
            1000000,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        (
            "instruction",
            "body.test_instruction.digital_outputs.9.high",
            2,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        (
            "instruction",
            "body.test_instruction.pwm_outputs.1.duty_cycle_permyriad",
            10001,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        (
            "instruction",
            "body.test_instruction.pwm_outputs.1.period_nanoseconds",
            0,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        ("result", "body.test_result.condition", 255, lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED),
        (
            "result",
            "body.test_result.tick_number",
            1000000,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        (
            "result",
            "body.test_result.digital_inputs.9.high",
            2,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        (
            "result",
            "body.test_result.pwm_inputs.1.duty_cycle_permyriad",
            10001,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
        (
            "result",
            "body.test_result.pwm_inputs.1.period_nanoseconds",
            0,
            lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED,
        ),
    ],
)
def test_invalid_native_message(family, path, value, status):
    ctx = context()
    msg, _ = message(family)
    set_field(msg, path, value)
    assert_typed_failure(ctx, msg, status)


def set_field(value, path, replacement):
    *parents, field = path.split(".")
    for name in parents:
        value = value[int(name)] if name.isdigit() else getattr(value, name)
    setattr(value, field, replacement)


def assert_typed_failure(ctx, msg, status):
    assert lib.HIL_APPLICATION_Validate_Message(ctx, msg) == status
    size = ffi.new("size_t *", 999)
    # Sizing checks the envelope and lengths, not family-specific values.
    # Typed validation and encoding perform the full body validation.
    if status == lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED:
        assert lib.HIL_APPLICATION_Encoded_Size(ctx, msg, size) == lib.HIL_APPLICATION_STATUS_OK
        assert size[0] > 0
    else:
        assert lib.HIL_APPLICATION_Encoded_Size(ctx, msg, size) == status
        assert size[0] == 0
    size[0] = 999
    assert (
        lib.HIL_APPLICATION_Encode_Message(ctx, msg, ffi.new("uint8_t[512]"), 512, size) == status
    )
    assert size[0] == 0


@pytest.mark.parametrize(
    "path,value",
    [
        ("tick_duration_us.microseconds", 0),
        ("tick_duration_us.microseconds", 999),
        ("expected_tick_count", 0),
        ("expected_tick_count", 1000001),
        ("flags", 1),
        ("digital_in.9.enabled", 2),
        ("digital_in.9.voltage_level", 255),
        ("digital_out.9.initial_high", 2),
        ("digital_out.9.voltage_level", 0),
        ("analog_in.1.enabled", 2),
        ("analog_out.5.enabled", 2),
        ("pwm_in.1.voltage_level", 255),
        ("pwm_out.1.voltage_level", 255),
        ("pwm_out.1.initial_period_nanoseconds", 0),
        ("pwm_out.1.initial_duty_cycle_permyriad", 10001),
        ("can.1.bit_rate", 0),
        ("can.1.termination_enabled", 2),
        ("can.1.capture_limit_bytes", 256),
        ("spi.1.bit_rate", 0),
        ("spi.1.role", 255),
        ("spi.1.data_width", 255),
        ("spi.1.bit_order", 255),
        ("spi.1.clock_polarity", 255),
        ("spi.1.clock_phase", 255),
        ("uart.1.baud_rate", 0),
        ("uart.1.electrical_mode", 255),
        ("uart.1.word_length", 255),
        ("uart.1.parity", 255),
        ("uart.1.stop_bits", 255),
        ("uart.1.rx_enabled", 2),
        ("uart.1.tx_enabled", 2),
        ("i2c.1.bit_rate", 0),
        ("i2c.1.role", 255),
        ("i2c.1.own_address_7bit", 128),
        ("i2c.1.voltage_level", 255),
        ("i2c.1.pull_up", 255),
        ("extension_data.size", 1),  # Nonempty span with a NULL pointer.
    ],
)
def test_invalid_configuration_fields(path, value):
    ctx = context()
    msg, _ = message("configuration")
    set_field(msg.body.test_configuration, path, value)
    assert_typed_failure(ctx, msg, lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED)


def test_extension_policy_failure_resets_decode_outputs():
    msg, source_owner = message("configuration", bytes(range(255)))
    wire, size = encode(context(), msg)
    ctx = context(max_variable_data_size=254)
    assert_typed_failure(ctx, msg, lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED)
    for function in (
        lib.HIL_APPLICATION_Decode_Storage_Size,
        lib.HIL_APPLICATION_Validate_Encoded_Message,
    ):
        required = ffi.new("size_t *", 999)
        assert function(ctx, wire, size, required) == lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED
        assert required[0] == 0
    assert_decode_failure(ctx, wire, size, lib.HIL_APPLICATION_STATUS_VALIDATION_FAILED, 255)
    assert source_owner != ffi.NULL


def test_deferred_variable_family():
    ctx = context()
    msg, _ = message("instruction")
    msg.type = lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA
    assert_typed_failure(ctx, msg, lib.HIL_APPLICATION_STATUS_NOT_IMPLEMENTED)


def test_null_initialization_arguments():
    assert (
        lib.HIL_APPLICATION_Default_Config(ffi.NULL) == lib.HIL_APPLICATION_STATUS_INVALID_ARGUMENT
    )
    config = ffi.new("HIL_Application_Config_T *")
    assert lib.HIL_APPLICATION_Default_Config(config) == lib.HIL_APPLICATION_STATUS_OK
    assert lib.HIL_APPLICATION_Init(ffi.NULL, config) == lib.HIL_APPLICATION_STATUS_INVALID_ARGUMENT
    ctx = context()
    assert lib.HIL_APPLICATION_Init(ctx, ffi.NULL) == lib.HIL_APPLICATION_STATUS_INVALID_ARGUMENT
    msg, _ = message("instruction")
    assert_typed_failure(ctx, msg, lib.HIL_APPLICATION_STATUS_UNINITIALIZED)


@pytest.mark.parametrize(
    "operation,null_index",
    [
        ("Validate_Message", 0),
        ("Validate_Message", 1),
        ("Encoded_Size", 0),
        ("Encoded_Size", 1),
        ("Encoded_Size", 2),
        ("Encode_Message", 0),
        ("Encode_Message", 1),
        ("Encode_Message", 2),
        ("Encode_Message", 4),
        ("Decode_Storage_Size", 0),
        ("Decode_Storage_Size", 1),
        ("Decode_Storage_Size", 3),
        ("Validate_Encoded_Message", 0),
        ("Validate_Encoded_Message", 1),
        ("Validate_Encoded_Message", 3),
        ("Decode_Message", 0),
        ("Decode_Message", 1),
        ("Decode_Message", 3),
        ("Decode_Message", 6),
    ],
)
def test_null_operation_arguments(operation, null_index):
    ctx = context()
    msg, _ = message("instruction")
    wire, length = encode(ctx, msg)
    result = ffi.new("HIL_Application_Message_T *")
    result.type = lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
    size = ffi.new("size_t *", 999)
    arguments = {
        "Validate_Message": [ctx, msg],
        "Encoded_Size": [ctx, msg, size],
        "Encode_Message": [ctx, msg, wire, length, size],
        "Decode_Storage_Size": [ctx, wire, length, size],
        "Validate_Encoded_Message": [ctx, wire, length, size],
        "Decode_Message": [ctx, wire, length, result, ffi.NULL, 0, size],
    }[operation]
    arguments[null_index] = ffi.NULL
    assert (
        getattr(lib, "HIL_APPLICATION_" + operation)(*arguments)
        == lib.HIL_APPLICATION_STATUS_INVALID_ARGUMENT
    )
    if operation != "Validate_Message" and arguments[-1] != ffi.NULL:
        assert size[0] == 0
    if operation == "Decode_Message" and null_index != 3:
        assert result.type == lib.HIL_APPLICATION_MESSAGE_TYPE_INVALID


def test_null_storage_with_nonzero_capacity():
    ctx = context()
    msg, _ = message("instruction")
    wire, size = encode(ctx, msg)
    output = ffi.new("HIL_Application_Message_T *")
    output.type = lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
    used = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Decode_Message(ctx, wire, size, output, ffi.NULL, 1, used)
        == lib.HIL_APPLICATION_STATUS_INVALID_ARGUMENT
    )
    assert used[0] == 0
    assert output.type == lib.HIL_APPLICATION_MESSAGE_TYPE_INVALID


# Published enum assignments, checked against compiled constants (not Python enums).
@pytest.mark.parametrize(
    "name,value",
    [
        ("HIL_APPLICATION_MESSAGE_TYPE_INVALID", 0),
        ("HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST", 1),
        ("HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE", 2),
        ("HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION", 16),
        ("HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION", 17),
        ("HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA", 18),
        ("HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL", 19),
        ("HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL", 20),
        ("HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT", 32),
        ("HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA", 33),
        ("HIL_APPLICATION_MESSAGE_TYPE_RESPONSE", 48),
        ("HIL_APPLICATION_MESSAGE_TYPE_ERROR", 49),
        ("HIL_APPLICATION_MESSAGE_TYPE_RESERVED", 255),
        ("HIL_APPLICATION_MESSAGE_SUBTYPE_NONE", 0),
        ("HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC", 1),
        ("HIL_APPLICATION_MESSAGE_SUBTYPE_RESERVED", 255),
        ("HIL_APPLICATION_RESULT_CONDITION_OK", 0),
        ("HIL_APPLICATION_RESULT_CONDITION_PARTIAL", 1),
        ("HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM", 2),
        ("HIL_APPLICATION_RESULT_CONDITION_RESERVED", 255),
        ("HIL_APPLICATION_STATUS_OK", 0),
        ("HIL_APPLICATION_STATUS_INVALID_ARGUMENT", 1),
        ("HIL_APPLICATION_STATUS_UNINITIALIZED", 2),
        ("HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL", 3),
        ("HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE", 4),
        ("HIL_APPLICATION_STATUS_INVALID_SUBTYPE", 5),
        ("HIL_APPLICATION_STATUS_MALFORMED_MESSAGE", 6),
        ("HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE", 7),
        ("HIL_APPLICATION_STATUS_INVALID_LENGTH", 8),
        ("HIL_APPLICATION_STATUS_INVALID_COUNT", 9),
        ("HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE", 10),
        ("HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID", 11),
        ("HIL_APPLICATION_STATUS_INCONSISTENT_TICK", 12),
        ("HIL_APPLICATION_STATUS_INCOMPLETE_DATA", 13),
        ("HIL_APPLICATION_STATUS_VALIDATION_FAILED", 14),
        ("HIL_APPLICATION_STATUS_NOT_IMPLEMENTED", 15),
        ("HIL_APPLICATION_STATUS_INTERNAL_ERROR", 16),
        ("HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID", 0),
        ("HIL_APPLICATION_PERIPHERAL_CONFIG_3V3", 1),
        ("HIL_APPLICATION_PERIPHERAL_CONFIG_5V", 2),
        ("HIL_APPLICATION_PERIPHERAL_CONFIG_12V", 3),
        ("HIL_APPLICATION_PERIPHERAL_CONFIG_24V", 4),
        ("HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED", 255),
        ("HIL_APPLICATION_BUS_ROLE_INVALID", 0),
        ("HIL_APPLICATION_BUS_ROLE_MASTER", 1),
        ("HIL_APPLICATION_BUS_ROLE_SLAVE", 2),
        ("HIL_APPLICATION_BUS_ROLE_RESERVED", 255),
        ("HIL_APPLICATION_SPI_DATA_WIDTH_INVALID", 0),
        ("HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS", 1),
        ("HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS", 2),
        ("HIL_APPLICATION_SPI_DATA_WIDTH_RESERVED", 255),
        ("HIL_APPLICATION_SPI_BIT_ORDER_INVALID", 0),
        ("HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST", 1),
        ("HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST", 2),
        ("HIL_APPLICATION_SPI_BIT_ORDER_RESERVED", 255),
        ("HIL_APPLICATION_SPI_CLOCK_POLARITY_INVALID", 0),
        ("HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW", 1),
        ("HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH", 2),
        ("HIL_APPLICATION_SPI_CLOCK_POLARITY_RESERVED", 255),
        ("HIL_APPLICATION_SPI_CLOCK_PHASE_INVALID", 0),
        ("HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE", 1),
        ("HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE", 2),
        ("HIL_APPLICATION_SPI_CLOCK_PHASE_RESERVED", 255),
        ("HIL_APPLICATION_UART_ELECTRICAL_MODE_INVALID", 0),
        ("HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3", 1),
        ("HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V", 2),
        ("HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232", 3),
        ("HIL_APPLICATION_UART_ELECTRICAL_MODE_RESERVED", 255),
        ("HIL_APPLICATION_UART_WORD_LENGTH_INVALID", 0),
        ("HIL_APPLICATION_UART_WORD_LENGTH_8_BITS", 1),
        ("HIL_APPLICATION_UART_WORD_LENGTH_9_BITS", 2),
        ("HIL_APPLICATION_UART_WORD_LENGTH_RESERVED", 255),
        ("HIL_APPLICATION_UART_PARITY_INVALID", 0),
        ("HIL_APPLICATION_UART_PARITY_NONE", 1),
        ("HIL_APPLICATION_UART_PARITY_EVEN", 2),
        ("HIL_APPLICATION_UART_PARITY_ODD", 3),
        ("HIL_APPLICATION_UART_PARITY_RESERVED", 255),
        ("HIL_APPLICATION_UART_STOP_BITS_INVALID", 0),
        ("HIL_APPLICATION_UART_STOP_BITS_1", 1),
        ("HIL_APPLICATION_UART_STOP_BITS_2", 2),
        ("HIL_APPLICATION_UART_STOP_BITS_RESERVED", 255),
        ("HIL_APPLICATION_I2C_VOLTAGE_INVALID", 0),
        ("HIL_APPLICATION_I2C_VOLTAGE_3V3", 1),
        ("HIL_APPLICATION_I2C_VOLTAGE_5V", 2),
        ("HIL_APPLICATION_I2C_VOLTAGE_RESERVED", 255),
        ("HIL_APPLICATION_I2C_PULL_UP_INVALID", 0),
        ("HIL_APPLICATION_I2C_PULL_UP_1K", 1),
        ("HIL_APPLICATION_I2C_PULL_UP_2K2", 2),
        ("HIL_APPLICATION_I2C_PULL_UP_4K7", 3),
        ("HIL_APPLICATION_I2C_PULL_UP_10K", 4),
        ("HIL_APPLICATION_I2C_PULL_UP_RESERVED", 255),
        ("HIL_APPLICATION_PERIPHERAL_INVALID", 0),
        ("HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT", 1),
        ("HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT", 2),
        ("HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT", 3),
        ("HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT", 4),
        ("HIL_APPLICATION_PERIPHERAL_PWM_INPUT", 5),
        ("HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT", 6),
        ("HIL_APPLICATION_PERIPHERAL_UART", 16),
        ("HIL_APPLICATION_PERIPHERAL_SPI", 17),
        ("HIL_APPLICATION_PERIPHERAL_I2C", 18),
        ("HIL_APPLICATION_PERIPHERAL_CAN", 19),
        ("HIL_APPLICATION_PERIPHERAL_RESERVED", 255),
    ],
)
def test_enum_values(name, value):
    assert getattr(lib, name) == value


@pytest.mark.parametrize(
    "name,value",
    [
        ("HIL_APPLICATION_TEST_ID_SIZE", 16),
        ("HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT", 10),
        ("HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT", 10),
        ("HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT", 6),
        ("HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_CAN_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_UART_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_SPI_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_I2C_CHANNEL_COUNT", 2),
        ("HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE", 255),
        ("HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE", 255),
        ("HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK", 8),
        ("HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT", 1000000),
        ("HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE", 512),
        ("HIL_APPLICATION_PROTOCOL_MAJOR_SIZE_BYTES", 1),
        ("HIL_APPLICATION_PROTOCOL_MINOR_SIZE_BYTES", 1),
        ("HIL_APPLICATION_MESSAGE_HAS_ID_SIZE_BYTES", 1),
        ("HIL_APPLICATION_MESSAGE_TYPE_SIZE_BYTES", 1),
        ("HIL_APPLICATION_MESSAGE_SUB_TYPE_SIZE_BYTES", 1),
        ("HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES", 2),
        ("HIL_APPLICATION_HEADER_SIZE_BYTES", 23),
        ("HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE", 65558),
        ("HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE", 25),
    ],
)
def test_public_constants(name, value):
    assert getattr(lib, name) == value


@pytest.mark.parametrize(
    "name",
    [
        "max_align_t",
        "HIL_Application_Status_T",
        "HIL_Application_Test_Id_T",
        "HIL_Application_Byte_Span_T",
        "HIL_Application_Peripheral_Type_T",
        "HIL_Application_Channel_Id_T",
        "HIL_Application_Tick_Duration_T",
        "HIL_Application_Digital_Output_Value_T",
        "HIL_Application_Digital_Input_Value_T",
        "HIL_Application_Analog_Output_Value_T",
        "HIL_Application_Analog_Input_Value_T",
        "HIL_Application_Pwm_Output_Value_T",
        "HIL_Application_Pwm_Input_Value_T",
        "HIL_Application_Config_T",
        "HIL_Application_Context_T",
        "HIL_Application_Peripheral_Config_Voltage_Level_T",
        "HIL_Application_Bus_Role_T",
        "HIL_Application_Spi_Data_Width_T",
        "HIL_Application_Spi_Bit_Order_T",
        "HIL_Application_Spi_Clock_Polarity_T",
        "HIL_Application_Spi_Clock_Phase_T",
        "HIL_Application_Uart_Electrical_Mode_T",
        "HIL_Application_Uart_Word_Length_T",
        "HIL_Application_Uart_Parity_T",
        "HIL_Application_Uart_Stop_Bits_T",
        "HIL_Application_I2c_Voltage_Level_T",
        "HIL_Application_I2c_Pull_Up_T",
        "HIL_Application_Digital_Input_Config_T",
        "HIL_Application_Digital_Output_Config_T",
        "HIL_Application_Analog_Input_Config_T",
        "HIL_Application_Analog_Output_Config_T",
        "HIL_Application_Pwm_Input_Config_T",
        "HIL_Application_Pwm_Output_Config_T",
        "HIL_Application_Can_Config_T",
        "HIL_Application_Spi_Config_T",
        "HIL_Application_Uart_Config_T",
        "HIL_Application_I2c_Config_T",
        "HIL_Application_Test_Configuration_T",
        "HIL_Application_Test_Instruction_T",
        "HIL_Application_Result_Condition_T",
        "HIL_Application_Test_Result_T",
        "HIL_Application_Message_Type_T",
        "HIL_Application_Message_Subtype_T",
        "HIL_Application_Message_T",
    ],
)
def test_native_sizes_and_alignments(name):
    assert ffi.sizeof(name) > 0
    assert ffi.alignof(name) > 0
    assert ffi.sizeof(name) % ffi.alignof(name) == 0
    assert ffi.new(name + " *") != ffi.NULL


def test_array_extents_and_decode_alignment():
    counts = {
        "digital_in": "DIGITAL_INPUT",
        "digital_out": "DIGITAL_OUTPUT",
        "analog_in": "ANALOG_INPUT",
        "analog_out": "ANALOG_OUTPUT",
        "pwm_in": "PWM_INPUT",
        "pwm_out": "PWM_OUTPUT",
        "can": "CAN",
        "spi": "SPI",
        "uart": "UART",
        "i2c": "I2C",
    }
    config = ffi.new("HIL_Application_Test_Configuration_T *")
    for field, constant in counts.items():
        assert len(getattr(config, field)) == getattr(
            lib, "HIL_APPLICATION_" + constant + "_CHANNEL_COUNT"
        )
    for family, suffix in (("Instruction", "outputs"), ("Result", "inputs")):
        body = ffi.new("HIL_Application_Test_" + family + "_T *")
        for signal in ("digital", "analog", "pwm"):
            field = signal + "_" + suffix
            constant = (signal + "_" + suffix[:-1]).upper()
            assert len(getattr(body, field)) == getattr(
                lib, "HIL_APPLICATION_" + constant + "_CHANNEL_COUNT"
            )
    assert len(ffi.new("HIL_Application_Test_Id_T *").bytes) == lib.HIL_APPLICATION_TEST_ID_SIZE
    owner, storage = aligned_storage(255)
    assert ffi.alignof("max_align_t") >= lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT
    assert int(ffi.cast("uintptr_t", storage)) % lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT == 0
    assert ffi.sizeof(owner) >= 255


@pytest.mark.parametrize("family", ["configuration", "instruction", "result"])
def test_configured_complete_message_limit(family):
    msg, source_owner = message(family)
    wire, length = encode(context(), msg)
    ctx = context(max_encoded_message_size=length - 1)
    size = ffi.new("size_t *", 999)
    assert (
        lib.HIL_APPLICATION_Encoded_Size(ctx, msg, size)
        == lib.HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL
    )
    assert size[0] == 0
    size[0] = 999
    assert (
        lib.HIL_APPLICATION_Encode_Message(ctx, msg, wire, length, size)
        == lib.HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL
    )
    assert size[0] == 0
    # Recreate valid input: failed encodes leave buffer contents unspecified.
    wire, length = encode(context(), msg)
    for function in (
        lib.HIL_APPLICATION_Decode_Storage_Size,
        lib.HIL_APPLICATION_Validate_Encoded_Message,
    ):
        size[0] = 999
        assert function(ctx, wire, length, size) == lib.HIL_APPLICATION_STATUS_INVALID_LENGTH
        assert size[0] == 0
    assert_decode_failure(ctx, wire, length, lib.HIL_APPLICATION_STATUS_INVALID_LENGTH)
    assert source_owner == ffi.NULL


def test_uninitialized_decode_context():
    msg, _ = message("instruction")
    wire, length = encode(context(), msg)
    ctx = ffi.new("HIL_Application_Context_T *")
    for function in (
        lib.HIL_APPLICATION_Decode_Storage_Size,
        lib.HIL_APPLICATION_Validate_Encoded_Message,
    ):
        size = ffi.new("size_t *", 999)
        assert function(ctx, wire, length, size) == lib.HIL_APPLICATION_STATUS_UNINITIALIZED
        assert size[0] == 0
    assert_decode_failure(ctx, wire, length, lib.HIL_APPLICATION_STATUS_UNINITIALIZED)
