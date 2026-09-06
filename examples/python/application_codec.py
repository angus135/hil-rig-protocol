"""Run with ``python examples/python/application_codec.py`` after installation.

All three messages round-trip without hardware. The final Transport is deliberately
disconnected, so submission returns NOT_READY and reading returns None. A consuming
project supplies link I/O and services Transport before exchanging these same bytes;
see transport_servicing.py and docs/python/application.md.
"""

from __future__ import annotations

from hil_rig_protocol import (
    AnalogInputConfig,
    AnalogInputValue,
    AnalogOutputConfig,
    AnalogOutputValue,
    ApplicationCodec,
    ApplicationConfig,
    DigitalInputConfig,
    DigitalInputValue,
    DigitalOutputConfig,
    DigitalOutputValue,
    PeripheralVoltage,
    PWMInputConfig,
    PWMInputValue,
    PWMOutputConfig,
    PWMOutputValue,
    ResultCondition,
    Role,
    TestConfiguration,
    TestId,
    TestInstruction,
    TestResult,
    TickDuration,
    Transport,
    TransportConfig,
)


def representative_messages() -> tuple[TestConfiguration, TestInstruction, TestResult]:
    # Example-only identity. Identifier allocation belongs to the consuming project.
    test_id = TestId(bytes(range(16)))
    configuration = TestConfiguration(
        test_id=test_id,
        tick_duration_us=TickDuration(microseconds=1000),
        expected_tick_count=100,
        digital_in=(DigitalInputConfig(True, PeripheralVoltage.V_3V3),) * 10,
        digital_out=(DigitalOutputConfig(True, PeripheralVoltage.V_5V, False),) * 10,
        analog_in=(AnalogInputConfig(True),) * 2,
        analog_out=(AnalogOutputConfig(True),) * 6,
        pwm_in=(PWMInputConfig(True, PeripheralVoltage.V_3V3),) * 2,
        pwm_out=(PWMOutputConfig(True, PeripheralVoltage.V_5V, 1_000_000, 5000),) * 2,
        extension_data=b"example",
        # CAN, SPI, UART and I2C default to complete, disabled configuration tuples.
    )
    instruction = TestInstruction(
        test_id=test_id,
        tick_number=0,
        digital_outputs=tuple(DigitalOutputValue(high=i % 2 == 0) for i in range(10)),
        analog_outputs=tuple(AnalogOutputValue(microvolts=i * 500_000) for i in range(6)),
        pwm_outputs=(PWMOutputValue(period_nanoseconds=1_000_000, duty_cycle_permyriad=5000),) * 2,
    )
    result = TestResult(
        test_id=test_id,
        tick_number=0,
        digital_inputs=tuple(DigitalInputValue(high=i % 2 == 1) for i in range(10)),
        analog_inputs=(AnalogInputValue(microvolts=1_500_000),) * 2,
        pwm_inputs=(PWMInputValue(period_nanoseconds=1_000_000, duty_cycle_permyriad=4999),) * 2,
        condition=ResultCondition.OK,
        problem_detail=0,
    )
    return configuration, instruction, result


def main() -> None:
    application_codec = ApplicationCodec(ApplicationConfig())
    configuration, instruction, result = representative_messages()
    for message in (configuration, instruction, result):
        encoded = application_codec.encode(message)
        assert application_codec.decode(encoded) == message
        print(f"{type(message).__name__}: {len(encoded)} bytes, round trip OK")

    # These calls are also the composition boundary for a serviced, connected
    # Transport. Check its status and service delivery using the Transport guide.
    with Transport(Role.HOST, TransportConfig(session_seed=1)) as transport:
        encoded = application_codec.encode(configuration)
        status = transport.submit_application_data(encoded)
        print(f"Disconnected Transport submission: {status.name}")

        received = transport.read_application_data()
        if received is not None:
            decoded = application_codec.decode(received)
            print(f"Received {type(decoded).__name__}")


if __name__ == "__main__":
    main()
