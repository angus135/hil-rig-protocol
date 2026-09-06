"""Populated public messages, independently checked against native binding fixtures."""

from dataclasses import replace

import hil_rig_protocol as p


def configuration(extension: bytes = b"") -> p.TestConfiguration:
    value = p.TestConfiguration(
        test_id=p.TestId(bytes=b"\t\x1a+<M^o\x80\x91\xa2\xb3\xc4\xd5\xe6\xf7\x08"),
        tick_duration_us=p.TickDuration(microseconds=1000),
        expected_tick_count=987654,
        flags=0,
        digital_in=(
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_3V3),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_5V),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_12V),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_24V),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_3V3),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_5V),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_12V),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_24V),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_3V3),
            p.DigitalInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_5V),
        ),
        digital_out=(
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_3V3, initial_high=False
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_5V, initial_high=True
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_12V, initial_high=False
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_24V, initial_high=True
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_3V3, initial_high=False
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_5V, initial_high=True
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_12V, initial_high=False
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_24V, initial_high=True
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_3V3, initial_high=False
            ),
            p.DigitalOutputConfig(
                enabled=True, voltage_level=p.PeripheralVoltage.V_5V, initial_high=True
            ),
        ),
        analog_in=(p.AnalogInputConfig(enabled=False), p.AnalogInputConfig(enabled=True)),
        analog_out=(
            p.AnalogOutputConfig(enabled=False),
            p.AnalogOutputConfig(enabled=True),
            p.AnalogOutputConfig(enabled=False),
            p.AnalogOutputConfig(enabled=True),
            p.AnalogOutputConfig(enabled=False),
            p.AnalogOutputConfig(enabled=True),
        ),
        pwm_in=(
            p.PWMInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_3V3),
            p.PWMInputConfig(enabled=True, voltage_level=p.PeripheralVoltage.V_5V),
        ),
        pwm_out=(
            p.PWMOutputConfig(
                enabled=True,
                voltage_level=p.PeripheralVoltage.V_3V3,
                initial_period_nanoseconds=123456,
                initial_duty_cycle_permyriad=1234,
            ),
            p.PWMOutputConfig(
                enabled=True,
                voltage_level=p.PeripheralVoltage.V_5V,
                initial_period_nanoseconds=199999,
                initial_duty_cycle_permyriad=5555,
            ),
        ),
        can=(
            p.CANConfig(
                enabled=True,
                bit_rate=125000,
                capture_limit_bytes=71,
                filter_id=0x123,
                filter_mask=0x7F0,
            ),
            p.CANConfig(
                enabled=True,
                bit_rate=250000,
                capture_limit_bytes=72,
                filter_id=0x456,
                filter_mask=0x700,
            ),
        ),
        spi=(
            p.SPIConfig(
                enabled=True,
                bit_rate=234567,
                role=p.BusRole.MASTER,
                data_width=p.SPIDataWidth.BITS_8,
                bit_order=p.SPIBitOrder.MSB_FIRST,
                clock_polarity=p.SPIClockPolarity.IDLE_LOW,
                clock_phase=p.SPIClockPhase.FIRST_EDGE,
                capture_limit_bytes=81,
            ),
            p.SPIConfig(
                enabled=True,
                bit_rate=469134,
                role=p.BusRole.SLAVE,
                data_width=p.SPIDataWidth.BITS_16,
                bit_order=p.SPIBitOrder.LSB_FIRST,
                clock_polarity=p.SPIClockPolarity.IDLE_HIGH,
                clock_phase=p.SPIClockPhase.SECOND_EDGE,
                capture_limit_bytes=82,
            ),
        ),
        uart=(
            p.UARTConfig(
                enabled=True,
                baud_rate=57600,
                electrical_mode=p.UARTElectricalMode.TTL_5V,
                word_length=p.UARTWordLength.BITS_8,
                parity=p.UARTParity.EVEN,
                stop_bits=p.UARTStopBits.BITS_1,
                rx_enabled=True,
                tx_enabled=False,
                capture_limit_bytes=91,
            ),
            p.UARTConfig(
                enabled=True,
                baud_rate=115200,
                electrical_mode=p.UARTElectricalMode.RS232,
                word_length=p.UARTWordLength.BITS_9,
                parity=p.UARTParity.ODD,
                stop_bits=p.UARTStopBits.BITS_2,
                rx_enabled=True,
                tx_enabled=True,
                capture_limit_bytes=92,
            ),
        ),
        i2c=(
            p.I2CConfig(
                enabled=True,
                bit_rate=100000,
                role=p.BusRole.MASTER,
                own_address_7bit=0,
                voltage_level=p.I2CVoltage.V_3V3,
                pull_up=p.I2CPullUp.OHM_2K2,
                capture_limit_bytes=101,
            ),
            p.I2CConfig(
                enabled=True,
                bit_rate=200000,
                role=p.BusRole.SLAVE,
                own_address_7bit=83,
                voltage_level=p.I2CVoltage.V_5V,
                pull_up=p.I2CPullUp.OHM_10K,
                capture_limit_bytes=102,
            ),
        ),
        extension_data=b"",
    )
    return replace(value, extension_data=extension)


def instruction() -> p.TestInstruction:
    value = p.TestInstruction(
        test_id=p.TestId(bytes=b"\t\x1a+<M^o\x80\x91\xa2\xb3\xc4\xd5\xe6\xf7\x08"),
        tick_number=876543,
        digital_outputs=(
            p.DigitalOutputValue(high=True),
            p.DigitalOutputValue(high=False),
            p.DigitalOutputValue(high=True),
            p.DigitalOutputValue(high=False),
            p.DigitalOutputValue(high=True),
            p.DigitalOutputValue(high=False),
            p.DigitalOutputValue(high=True),
            p.DigitalOutputValue(high=False),
            p.DigitalOutputValue(high=True),
            p.DigitalOutputValue(high=False),
        ),
        analog_outputs=(
            p.AnalogOutputValue(microvolts=4045620583),
            p.AnalogOutputValue(microvolts=4037966262),
            p.AnalogOutputValue(microvolts=4030311941),
            p.AnalogOutputValue(microvolts=4022657620),
            p.AnalogOutputValue(microvolts=4015003299),
            p.AnalogOutputValue(microvolts=4007348978),
        ),
        pwm_outputs=(
            p.PWMOutputValue(period_nanoseconds=3777185127, duty_cycle_permyriad=1234),
            p.PWMOutputValue(period_nanoseconds=3775950560, duty_cycle_permyriad=5555),
        ),
    )
    return value


def result() -> p.TestResult:
    value = p.TestResult(
        test_id=p.TestId(bytes=b"\t\x1a+<M^o\x80\x91\xa2\xb3\xc4\xd5\xe6\xf7\x08"),
        tick_number=876543,
        digital_inputs=(
            p.DigitalInputValue(high=True),
            p.DigitalInputValue(high=False),
            p.DigitalInputValue(high=True),
            p.DigitalInputValue(high=False),
            p.DigitalInputValue(high=True),
            p.DigitalInputValue(high=False),
            p.DigitalInputValue(high=True),
            p.DigitalInputValue(high=False),
            p.DigitalInputValue(high=True),
            p.DigitalInputValue(high=False),
        ),
        analog_inputs=(
            p.AnalogInputValue(microvolts=4045620583),
            p.AnalogInputValue(microvolts=4037966262),
        ),
        pwm_inputs=(
            p.PWMInputValue(period_nanoseconds=3777185127, duty_cycle_permyriad=1234),
            p.PWMInputValue(period_nanoseconds=3775950560, duty_cycle_permyriad=5555),
        ),
        condition=p.ResultCondition.OK,
        problem_detail=3508749671,
    )
    return value
