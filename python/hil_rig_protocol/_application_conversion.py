"""Private native structure conversions, grouped by Application message family."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import fields
from typing import Any, TypeVar

from . import _binding
from .application_types import (
    AnalogInputConfig,
    AnalogInputValue,
    AnalogOutputConfig,
    AnalogOutputValue,
    ApplicationConfig,
    BusRole,
    CANConfig,
    DigitalInputConfig,
    DigitalInputValue,
    DigitalOutputConfig,
    DigitalOutputValue,
    I2CConfig,
    I2CPullUp,
    I2CVoltage,
    PeripheralVoltage,
    PWMInputConfig,
    PWMInputValue,
    PWMOutputConfig,
    PWMOutputValue,
    ResultCondition,
    SPIBitOrder,
    SPIClockPhase,
    SPIClockPolarity,
    SPIConfig,
    SPIDataWidth,
    TestConfiguration,
    TestId,
    TestInstruction,
    TestResult,
    TickDuration,
    UARTConfig,
    UARTElectricalMode,
    UARTParity,
    UARTStopBits,
    UARTWordLength,
)
from .errors import ApplicationBindingError

_T = TypeVar("_T")


def _write_record(value: Any, native: Any) -> None:
    """Copy scalar record fields; native field names deliberately match Python."""
    for field in fields(value):
        setattr(native, field.name, getattr(value, field.name))


def _write_fixed(values: tuple[Any, ...], native: Any) -> None:
    if len(values) != len(native):
        raise ApplicationBindingError("compiled fixed-array extent disagrees with Python")
    for value, destination in zip(values, native, strict=True):
        _write_record(value, destination)


def _read_fixed(native: Any, convert: Callable[[Any], _T]) -> tuple[_T, ...]:
    return tuple(convert(value) for value in native)


def _read_bool(value: int) -> bool:
    if value not in (0, 1):
        raise ApplicationBindingError("native decoder returned an invalid boolean")
    return bool(value)


def _build_native_config(config: ApplicationConfig) -> Any:
    native = _binding.ffi.new("HIL_Application_Config_T *")
    _write_record(config, native)
    return native


def _read_digital_output_value(native: Any) -> DigitalOutputValue:
    return DigitalOutputValue(
        high=_read_bool(native.high),
    )


def _read_digital_input_value(native: Any) -> DigitalInputValue:
    return DigitalInputValue(
        high=_read_bool(native.high),
    )


def _read_analog_output_value(native: Any) -> AnalogOutputValue:
    return AnalogOutputValue(
        microvolts=int(native.microvolts),
    )


def _read_analog_input_value(native: Any) -> AnalogInputValue:
    return AnalogInputValue(
        microvolts=int(native.microvolts),
    )


def _read_pwm_output_value(native: Any) -> PWMOutputValue:
    return PWMOutputValue(
        period_nanoseconds=int(native.period_nanoseconds),
        duty_cycle_permyriad=int(native.duty_cycle_permyriad),
    )


def _read_pwm_input_value(native: Any) -> PWMInputValue:
    return PWMInputValue(
        period_nanoseconds=int(native.period_nanoseconds),
        duty_cycle_permyriad=int(native.duty_cycle_permyriad),
    )


def _read_digital_input_config(native: Any) -> DigitalInputConfig:
    return DigitalInputConfig(
        enabled=_read_bool(native.enabled),
        voltage_level=PeripheralVoltage(native.voltage_level),
    )


def _read_digital_output_config(native: Any) -> DigitalOutputConfig:
    return DigitalOutputConfig(
        enabled=_read_bool(native.enabled),
        voltage_level=PeripheralVoltage(native.voltage_level),
        initial_high=_read_bool(native.initial_high),
    )


def _read_analog_input_config(native: Any) -> AnalogInputConfig:
    return AnalogInputConfig(
        enabled=_read_bool(native.enabled),
    )


def _read_analog_output_config(native: Any) -> AnalogOutputConfig:
    return AnalogOutputConfig(
        enabled=_read_bool(native.enabled),
    )


def _read_pwm_input_config(native: Any) -> PWMInputConfig:
    return PWMInputConfig(
        enabled=_read_bool(native.enabled),
        voltage_level=PeripheralVoltage(native.voltage_level),
    )


def _read_pwm_output_config(native: Any) -> PWMOutputConfig:
    return PWMOutputConfig(
        enabled=_read_bool(native.enabled),
        voltage_level=PeripheralVoltage(native.voltage_level),
        initial_period_nanoseconds=int(native.initial_period_nanoseconds),
        initial_duty_cycle_permyriad=int(native.initial_duty_cycle_permyriad),
    )


def _read_can_config(native: Any) -> CANConfig:
    return CANConfig(
        enabled=_read_bool(native.enabled),
        bit_rate=int(native.bit_rate),
        capture_limit_bytes=int(native.capture_limit_bytes),
        filter_id=int(native.filter_id),
        filter_mask=int(native.filter_mask),
    )


def _read_spi_config(native: Any) -> SPIConfig:
    return SPIConfig(
        enabled=_read_bool(native.enabled),
        bit_rate=int(native.bit_rate),
        role=BusRole(native.role),
        data_width=SPIDataWidth(native.data_width),
        bit_order=SPIBitOrder(native.bit_order),
        clock_polarity=SPIClockPolarity(native.clock_polarity),
        clock_phase=SPIClockPhase(native.clock_phase),
        capture_limit_bytes=int(native.capture_limit_bytes),
    )


def _read_uart_config(native: Any) -> UARTConfig:
    return UARTConfig(
        enabled=_read_bool(native.enabled),
        baud_rate=int(native.baud_rate),
        electrical_mode=UARTElectricalMode(native.electrical_mode),
        word_length=UARTWordLength(native.word_length),
        parity=UARTParity(native.parity),
        stop_bits=UARTStopBits(native.stop_bits),
        rx_enabled=_read_bool(native.rx_enabled),
        tx_enabled=_read_bool(native.tx_enabled),
        capture_limit_bytes=int(native.capture_limit_bytes),
    )


def _read_i2c_config(native: Any) -> I2CConfig:
    return I2CConfig(
        enabled=_read_bool(native.enabled),
        bit_rate=int(native.bit_rate),
        role=BusRole(native.role),
        own_address_7bit=int(native.own_address_7bit),
        voltage_level=I2CVoltage(native.voltage_level),
        pull_up=I2CPullUp(native.pull_up),
        capture_limit_bytes=int(native.capture_limit_bytes),
    )


def _write_test_configuration(value: TestConfiguration, native: Any) -> list[Any]:
    """Populate this body and return owners of every synchronously borrowed span."""
    owners: list[Any] = []
    _write_record(value.tick_duration_us, native.tick_duration_us)
    native.expected_tick_count = value.expected_tick_count
    native.flags = value.flags
    _write_fixed(value.digital_in, native.digital_in)
    _write_fixed(value.digital_out, native.digital_out)
    _write_fixed(value.analog_in, native.analog_in)
    _write_fixed(value.analog_out, native.analog_out)
    _write_fixed(value.pwm_in, native.pwm_in)
    _write_fixed(value.pwm_out, native.pwm_out)
    _write_fixed(value.can, native.can)
    _write_fixed(value.spi, native.spi)
    _write_fixed(value.uart, native.uart)
    _write_fixed(value.i2c, native.i2c)
    if value.extension_data:
        owner = _binding.ffi.new("uint8_t[]", value.extension_data)
        owners.append(owner)
        native.extension_data.data = owner
    native.extension_data.size = len(value.extension_data)
    return owners


def _read_test_configuration(test_id: TestId, native: Any) -> TestConfiguration:
    return TestConfiguration(
        test_id=test_id,
        tick_duration_us=TickDuration(int(native.tick_duration_us.microseconds)),
        expected_tick_count=int(native.expected_tick_count),
        flags=int(native.flags),
        digital_in=_read_fixed(native.digital_in, _read_digital_input_config),
        digital_out=_read_fixed(native.digital_out, _read_digital_output_config),
        analog_in=_read_fixed(native.analog_in, _read_analog_input_config),
        analog_out=_read_fixed(native.analog_out, _read_analog_output_config),
        pwm_in=_read_fixed(native.pwm_in, _read_pwm_input_config),
        pwm_out=_read_fixed(native.pwm_out, _read_pwm_output_config),
        can=_read_fixed(native.can, _read_can_config),
        spi=_read_fixed(native.spi, _read_spi_config),
        uart=_read_fixed(native.uart, _read_uart_config),
        i2c=_read_fixed(native.i2c, _read_i2c_config),
        extension_data=bytes(
            _binding.ffi.buffer(native.extension_data.data, native.extension_data.size)
        )
        if native.extension_data.size
        else b"",
    )


def _write_test_instruction(value: TestInstruction, native: Any) -> list[Any]:
    """Populate this body and return owners of every synchronously borrowed span."""
    owners: list[Any] = []
    native.tick_number = value.tick_number
    _write_fixed(value.digital_outputs, native.digital_outputs)
    _write_fixed(value.analog_outputs, native.analog_outputs)
    _write_fixed(value.pwm_outputs, native.pwm_outputs)
    return owners


def _read_test_instruction(test_id: TestId, native: Any) -> TestInstruction:
    return TestInstruction(
        test_id=test_id,
        tick_number=int(native.tick_number),
        digital_outputs=_read_fixed(native.digital_outputs, _read_digital_output_value),
        analog_outputs=_read_fixed(native.analog_outputs, _read_analog_output_value),
        pwm_outputs=_read_fixed(native.pwm_outputs, _read_pwm_output_value),
    )


def _write_test_result(value: TestResult, native: Any) -> list[Any]:
    """Populate this body and return owners of every synchronously borrowed span."""
    owners: list[Any] = []
    native.tick_number = value.tick_number
    _write_fixed(value.digital_inputs, native.digital_inputs)
    _write_fixed(value.analog_inputs, native.analog_inputs)
    _write_fixed(value.pwm_inputs, native.pwm_inputs)
    native.condition = value.condition
    native.problem_detail = value.problem_detail
    return owners


def _read_test_result(test_id: TestId, native: Any) -> TestResult:
    return TestResult(
        test_id=test_id,
        tick_number=int(native.tick_number),
        digital_inputs=_read_fixed(native.digital_inputs, _read_digital_input_value),
        analog_inputs=_read_fixed(native.analog_inputs, _read_analog_input_value),
        pwm_inputs=_read_fixed(native.pwm_inputs, _read_pwm_input_value),
        condition=ResultCondition(native.condition),
        problem_detail=int(native.problem_detail),
    )
