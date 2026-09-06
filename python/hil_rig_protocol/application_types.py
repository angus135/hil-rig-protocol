"""Immutable Application values; Python checks representation, C checks semantics."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

from . import _binding
from .transport_types import _validate_integer

_SIZE_MAX = (1 << (8 * _binding.ffi.sizeof("size_t"))) - 1
_UINT32_MAX = (1 << 32) - 1
_UINT16_MAX = (1 << 16) - 1


class ApplicationStatus(IntEnum):
    """Local status returned by the native Application facade."""

    OK = 0
    INVALID_ARGUMENT = 1
    UNINITIALIZED = 2
    BUFFER_TOO_SMALL = 3
    INVALID_MESSAGE_TYPE = 4
    INVALID_SUBTYPE = 5
    MALFORMED_MESSAGE = 6
    TRUNCATED_MESSAGE = 7
    INVALID_LENGTH = 8
    INVALID_COUNT = 9
    UNSUPPORTED_MESSAGE = 10
    INCONSISTENT_TEST_ID = 11
    INCONSISTENT_TICK = 12
    INCOMPLETE_DATA = 13
    VALIDATION_FAILED = 14
    NOT_IMPLEMENTED = 15
    INTERNAL_ERROR = 16


class PeripheralVoltage(IntEnum):
    """Native Peripheral Config Voltage Level selections, including sentinels."""

    INVALID = 0
    V_3V3 = 1
    V_5V = 2
    V_12V = 3
    V_24V = 4
    RESERVED = 255


class BusRole(IntEnum):
    """Native Bus Role selections, including sentinels."""

    INVALID = 0
    MASTER = 1
    SLAVE = 2
    RESERVED = 255


class SPIDataWidth(IntEnum):
    """Native SPI Data Width selections, including sentinels."""

    INVALID = 0
    BITS_8 = 1
    BITS_16 = 2
    RESERVED = 255


class SPIBitOrder(IntEnum):
    """Native SPI Bit Order selections, including sentinels."""

    INVALID = 0
    MSB_FIRST = 1
    LSB_FIRST = 2
    RESERVED = 255


class SPIClockPolarity(IntEnum):
    """Native SPI Clock Polarity selections, including sentinels."""

    INVALID = 0
    IDLE_LOW = 1
    IDLE_HIGH = 2
    RESERVED = 255


class SPIClockPhase(IntEnum):
    """Native SPI Clock Phase selections, including sentinels."""

    INVALID = 0
    FIRST_EDGE = 1
    SECOND_EDGE = 2
    RESERVED = 255


class UARTElectricalMode(IntEnum):
    """Native UART Electrical Mode selections, including sentinels."""

    INVALID = 0
    TTL_3V3 = 1
    TTL_5V = 2
    RS232 = 3
    RESERVED = 255


class UARTWordLength(IntEnum):
    """Native UART Word Length selections, including sentinels."""

    INVALID = 0
    BITS_8 = 1
    BITS_9 = 2
    RESERVED = 255


class UARTParity(IntEnum):
    """Native UART Parity selections, including sentinels."""

    INVALID = 0
    NONE = 1
    EVEN = 2
    ODD = 3
    RESERVED = 255


class UARTStopBits(IntEnum):
    """Native UART Stop Bits selections, including sentinels."""

    INVALID = 0
    BITS_1 = 1
    BITS_2 = 2
    RESERVED = 255


class I2CVoltage(IntEnum):
    """Native I2C Voltage Level selections, including sentinels."""

    INVALID = 0
    V_3V3 = 1
    V_5V = 2
    RESERVED = 255


class I2CPullUp(IntEnum):
    """Native I2C Pull Up selections, including sentinels."""

    INVALID = 0
    OHM_1K = 1
    OHM_2K2 = 2
    OHM_4K7 = 3
    OHM_10K = 4
    RESERVED = 255


class ResultCondition(IntEnum):
    """Native Result Condition selections, including sentinels."""

    OK = 0
    PARTIAL = 1
    EXECUTION_PROBLEM = 2
    RESERVED = 255


def _exact(name: str, value: object, expected: type) -> None:
    if type(value) is not expected:
        raise TypeError(f"{name} must be exactly {expected.__name__}")


def _fixed(name: str, value: object, element: type, count: int) -> None:
    _exact(name, value, tuple)
    assert isinstance(value, tuple)
    if len(value) != count:
        raise ValueError(f"{name} must contain exactly {count} elements")
    for item in value:
        _exact(name, item, element)


def _bytes(name: str, value: bytes, *, exact: int | None = None) -> None:
    _exact(name, value, bytes)
    if exact is not None and len(value) != exact:
        raise ValueError(f"{name} must contain exactly {exact} bytes")
    if exact is None and len(value) > 255:
        raise ValueError(f"{name} must contain at most 255 bytes")


@dataclass(frozen=True, slots=True)
class TestId:
    """Exactly 16 opaque immutable identifier bytes; allocation belongs to callers."""

    bytes: bytes

    def __post_init__(self) -> None:
        _bytes("bytes", self.bytes, exact=16)


@dataclass(frozen=True, slots=True)
class TickDuration:
    """Tick duration in unsigned 32-bit microseconds; C validates permitted durations."""

    microseconds: int = 0

    def __post_init__(self) -> None:
        _validate_integer("microseconds", self.microseconds, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class DigitalOutputValue:
    """Python-owned Digital Output Value record; no protocol validation."""

    high: bool = False

    def __post_init__(self) -> None:
        _exact("high", self.high, bool)


@dataclass(frozen=True, slots=True)
class DigitalInputValue:
    """Python-owned Digital Input Value record; no protocol validation."""

    high: bool = False

    def __post_init__(self) -> None:
        _exact("high", self.high, bool)


@dataclass(frozen=True, slots=True)
class AnalogOutputValue:
    """Python-owned Analog Output Value record; no protocol validation."""

    microvolts: int = 0

    def __post_init__(self) -> None:
        _validate_integer("microvolts", self.microvolts, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class AnalogInputValue:
    """Python-owned Analog Input Value record; no protocol validation."""

    microvolts: int = 0

    def __post_init__(self) -> None:
        _validate_integer("microvolts", self.microvolts, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class PWMOutputValue:
    """Python-owned PWM Output Value record; no protocol validation."""

    period_nanoseconds: int = 0
    duty_cycle_permyriad: int = 0

    def __post_init__(self) -> None:
        _validate_integer("period_nanoseconds", self.period_nanoseconds, 0, _UINT32_MAX)
        _validate_integer("duty_cycle_permyriad", self.duty_cycle_permyriad, 0, _UINT16_MAX)


@dataclass(frozen=True, slots=True)
class PWMInputValue:
    """Python-owned PWM Input Value record; no protocol validation."""

    period_nanoseconds: int = 0
    duty_cycle_permyriad: int = 0

    def __post_init__(self) -> None:
        _validate_integer("period_nanoseconds", self.period_nanoseconds, 0, _UINT32_MAX)
        _validate_integer("duty_cycle_permyriad", self.duty_cycle_permyriad, 0, _UINT16_MAX)


@dataclass(frozen=True, slots=True)
class DigitalInputConfig:
    """Python-owned Digital Input Config record; no protocol validation."""

    enabled: bool = False
    voltage_level: PeripheralVoltage = PeripheralVoltage.INVALID

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _exact("voltage_level", self.voltage_level, PeripheralVoltage)


@dataclass(frozen=True, slots=True)
class DigitalOutputConfig:
    """Python-owned Digital Output Config record; no protocol validation."""

    enabled: bool = False
    voltage_level: PeripheralVoltage = PeripheralVoltage.INVALID
    initial_high: bool = False

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _exact("voltage_level", self.voltage_level, PeripheralVoltage)
        _exact("initial_high", self.initial_high, bool)


@dataclass(frozen=True, slots=True)
class AnalogInputConfig:
    """Python-owned Analog Input Config record; no protocol validation."""

    enabled: bool = False

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)


@dataclass(frozen=True, slots=True)
class AnalogOutputConfig:
    """Python-owned Analog Output Config record; no protocol validation."""

    enabled: bool = False

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)


@dataclass(frozen=True, slots=True)
class PWMInputConfig:
    """Python-owned PWM Input Config record; no protocol validation."""

    enabled: bool = False
    voltage_level: PeripheralVoltage = PeripheralVoltage.INVALID

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _exact("voltage_level", self.voltage_level, PeripheralVoltage)


@dataclass(frozen=True, slots=True)
class PWMOutputConfig:
    """Python-owned PWM Output Config record; no protocol validation."""

    enabled: bool = False
    voltage_level: PeripheralVoltage = PeripheralVoltage.INVALID
    initial_period_nanoseconds: int = 0
    initial_duty_cycle_permyriad: int = 0

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _exact("voltage_level", self.voltage_level, PeripheralVoltage)
        _validate_integer(
            "initial_period_nanoseconds", self.initial_period_nanoseconds, 0, _UINT32_MAX
        )
        _validate_integer(
            "initial_duty_cycle_permyriad", self.initial_duty_cycle_permyriad, 0, _UINT16_MAX
        )


@dataclass(frozen=True, slots=True)
class CANConfig:
    """Python-owned CAN Config record; no protocol validation."""

    enabled: bool = False
    bit_rate: int = 0
    termination_enabled: bool = False
    capture_limit_bytes: int = 0

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _validate_integer("bit_rate", self.bit_rate, 0, _UINT32_MAX)
        _exact("termination_enabled", self.termination_enabled, bool)
        _validate_integer("capture_limit_bytes", self.capture_limit_bytes, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class SPIConfig:
    """Python-owned SPI Config record; no protocol validation."""

    enabled: bool = False
    bit_rate: int = 0
    role: BusRole = BusRole.INVALID
    data_width: SPIDataWidth = SPIDataWidth.INVALID
    bit_order: SPIBitOrder = SPIBitOrder.INVALID
    clock_polarity: SPIClockPolarity = SPIClockPolarity.INVALID
    clock_phase: SPIClockPhase = SPIClockPhase.INVALID
    capture_limit_bytes: int = 0

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _validate_integer("bit_rate", self.bit_rate, 0, _UINT32_MAX)
        _exact("role", self.role, BusRole)
        _exact("data_width", self.data_width, SPIDataWidth)
        _exact("bit_order", self.bit_order, SPIBitOrder)
        _exact("clock_polarity", self.clock_polarity, SPIClockPolarity)
        _exact("clock_phase", self.clock_phase, SPIClockPhase)
        _validate_integer("capture_limit_bytes", self.capture_limit_bytes, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class UARTConfig:
    """Python-owned UART Config record; no protocol validation."""

    enabled: bool = False
    baud_rate: int = 0
    electrical_mode: UARTElectricalMode = UARTElectricalMode.INVALID
    word_length: UARTWordLength = UARTWordLength.INVALID
    parity: UARTParity = UARTParity.INVALID
    stop_bits: UARTStopBits = UARTStopBits.INVALID
    rx_enabled: bool = False
    tx_enabled: bool = False
    capture_limit_bytes: int = 0

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _validate_integer("baud_rate", self.baud_rate, 0, _UINT32_MAX)
        _exact("electrical_mode", self.electrical_mode, UARTElectricalMode)
        _exact("word_length", self.word_length, UARTWordLength)
        _exact("parity", self.parity, UARTParity)
        _exact("stop_bits", self.stop_bits, UARTStopBits)
        _exact("rx_enabled", self.rx_enabled, bool)
        _exact("tx_enabled", self.tx_enabled, bool)
        _validate_integer("capture_limit_bytes", self.capture_limit_bytes, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class I2CConfig:
    """Python-owned I2C Config record; no protocol validation."""

    enabled: bool = False
    bit_rate: int = 0
    role: BusRole = BusRole.INVALID
    own_address_7bit: int = 0
    voltage_level: I2CVoltage = I2CVoltage.INVALID
    pull_up: I2CPullUp = I2CPullUp.INVALID
    capture_limit_bytes: int = 0

    def __post_init__(self) -> None:
        _exact("enabled", self.enabled, bool)
        _validate_integer("bit_rate", self.bit_rate, 0, _UINT32_MAX)
        _exact("role", self.role, BusRole)
        _validate_integer("own_address_7bit", self.own_address_7bit, 0, _UINT16_MAX)
        _exact("voltage_level", self.voltage_level, I2CVoltage)
        _exact("pull_up", self.pull_up, I2CPullUp)
        _validate_integer("capture_limit_bytes", self.capture_limit_bytes, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class ApplicationConfig:
    """Immutable native codec limits; initialization validates protocol constraints."""

    max_encoded_message_size: int = 512
    max_variable_data_size: int = 255
    max_variable_transfers_per_tick: int = 8
    max_expected_tick_count: int = 1_000_000

    def __post_init__(self) -> None:
        _validate_integer("max_encoded_message_size", self.max_encoded_message_size, 0, _SIZE_MAX)
        _validate_integer("max_variable_data_size", self.max_variable_data_size, 0, _SIZE_MAX)
        _validate_integer(
            "max_variable_transfers_per_tick", self.max_variable_transfers_per_tick, 0, _SIZE_MAX
        )
        _validate_integer("max_expected_tick_count", self.max_expected_tick_count, 0, _UINT32_MAX)


@dataclass(frozen=True, slots=True)
class TestConfiguration:
    """Python-owned Test Configuration record; no protocol validation."""

    test_id: TestId
    tick_duration_us: TickDuration
    expected_tick_count: int
    flags: int = 0
    digital_in: tuple[DigitalInputConfig, ...] = (DigitalInputConfig(),) * 10
    digital_out: tuple[DigitalOutputConfig, ...] = (DigitalOutputConfig(),) * 10
    analog_in: tuple[AnalogInputConfig, ...] = (AnalogInputConfig(),) * 2
    analog_out: tuple[AnalogOutputConfig, ...] = (AnalogOutputConfig(),) * 6
    pwm_in: tuple[PWMInputConfig, ...] = (PWMInputConfig(),) * 2
    pwm_out: tuple[PWMOutputConfig, ...] = (PWMOutputConfig(),) * 2
    can: tuple[CANConfig, ...] = (CANConfig(),) * 2
    spi: tuple[SPIConfig, ...] = (SPIConfig(),) * 2
    uart: tuple[UARTConfig, ...] = (UARTConfig(),) * 2
    i2c: tuple[I2CConfig, ...] = (I2CConfig(),) * 2
    extension_data: bytes = b""

    def __post_init__(self) -> None:
        _exact("test_id", self.test_id, TestId)
        _exact("tick_duration_us", self.tick_duration_us, TickDuration)
        _validate_integer("expected_tick_count", self.expected_tick_count, 0, _UINT32_MAX)
        _validate_integer("flags", self.flags, 0, _UINT32_MAX)
        _fixed(
            "digital_in",
            self.digital_in,
            DigitalInputConfig,
            int(_binding.lib.HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT),
        )
        _fixed(
            "digital_out",
            self.digital_out,
            DigitalOutputConfig,
            int(_binding.lib.HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT),
        )
        _fixed(
            "analog_in",
            self.analog_in,
            AnalogInputConfig,
            int(_binding.lib.HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT),
        )
        _fixed(
            "analog_out",
            self.analog_out,
            AnalogOutputConfig,
            int(_binding.lib.HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT),
        )
        _fixed(
            "pwm_in",
            self.pwm_in,
            PWMInputConfig,
            int(_binding.lib.HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT),
        )
        _fixed(
            "pwm_out",
            self.pwm_out,
            PWMOutputConfig,
            int(_binding.lib.HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT),
        )
        _fixed("can", self.can, CANConfig, int(_binding.lib.HIL_APPLICATION_CAN_CHANNEL_COUNT))
        _fixed("spi", self.spi, SPIConfig, int(_binding.lib.HIL_APPLICATION_SPI_CHANNEL_COUNT))
        _fixed("uart", self.uart, UARTConfig, int(_binding.lib.HIL_APPLICATION_UART_CHANNEL_COUNT))
        _fixed("i2c", self.i2c, I2CConfig, int(_binding.lib.HIL_APPLICATION_I2C_CHANNEL_COUNT))
        _bytes("extension_data", self.extension_data)


@dataclass(frozen=True, slots=True)
class TestInstruction:
    """Python-owned Test Instruction record; no protocol validation."""

    test_id: TestId
    tick_number: int = 0
    digital_outputs: tuple[DigitalOutputValue, ...] = (DigitalOutputValue(),) * 10
    analog_outputs: tuple[AnalogOutputValue, ...] = (AnalogOutputValue(),) * 6
    pwm_outputs: tuple[PWMOutputValue, ...] = (PWMOutputValue(),) * 2

    def __post_init__(self) -> None:
        _exact("test_id", self.test_id, TestId)
        _validate_integer("tick_number", self.tick_number, 0, _UINT32_MAX)
        _fixed(
            "digital_outputs",
            self.digital_outputs,
            DigitalOutputValue,
            int(_binding.lib.HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT),
        )
        _fixed(
            "analog_outputs",
            self.analog_outputs,
            AnalogOutputValue,
            int(_binding.lib.HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT),
        )
        _fixed(
            "pwm_outputs",
            self.pwm_outputs,
            PWMOutputValue,
            int(_binding.lib.HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT),
        )


@dataclass(frozen=True, slots=True)
class TestResult:
    """Python-owned Test Result record; no protocol validation."""

    test_id: TestId
    tick_number: int = 0
    digital_inputs: tuple[DigitalInputValue, ...] = (DigitalInputValue(),) * 10
    analog_inputs: tuple[AnalogInputValue, ...] = (AnalogInputValue(),) * 2
    pwm_inputs: tuple[PWMInputValue, ...] = (PWMInputValue(),) * 2
    condition: ResultCondition = ResultCondition.OK
    problem_detail: int = 0

    def __post_init__(self) -> None:
        _exact("test_id", self.test_id, TestId)
        _validate_integer("tick_number", self.tick_number, 0, _UINT32_MAX)
        _fixed(
            "digital_inputs",
            self.digital_inputs,
            DigitalInputValue,
            int(_binding.lib.HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT),
        )
        _fixed(
            "analog_inputs",
            self.analog_inputs,
            AnalogInputValue,
            int(_binding.lib.HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT),
        )
        _fixed(
            "pwm_inputs",
            self.pwm_inputs,
            PWMInputValue,
            int(_binding.lib.HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT),
        )
        _exact("condition", self.condition, ResultCondition)
        _validate_integer("problem_detail", self.problem_detail, 0, _UINT32_MAX)


type ApplicationMessage = TestConfiguration | TestInstruction | TestResult


__all__ = [
    "ApplicationStatus",
    "PeripheralVoltage",
    "BusRole",
    "SPIDataWidth",
    "SPIBitOrder",
    "SPIClockPolarity",
    "SPIClockPhase",
    "UARTElectricalMode",
    "UARTWordLength",
    "UARTParity",
    "UARTStopBits",
    "I2CVoltage",
    "I2CPullUp",
    "ResultCondition",
    "TestId",
    "TickDuration",
    "DigitalOutputValue",
    "DigitalInputValue",
    "AnalogOutputValue",
    "AnalogInputValue",
    "PWMOutputValue",
    "PWMInputValue",
    "DigitalInputConfig",
    "DigitalOutputConfig",
    "AnalogInputConfig",
    "AnalogOutputConfig",
    "PWMInputConfig",
    "PWMOutputConfig",
    "CANConfig",
    "SPIConfig",
    "UARTConfig",
    "I2CConfig",
    "ApplicationConfig",
    "TestConfiguration",
    "TestInstruction",
    "TestResult",
    "ApplicationMessage",
]
