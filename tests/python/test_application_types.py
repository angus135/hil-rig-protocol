"""Application representation contract and compiled native parity."""

from __future__ import annotations

from dataclasses import FrozenInstanceError, fields, replace
from enum import IntEnum

import hil_rig_protocol as p
import pytest
from application_fixtures import configuration, instruction, result
from hil_rig_protocol import _binding

ENUMS = {
    p.ApplicationStatus: (
        "HIL_Application_Status_T",
        {
            "OK": "HIL_APPLICATION_STATUS_OK",
            "INVALID_ARGUMENT": "HIL_APPLICATION_STATUS_INVALID_ARGUMENT",
            "UNINITIALIZED": "HIL_APPLICATION_STATUS_UNINITIALIZED",
            "BUFFER_TOO_SMALL": "HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL",
            "INVALID_MESSAGE_TYPE": "HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE",
            "INVALID_SUBTYPE": "HIL_APPLICATION_STATUS_INVALID_SUBTYPE",
            "MALFORMED_MESSAGE": "HIL_APPLICATION_STATUS_MALFORMED_MESSAGE",
            "TRUNCATED_MESSAGE": "HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE",
            "INVALID_LENGTH": "HIL_APPLICATION_STATUS_INVALID_LENGTH",
            "INVALID_COUNT": "HIL_APPLICATION_STATUS_INVALID_COUNT",
            "UNSUPPORTED_MESSAGE": "HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE",
            "INCONSISTENT_TEST_ID": "HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID",
            "INCONSISTENT_TICK": "HIL_APPLICATION_STATUS_INCONSISTENT_TICK",
            "INCOMPLETE_DATA": "HIL_APPLICATION_STATUS_INCOMPLETE_DATA",
            "VALIDATION_FAILED": "HIL_APPLICATION_STATUS_VALIDATION_FAILED",
            "NOT_IMPLEMENTED": "HIL_APPLICATION_STATUS_NOT_IMPLEMENTED",
            "INTERNAL_ERROR": "HIL_APPLICATION_STATUS_INTERNAL_ERROR",
        },
    ),
    p.PeripheralVoltage: (
        "HIL_Application_Peripheral_Config_Voltage_Level_T",
        {
            "INVALID": "HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID",
            "V_3V3": "HIL_APPLICATION_PERIPHERAL_CONFIG_3V3",
            "V_5V": "HIL_APPLICATION_PERIPHERAL_CONFIG_5V",
            "V_12V": "HIL_APPLICATION_PERIPHERAL_CONFIG_12V",
            "V_24V": "HIL_APPLICATION_PERIPHERAL_CONFIG_24V",
            "RESERVED": "HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED",
        },
    ),
    p.BusRole: (
        "HIL_Application_Bus_Role_T",
        {
            "INVALID": "HIL_APPLICATION_BUS_ROLE_INVALID",
            "MASTER": "HIL_APPLICATION_BUS_ROLE_MASTER",
            "SLAVE": "HIL_APPLICATION_BUS_ROLE_SLAVE",
            "RESERVED": "HIL_APPLICATION_BUS_ROLE_RESERVED",
        },
    ),
    p.SPIDataWidth: (
        "HIL_Application_Spi_Data_Width_T",
        {
            "INVALID": "HIL_APPLICATION_SPI_DATA_WIDTH_INVALID",
            "BITS_8": "HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS",
            "BITS_16": "HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS",
            "RESERVED": "HIL_APPLICATION_SPI_DATA_WIDTH_RESERVED",
        },
    ),
    p.SPIBitOrder: (
        "HIL_Application_Spi_Bit_Order_T",
        {
            "INVALID": "HIL_APPLICATION_SPI_BIT_ORDER_INVALID",
            "MSB_FIRST": "HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST",
            "LSB_FIRST": "HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST",
            "RESERVED": "HIL_APPLICATION_SPI_BIT_ORDER_RESERVED",
        },
    ),
    p.SPIClockPolarity: (
        "HIL_Application_Spi_Clock_Polarity_T",
        {
            "INVALID": "HIL_APPLICATION_SPI_CLOCK_POLARITY_INVALID",
            "IDLE_LOW": "HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW",
            "IDLE_HIGH": "HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH",
            "RESERVED": "HIL_APPLICATION_SPI_CLOCK_POLARITY_RESERVED",
        },
    ),
    p.SPIClockPhase: (
        "HIL_Application_Spi_Clock_Phase_T",
        {
            "INVALID": "HIL_APPLICATION_SPI_CLOCK_PHASE_INVALID",
            "FIRST_EDGE": "HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE",
            "SECOND_EDGE": "HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE",
            "RESERVED": "HIL_APPLICATION_SPI_CLOCK_PHASE_RESERVED",
        },
    ),
    p.UARTElectricalMode: (
        "HIL_Application_Uart_Electrical_Mode_T",
        {
            "INVALID": "HIL_APPLICATION_UART_ELECTRICAL_MODE_INVALID",
            "TTL_3V3": "HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3",
            "TTL_5V": "HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V",
            "RS232": "HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232",
            "RESERVED": "HIL_APPLICATION_UART_ELECTRICAL_MODE_RESERVED",
        },
    ),
    p.UARTWordLength: (
        "HIL_Application_Uart_Word_Length_T",
        {
            "INVALID": "HIL_APPLICATION_UART_WORD_LENGTH_INVALID",
            "BITS_8": "HIL_APPLICATION_UART_WORD_LENGTH_8_BITS",
            "BITS_9": "HIL_APPLICATION_UART_WORD_LENGTH_9_BITS",
            "RESERVED": "HIL_APPLICATION_UART_WORD_LENGTH_RESERVED",
        },
    ),
    p.UARTParity: (
        "HIL_Application_Uart_Parity_T",
        {
            "INVALID": "HIL_APPLICATION_UART_PARITY_INVALID",
            "NONE": "HIL_APPLICATION_UART_PARITY_NONE",
            "EVEN": "HIL_APPLICATION_UART_PARITY_EVEN",
            "ODD": "HIL_APPLICATION_UART_PARITY_ODD",
            "RESERVED": "HIL_APPLICATION_UART_PARITY_RESERVED",
        },
    ),
    p.UARTStopBits: (
        "HIL_Application_Uart_Stop_Bits_T",
        {
            "INVALID": "HIL_APPLICATION_UART_STOP_BITS_INVALID",
            "BITS_1": "HIL_APPLICATION_UART_STOP_BITS_1",
            "BITS_2": "HIL_APPLICATION_UART_STOP_BITS_2",
            "RESERVED": "HIL_APPLICATION_UART_STOP_BITS_RESERVED",
        },
    ),
    p.I2CVoltage: (
        "HIL_Application_I2c_Voltage_Level_T",
        {
            "INVALID": "HIL_APPLICATION_I2C_VOLTAGE_INVALID",
            "V_3V3": "HIL_APPLICATION_I2C_VOLTAGE_3V3",
            "V_5V": "HIL_APPLICATION_I2C_VOLTAGE_5V",
            "RESERVED": "HIL_APPLICATION_I2C_VOLTAGE_RESERVED",
        },
    ),
    p.I2CPullUp: (
        "HIL_Application_I2c_Pull_Up_T",
        {
            "INVALID": "HIL_APPLICATION_I2C_PULL_UP_INVALID",
            "OHM_1K": "HIL_APPLICATION_I2C_PULL_UP_1K",
            "OHM_2K2": "HIL_APPLICATION_I2C_PULL_UP_2K2",
            "OHM_4K7": "HIL_APPLICATION_I2C_PULL_UP_4K7",
            "OHM_10K": "HIL_APPLICATION_I2C_PULL_UP_10K",
            "RESERVED": "HIL_APPLICATION_I2C_PULL_UP_RESERVED",
        },
    ),
    p.ResultCondition: (
        "HIL_Application_Result_Condition_T",
        {
            "OK": "HIL_APPLICATION_RESULT_CONDITION_OK",
            "PARTIAL": "HIL_APPLICATION_RESULT_CONDITION_PARTIAL",
            "EXECUTION_PROBLEM": "HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM",
            "RESERVED": "HIL_APPLICATION_RESULT_CONDITION_RESERVED",
        },
    ),
}

NATIVE_RECORDS = {
    p.TestId: "HIL_Application_Test_Id_T",
    p.TickDuration: "HIL_Application_Tick_Duration_T",
    p.DigitalOutputValue: "HIL_Application_Digital_Output_Value_T",
    p.DigitalInputValue: "HIL_Application_Digital_Input_Value_T",
    p.AnalogOutputValue: "HIL_Application_Analog_Output_Value_T",
    p.AnalogInputValue: "HIL_Application_Analog_Input_Value_T",
    p.PWMOutputValue: "HIL_Application_Pwm_Output_Value_T",
    p.PWMInputValue: "HIL_Application_Pwm_Input_Value_T",
    p.DigitalInputConfig: "HIL_Application_Digital_Input_Config_T",
    p.DigitalOutputConfig: "HIL_Application_Digital_Output_Config_T",
    p.AnalogInputConfig: "HIL_Application_Analog_Input_Config_T",
    p.AnalogOutputConfig: "HIL_Application_Analog_Output_Config_T",
    p.PWMInputConfig: "HIL_Application_Pwm_Input_Config_T",
    p.PWMOutputConfig: "HIL_Application_Pwm_Output_Config_T",
    p.CANConfig: "HIL_Application_Can_Config_T",
    p.SPIConfig: "HIL_Application_Spi_Config_T",
    p.UARTConfig: "HIL_Application_Uart_Config_T",
    p.I2CConfig: "HIL_Application_I2c_Config_T",
    p.ApplicationConfig: "HIL_Application_Config_T",
    p.TestConfiguration: "HIL_Application_Test_Configuration_T",
    p.TestInstruction: "HIL_Application_Test_Instruction_T",
    p.TestResult: "HIL_Application_Test_Result_T",
}


def record_cases():
    messages = [configuration(), instruction(), result()]
    values = [p.ApplicationConfig(), messages[0].test_id, messages[0].tick_duration_us, *messages]
    for message in messages:
        for field in fields(message):
            value = getattr(message, field.name)
            if type(value) is tuple:
                values.extend(value)
    # One populated instance of every public record is enough for representation checks.
    return list({type(value): value for value in values}.values())


@pytest.mark.parametrize("enum,contract", ENUMS.items())
def test_all_enum_members_match_compiled_native(enum, contract):
    native_name, members = contract
    assert issubclass(enum, IntEnum)
    assert set(enum.__members__) == set(members)
    assert {int(value) for value in enum} == set(_binding.ffi.typeof(native_name).elements)
    for name, constant in members.items():
        assert int(enum[name]) == getattr(_binding.lib, constant)


@pytest.mark.parametrize("value", record_cases(), ids=lambda v: type(v).__name__)
def test_records_are_frozen_slotted_and_cover_native_fields(value):
    assert not hasattr(value, "__dict__")
    field = fields(value)[0]
    with pytest.raises(FrozenInstanceError):
        setattr(value, field.name, getattr(value, field.name))
    with pytest.raises((TypeError, AttributeError)):
        value.extra = 1
    native_fields = dict(_binding.ffi.typeof(NATIVE_RECORDS[type(value)]).fields)
    public_fields = {field.name for field in fields(value)}
    if type(value) in (p.TestConfiguration, p.TestInstruction, p.TestResult):
        public_fields.remove("test_id")
    assert public_fields == set(native_fields)


@pytest.mark.parametrize("value", record_cases(), ids=lambda v: type(v).__name__)
def test_every_field_representation(value):
    native_fields = dict(_binding.ffi.typeof(NATIVE_RECORDS[type(value)]).fields)
    for field in fields(value):
        current = getattr(value, field.name)
        if type(current) is int:
            width = _binding.ffi.sizeof(native_fields[field.name].type)
            maximum = (1 << (8 * width)) - 1
            for valid in (0, maximum):
                assert getattr(replace(value, **{field.name: valid}), field.name) == valid
            for invalid in (-1, maximum + 1):
                with pytest.raises(ValueError):
                    replace(value, **{field.name: invalid})
            for invalid in (True, False, 1.0, "1", p.ApplicationStatus.OK):
                with pytest.raises(TypeError):
                    replace(value, **{field.name: invalid})
        elif type(current) is bool:
            for invalid in (0, 1, 2, None, 1.0, p.ApplicationStatus.OK):
                with pytest.raises(TypeError):
                    replace(value, **{field.name: invalid})
        elif isinstance(current, IntEnum):
            for invalid in (int(current), True, p.TransportStatus.OK, 1.0):
                with pytest.raises(TypeError):
                    replace(value, **{field.name: invalid})
        elif type(current) is tuple:
            assert len(current) == native_fields[field.name].type.length
            for invalid in (current[:-1], current + current[:1]):
                with pytest.raises(ValueError):
                    replace(value, **{field.name: invalid})
            for invalid in (list(current), None, (None,) * len(current)):
                with pytest.raises(TypeError):
                    replace(value, **{field.name: invalid})
            wrong_record = (
                p.AnalogInputConfig()
                if type(current[0]) is not p.AnalogInputConfig
                else p.AnalogOutputConfig()
            )
            with pytest.raises(TypeError):
                replace(value, **{field.name: (wrong_record,) * len(current)})
        elif type(current) is bytes:
            for invalid in (bytearray(current), memoryview(current), list(current), ""):
                with pytest.raises(TypeError):
                    replace(value, **{field.name: invalid})
        else:
            for invalid in (None, 0, b"", object()):
                with pytest.raises(TypeError):
                    replace(value, **{field.name: invalid})


@pytest.mark.parametrize("size", [0, 1, 15, 17, 255])
def test_test_id_is_exactly_sixteen_bytes(size):
    with pytest.raises(ValueError):
        p.TestId(bytes(size))


def test_bytes_subclasses_are_rejected():
    class Bytes(bytes):
        pass

    with pytest.raises(TypeError):
        p.TestId(Bytes(16))
    with pytest.raises(TypeError):
        configuration(Bytes(4))


def test_extension_length_and_all_zero_identifier():
    assert p.TestId(bytes(16)).bytes == bytes(16)
    for size in (0, 3, 255):
        assert configuration(bytes(size)).extension_data == bytes(size)
    with pytest.raises(ValueError):
        configuration(bytes(256))


def test_configuration_defaults_match_compiled_native():
    native = _binding.ffi.new("HIL_Application_Config_T *")
    assert _binding.lib.HIL_APPLICATION_Default_Config(native) == p.ApplicationStatus.OK
    config = p.ApplicationConfig()
    for field in fields(config):
        assert getattr(config, field.name) == getattr(native, field.name)


def test_semantically_invalid_values_can_be_represented():
    assert p.TickDuration(999).microseconds == 999
    assert p.PWMOutputValue(0, 65535).duty_cycle_permyriad == 65535
    assert p.PWMInputValue(0, 1).period_nanoseconds == 0
    assert p.DigitalOutputConfig(False, p.PeripheralVoltage.RESERVED, True).initial_high
    assert p.UARTConfig(enabled=True).rx_enabled is False
    assert p.I2CConfig(own_address_7bit=65535).own_address_7bit == 65535
    assert replace(configuration(), flags=1, expected_tick_count=0).flags == 1
    assert (
        replace(result(), condition=p.ResultCondition.RESERVED).condition
        is p.ResultCondition.RESERVED
    )
    assert p.ApplicationConfig(0, 0, 0, 0).max_encoded_message_size == 0


def test_exact_records_and_tuples_reject_subclasses():
    class Digital(p.DigitalOutputValue):
        pass

    class Tuple(tuple):
        pass

    with pytest.raises(TypeError):
        replace(instruction(), digital_outputs=(Digital(),) * 10)
    with pytest.raises(TypeError):
        replace(instruction(), digital_outputs=Tuple(instruction().digital_outputs))
