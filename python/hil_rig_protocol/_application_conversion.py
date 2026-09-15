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
    ApplicationErrorMessage,
    ApplicationResponse,
    BusRole,
    CANConfig,
    CapturedRecord,
    ControlCommand,
    DigitalInputConfig,
    DigitalInputValue,
    DigitalOutputConfig,
    DigitalOutputValue,
    ErrorCategory,
    ExecutionControl,
    GlobalControl,
    GlobalControlCommand,
    I2CConfig,
    I2CPullUp,
    I2CVoltage,
    LogicalOperation,
    PeripheralType,
    PeripheralVoltage,
    ProtocolVersion,
    PWMInputConfig,
    PWMInputValue,
    PWMOutputConfig,
    PWMOutputValue,
    ResponseOutcome,
    ResponseReason,
    ResponseScope,
    ResultCondition,
    SPIBitOrder,
    SPIClockPhase,
    SPIClockPolarity,
    SPIConfig,
    SPIDataWidth,
    SystemInfoQuery,
    SystemInfoRequest,
    SystemInfoResponse,
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
    UpdateInstruction,
    VariableTestResult,
)
from .errors import ApplicationBindingError

_T = TypeVar("_T")


def _write_aligned_records(
    values: tuple[Any, ...], ctype: str, span_name: str
) -> tuple[Any, list[Any]]:
    if not values:
        return _binding.ffi.NULL, []
    records = _binding.ffi.new(f"{ctype}[]", len(values))
    owners = [records]
    for value, record in zip(values, records, strict=True):
        record.peripheral_type = value.peripheral_type
        record.channel = value.channel
        payload = getattr(value, span_name)
        span = getattr(record, span_name)
        span.size = len(payload)
        if payload:
            owner = _binding.ffi.new("uint8_t[]", payload)
            span.data = owner
            owners.append(owner)
    return records, owners


def _write_update_instruction(value: UpdateInstruction, native: Any) -> list[Any]:
    native.tick_number = value.tick_number
    native.flags = value.flags
    native.operation_count = len(value.operations)
    native.operations, owners = _write_aligned_records(
        value.operations, "HIL_Application_Logical_Operation_T", "payload"
    )
    return owners


def _write_variable_test_result(value: VariableTestResult, native: Any) -> list[Any]:
    native.tick_number = value.tick_number
    native.flags = value.flags
    native.condition = value.condition
    native.problem_detail = value.problem_detail
    native.record_count = len(value.records)
    native.records, owners = _write_aligned_records(
        value.records, "HIL_Application_Captured_Record_T", "data"
    )
    return owners


def _read_aligned_records(
    records: Any, count: int, ctype: str, span_name: str, storage: Any, capacity: int
) -> tuple[tuple[PeripheralType, int, bytes], ...]:
    """Check descriptor and payload ownership before dereferencing native pointers."""
    ffi = _binding.ffi
    if count == 0:
        if capacity != 0 or records != ffi.NULL:
            raise ApplicationBindingError("native empty records disagree with decode storage")
        return ()
    alignment = int(_binding.lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT)
    offset = ((count * ffi.sizeof(ctype) + alignment - 1) // alignment) * alignment
    if storage == ffi.NULL or ffi.cast("uint8_t *", records) != storage or offset > capacity:
        raise ApplicationBindingError("native record array disagrees with decode storage")
    result = []
    for i in range(count):
        record = records[i]
        span = getattr(record, span_name)
        size = int(span.size)
        if size == 0 or offset + size > capacity or span.data != storage + offset:
            raise ApplicationBindingError("native record span disagrees with decode storage")
        result.append(
            (
                PeripheralType(record.peripheral_type),
                int(record.channel),
                bytes(ffi.buffer(span.data, size)),
            )
        )
        offset += size
    if offset != capacity:
        raise ApplicationBindingError("native records do not fill decode storage")
    return tuple(result)


def _read_update_instruction(
    test_id: TestId, native: Any, storage: Any, capacity: int
) -> UpdateInstruction:
    if native.operation_count == 0:
        raise ApplicationBindingError("native update instruction has no operations")
    records = _read_aligned_records(
        native.operations,
        int(native.operation_count),
        "HIL_Application_Logical_Operation_T",
        "payload",
        storage,
        capacity,
    )
    return UpdateInstruction(
        test_id=test_id,
        tick_number=int(native.tick_number),
        flags=int(native.flags),
        operations=tuple(LogicalOperation(*record) for record in records),
    )


def _read_variable_test_result(
    test_id: TestId, native: Any, storage: Any, capacity: int
) -> VariableTestResult:
    records = _read_aligned_records(
        native.records,
        int(native.record_count),
        "HIL_Application_Captured_Record_T",
        "data",
        storage,
        capacity,
    )
    return VariableTestResult(
        test_id=test_id,
        tick_number=int(native.tick_number),
        flags=int(native.flags),
        condition=ResultCondition(native.condition),
        problem_detail=int(native.problem_detail),
        records=tuple(CapturedRecord(*record) for record in records),
    )


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


def _write_system_info_request(value: SystemInfoRequest, native: Any) -> list[Any]:
    """Populate a discovery request; its exact-version rule remains in C."""
    native.request_firmware_git_hash = int(value.request_firmware_git_hash)
    native.query = value.query
    native.application_protocol_major = value.protocol_version.major
    native.application_protocol_minor = value.protocol_version.minor
    native.application_protocol_patch = value.protocol_version.patch
    return []


def _read_system_info_request(native: Any) -> SystemInfoRequest:
    return SystemInfoRequest(
        query=SystemInfoQuery(native.query),
        request_firmware_git_hash=_read_bool(native.request_firmware_git_hash),
        protocol_version=ProtocolVersion(
            int(native.application_protocol_major),
            int(native.application_protocol_minor),
            int(native.application_protocol_patch),
        ),
    )


def _write_system_info_response(value: SystemInfoResponse, native: Any) -> list[Any]:
    """Populate a discovery response and retain borrowed spans for native encoding."""
    owners: list[Any] = []
    native.application_protocol_major = value.protocol_version.major
    native.application_protocol_minor = value.protocol_version.minor
    native.application_protocol_patch = value.protocol_version.patch
    native.firmware_version_major = value.firmware_version.major
    native.firmware_version_minor = value.firmware_version.minor
    native.firmware_version_patch = value.firmware_version.patch
    if value.diagnostic_data:
        diagnostic_owner = _binding.ffi.new("uint8_t[]", value.diagnostic_data)
        native.diagnostic_data.data = diagnostic_owner
        owners.append(diagnostic_owner)
    native.diagnostic_data.size = len(value.diagnostic_data)
    if value.firmware_git_hash:
        hash_owner = _binding.ffi.new("uint8_t[]", value.firmware_git_hash)
        native.firmware_git_hash.data = hash_owner
        owners.append(hash_owner)
    native.firmware_git_hash.size = len(value.firmware_git_hash)
    return owners


def _read_system_info_response(
    native: Any, diagnostic_data: bytes, firmware_git_hash: bytes
) -> SystemInfoResponse:
    return SystemInfoResponse(
        protocol_version=ProtocolVersion(
            int(native.application_protocol_major),
            int(native.application_protocol_minor),
            int(native.application_protocol_patch),
        ),
        firmware_version=ProtocolVersion(
            int(native.firmware_version_major),
            int(native.firmware_version_minor),
            int(native.firmware_version_patch),
        ),
        diagnostic_data=diagnostic_data,
        firmware_git_hash=firmware_git_hash,
    )


def _write_execution_control(value: ExecutionControl, native: Any) -> list[Any]:
    native.command = value.command
    native.flags = value.flags
    return []


def _read_execution_control(test_id: TestId, native: Any) -> ExecutionControl:
    return ExecutionControl(
        test_id=test_id,
        command=ControlCommand(native.command),
        flags=int(native.flags),
    )


def _write_global_control(value: GlobalControl, native: Any) -> list[Any]:
    native.command = value.command
    native.flags = value.flags
    return []


def _read_global_control(native: Any) -> GlobalControl:
    return GlobalControl(command=GlobalControlCommand(native.command), flags=int(native.flags))


def _write_response(value: ApplicationResponse, native: Any) -> list[Any]:
    native.scope = value.scope
    native.outcome = value.outcome
    native.reason = value.reason
    native.tick_number = value.tick_number
    native.control_command = value.control_command
    native.global_control_command = value.global_control_command
    native.detail = value.detail
    return []


def _read_response(test_id: TestId | None, native: Any) -> ApplicationResponse:
    return ApplicationResponse(
        test_id=test_id,
        scope=ResponseScope(native.scope),
        outcome=ResponseOutcome(native.outcome),
        reason=ResponseReason(native.reason),
        tick_number=int(native.tick_number),
        control_command=ControlCommand(native.control_command),
        global_control_command=GlobalControlCommand(native.global_control_command),
        detail=int(native.detail),
    )


def _write_error(value: ApplicationErrorMessage, native: Any) -> list[Any]:
    owners: list[Any] = []
    native.category = value.category
    native.recoverable = int(value.recoverable)
    native.has_tick_number = int(value.tick_number is not None)
    native.tick_number = 0 if value.tick_number is None else value.tick_number
    native.detail = value.detail
    if value.diagnostic_data:
        diagnostic_owner = _binding.ffi.new("uint8_t[]", value.diagnostic_data)
        native.diagnostic_data.data = diagnostic_owner
        owners.append(diagnostic_owner)
    native.diagnostic_data.size = len(value.diagnostic_data)
    return owners


def _read_error(
    test_id: TestId | None, native: Any, diagnostic_data: bytes
) -> ApplicationErrorMessage:
    tick_number = int(native.tick_number) if _read_bool(native.has_tick_number) else None
    return ApplicationErrorMessage(
        test_id=test_id,
        category=ErrorCategory(native.category),
        recoverable=_read_bool(native.recoverable),
        tick_number=tick_number,
        detail=int(native.detail),
        diagnostic_data=diagnostic_data,
    )


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
