# Python Application codec

`ApplicationCodec` encodes and decodes complete BASIC System Information,
Execution Control, Global Control, Test Configuration, fixed Digital, Analog
and PWM Test Instruction and Test Result messages, sparse Update Instruction and
Variable Test Result messages, Application Responses, and Application Errors. All wire
encoding, decoding and protocol validation execute the shared native C Application
implementation. Import the supported API from `hil_rig_protocol`; CFFI objects and
conversion helpers are private.

The codec is stateless and direction-neutral. Either endpoint may encode or decode
any supported message. It retains only configuration limits, with no active test,
host/rig role, transaction, tick progress or sequence. Test ID allocation, test
lifecycle, message ordering, enabled-channel policy and host/rig workflow belong to
consuming projects. It does not translate every-tick state into state changes.

## Construction and ownership

Install using the [Transport guide](transport.md#installation), then create an
explicit configuration:

```python
from hil_rig_protocol import ApplicationCodec, ApplicationConfig

application_codec = ApplicationCodec(
    ApplicationConfig(
        max_encoded_message_size=512,
        max_variable_data_size=255,
        max_expected_tick_count=1_000_000,
    )
)
assert application_codec.config == ApplicationConfig()
```

These defaults match `HIL_APPLICATION_Default_Config`. `max_encoded_message_size`
and `max_variable_data_size` are native `size_t`; `max_expected_tick_count` is
`uint32_t`. Python permits their full unsigned representation ranges; native
initialization decides whether a policy is usable.
`max_variable_data_size` bounds extension bytes and variable record spans;
capture capacity is an internal firmware property and is not configured here.
`max_expected_tick_count` limits configuration tick counts and is the exclusive
upper bound for fixed-message tick numbers. These limits reserve no tick storage.

Create and use each codec on the same thread, including reading its `config`
property. Cross-thread access raises `ApplicationOwnershipError`. Independent
threads may own independent codecs. Copying, deep copying and pickling a codec
raise `TypeError`. No `close()`, reset or context manager is needed: this native
context owns no dynamic resources. Immutable message/configuration values may be
shared between threads.

## Complete messages

All public records are frozen, slotted dataclasses. Arrays are exact immutable
tuples; element index is the logical channel number. Defaults provide complete
disabled configuration arrays or zero-valued fixed I/O arrays. The message class
determines type, subtype and Test ID presence; there is no configurable envelope.

```python
from hil_rig_protocol import (
    AnalogInputValue,
    AnalogOutputValue,
    DigitalInputValue,
    DigitalOutputConfig,
    DigitalOutputValue,
    PeripheralVoltage,
    PWMInputValue,
    PWMOutputValue,
    ResultCondition,
    TestConfiguration,
    TestId,
    TestInstruction,
    TestResult,
    TickDuration,
)

# Fixed example identity, allocated by the consuming application in real use.
test_id = TestId(bytes(range(16)))
configuration = TestConfiguration(
    test_id=test_id,
    tick_duration_us=TickDuration(microseconds=1000),
    expected_tick_count=100,
    flags=0,
    digital_out=(DigitalOutputConfig(True, PeripheralVoltage.V_3V3, False),) * 10,
    extension_data=b"metadata",
)
instruction = TestInstruction(
    test_id=test_id,
    tick_number=0,
    digital_outputs=(DigitalOutputValue(high=True),) * 10,
    analog_outputs=(AnalogOutputValue(microvolts=1_500_000),) * 6,
    pwm_outputs=(PWMOutputValue(period_nanoseconds=1_000_000, duty_cycle_permyriad=5000),) * 2,
)
result = TestResult(
    test_id=test_id,
    tick_number=0,
    digital_inputs=(DigitalInputValue(high=False),) * 10,
    analog_inputs=(AnalogInputValue(microvolts=1_499_000),) * 2,
    pwm_inputs=(PWMInputValue(period_nanoseconds=1_000_000, duty_cycle_permyriad=4999),) * 2,
    condition=ResultCondition.OK,
    problem_detail=0,
)
for message in (configuration, instruction, result):
    encoded = application_codec.encode(message)
    assert application_codec.decode(encoded) == message
```

`ApplicationMessage` includes those fixed test messages plus `SystemInfoRequest`,
`SystemInfoResponse`, `ExecutionControl`, and `GlobalControl`.
`TestId.bytes` is exactly 16 immutable bytes; zero bytes are valid, and the codec
does not generate identifiers. `tick_number`, `expected_tick_count`, `flags` and
`problem_detail` are unsigned 32-bit integers. Flags are reserved and native C
currently requires zero. Problem detail is an integration-defined diagnostic.

## Discovery and controls

`PROTOCOL_VERSION` is the native library's `ProtocolVersion(major, minor, patch)`.
`SystemInfoRequest()` defaults to that triplet. Decode can return a structurally
valid foreign discovery message so the endpoint can inspect it; it does not make
the peer compatible. Call `check_protocol_version()` and block test messages
until it succeeds. It requires exact patch equality as well as envelope major and
minor equality, and must be repeated after every new Transport session. A peer
without usable discovery is unconfirmed.

```python
from hil_rig_protocol import (
    ControlCommand,
    ExecutionControl,
    GlobalControl,
    GlobalControlCommand,
    SystemInfoRequest,
    check_protocol_version,
)

request = SystemInfoRequest(request_firmware_git_hash=True)
peer_request = application_codec.decode(application_codec.encode(request))
assert isinstance(peer_request, SystemInfoRequest)
check_protocol_version(peer_request.protocol_version)

start = ExecutionControl(test_id, ControlCommand.START)
abort = ExecutionControl(test_id, ControlCommand.ABORT)
reset = GlobalControl(GlobalControlCommand.RESET_APPLICATION)
```

`ApplicationVersionMismatchError` exposes `local_version`, `peer_version`, and
the `VERSION_MISMATCH` status. START requires an accepted complete test, ABORT
abandons that operation, and RESET_APPLICATION clears Application state while
preserving Transport. The codec only represents these controls; endpoint
lifecycle enforcement remains consuming-application work.

`FinalizeTestUpload(test_id, flags=0)` is the explicit Type 22 upload-finalisation
request. Send it only after every submitted tick has a positive Tick Response,
or immediately after accepted sparse Test Configuration when no instruction
messages were submitted. It declares that no more instruction chunks will be
sent. Firmware returns the existing Complete Test Response scope; only an
accepted response makes START valid. There is no result-finalisation message.

`ApplicationResponse` carries an optional `TestId`, `ResponseScope`,
`ResponseOutcome`, `ResponseReason`, tick, command-correlation fields and detail.
Global Control scope requires `test_id=None`; every other scope requires a Test
ID. `ApplicationErrorMessage` carries an optional Test ID, `ErrorCategory`, exact
Boolean recoverability, optional tick, detail, and diagnostic `bytes`. A global
Error has neither Test ID nor tick, a test-wide Error has a Test ID and no tick,
and a tick-specific Error has both. Decoded diagnostic data is copied into
Python-owned `bytes`.

The codec checks wire structure and correlation fields only. Firmware and Python
interpret whether an outcome is appropriate for their current workflow. A
Transport ACK confirms reliable delivery; an Application Response reports an
Application-level semantic outcome. Test Results receive no Application
acknowledgements. Type 21 Update Instruction and Type 34 Variable Test Result
messages are supported as bounded chunks. The codec does not accumulate or
order chunks across calls.

`ResultCondition` includes `OK`, `PARTIAL`, `EXECUTION_PROBLEM` and `RESERVED`.
Native C accepts the first three, including `PARTIAL`. `EXECUTION_PROBLEM` means
the complete fixed capture set cannot be trusted. The consuming application
interprets condition and problem detail; the codec returns the complete values
without filtering them.

### Units and fixed extents

| Field | Representation and unit |
| --- | --- |
| `TickDuration.microseconds` | unsigned 32-bit microseconds |
| Analog value `microvolts` | unsigned 32-bit microvolts |
| PWM `period_nanoseconds`, `initial_period_nanoseconds` | unsigned 32-bit nanoseconds |
| PWM `duty_cycle_permyriad`, `initial_duty_cycle_permyriad` | unsigned 16-bit; 10000 means 100% |
| `bit_rate` | unsigned 32-bit bits per second |
| `baud_rate` | unsigned 32-bit symbols per second |
| `own_address_7bit` | unsigned 16-bit container; native C checks the address rules |
| `filter_id`, `filter_mask` | unsigned 16-bit containers; native C checks the standard 11-bit `0x000..0x7FF` protocol range |
| `enabled`, `high`, `initial_high`, `rx_enabled`, `tx_enabled` | exact `bool` |

| Family | Configuration field / count | Fixed message field / count |
| --- | --- | --- |
| Digital input | `digital_in` / 10 | result `digital_inputs` / 10 |
| Digital output | `digital_out` / 10 | instruction `digital_outputs` / 10 |
| Analog input | `analog_in` / 2 | result `analog_inputs` / 2 |
| Analog output | `analog_out` / 6 | instruction `analog_outputs` / 6 |
| PWM input | `pwm_in` / 2 | result `pwm_inputs` / 2 |
| PWM output | `pwm_out` / 2 | instruction `pwm_outputs` / 2 |
| CAN, SPI, UART, I2C | `can`, `spi`, `uart`, `i2c` / 2 each | Type 21/34 records |

The ten configuration record types preserve all native fields:

| Record | Fields (in addition to `enabled`) |
| --- | --- |
| `DigitalInputConfig` | `voltage_level` |
| `DigitalOutputConfig` | `voltage_level`, `initial_high` |
| `AnalogInputConfig`, `AnalogOutputConfig` | none |
| `PWMInputConfig` | `voltage_level` |
| `PWMOutputConfig` | `voltage_level`, `initial_period_nanoseconds`, `initial_duty_cycle_permyriad` |
| `CANConfig` | `bit_rate`, `filter_id`, `filter_mask` |
| `SPIConfig` | `bit_rate`, `role`, `data_width`, `bit_order`, `clock_polarity`, `clock_phase` |
| `UARTConfig` | `baud_rate`, `electrical_mode`, `word_length`, `parity`, `stop_bits`, `rx_enabled`, `tx_enabled` |
| `I2CConfig` | `bit_rate`, `role`, `own_address_7bit`, `voltage_level`, `pull_up` |

Use the exact `PeripheralVoltage`, `BusRole`, `SPIDataWidth`, `SPIBitOrder`,
`SPIClockPolarity`, `SPIClockPhase`, `UARTElectricalMode`, `UARTWordLength`,
`UARTParity`, `UARTStopBits`, `I2CVoltage` and `I2CPullUp` enums for their fields.
For example, `SPIDataWidth.BITS_8`, `UARTStopBits.BITS_1`,
`I2CVoltage.V_3V3` and `I2CPullUp.OHM_4K7`. Each enum exposes native invalid and
reserved sentinels as well as usable selections. Disabled records default to
zero/`False`/`INVALID`. CAN uses standard 11-bit identifiers only. A receive
filter matches when `(received_standard_id & filter_mask) == (filter_id &
filter_mask)`; mask zero accepts every standard identifier. Filter-bank allocation
is firmware-internal. CAN termination is not software-configurable through this
protocol, so physical termination must be fixed or managed outside Application
configuration.

Extensions are immutable `bytes` of length 0 through 255, subject to native policy.
Complete encoded sizes are 194 bytes for a configuration without extensions,
449 with a 255-byte extension, 73 for an instruction and 62 for a result.

## Validation and errors

Python validates representation: exact types, C integer widths, Test ID length,
tuple extents and element types, and the one-byte extension length. It rejects
lists, mutable byte containers in value records, floats, unrelated enums,
integer substitutes for enums and `bool` substitutes for integers. Wrong types
raise `TypeError`; unrepresentable lengths/ranges raise `ValueError`.

Native C validates protocol semantics during initialization/encoding/decoding.
For example, `TickDuration(999)` and `PWMOutputValue(0, 65535)` are representable
Python values but fail native message encoding. Python does not duplicate tick
duration rules, disabled-record canonical values, the CAN 11-bit filter bound,
PWM relationships, UART direction rules, canonical disabled I2C or cross-field
limits. Enabled I2C configuration and I2C operation/result records are not
implemented in v0.3.0 and return `ApplicationStatus.NOT_IMPLEMENTED` from the
native boundary. `CANConfig.filter_id` and `filter_mask` therefore accept Python integers
through `0xFFFF`; native encoding rejects values above `0x7FF`. Hardware-only
choices such as CAN filter-bank allocation, analogue-input sampling frequency,
analogue-output reference selection, peripheral instances, and timer/register
settings are not Python fields. Firmware integration must reject unsupported
hardware configurations instead of silently substituting alternatives.

```python
from hil_rig_protocol import ApplicationDecodeError, ApplicationEncodeError

try:
    application_codec.encode(instruction)
except ApplicationEncodeError as error:
    print(error.status)  # Exact ApplicationStatus returned by C.

try:
    application_codec.decode(b"malformed")
except ApplicationDecodeError as error:
    print(error.status)
```

All Application exceptions inherit `ApplicationError`, which inherits
`ProtocolError`. `ApplicationConfigurationError` also inherits `ValueError`.
Initialization failures use `ApplicationConfigurationError`; message encoding
failures use `ApplicationEncodeError`; malformed, truncated, trailing, oversized,
unsupported or deferred input uses `ApplicationDecodeError`. Native internal
errors use `ApplicationInternalError`. Unknown statuses, impossible native
outputs and size/storage disagreements use `ApplicationBindingError`.

Native-caused errors retain their exact `ApplicationStatus` in `.status`.
Wrapper-only failures have `status=None`, including a native-successfully-decoded
family deliberately outside the Python subset. A size-query `BUFFER_TOO_SMALL`
can mean the configured message limit was exceeded and becomes an encode error.
The same status from encoding/decoding with wrapper-owned correctly sized buffers
is a binding error. Raw CFFI exceptions never form the public error contract.

`decode()` follows Transport's `collections.abc.Buffer` input convention and
requires C-contiguous storage. It snapshots input before the first native call,
so `bytes`, `bytearray` and contiguous `memoryview` are accepted. Empty input
deliberately reaches C and raises a decode error with `TRUNCATED_MESSAGE`.
Returned records and spans are entirely Python-owned and remain valid after input
mutation, another decode or codec destruction.

## Explicit Transport composition

For a connected and serviced Transport, submission and reception remain separate
caller operations:

```python
encoded = application_codec.encode(instruction)
status = transport.submit_application_data(encoded)
# Handle TransportStatus and service Transport using the Transport caller guide.

encoded = transport.read_application_data()
if encoded is not None:
    message = application_codec.decode(encoded)
```

Configure Transport to accept the Application sizes you intend to exchange. A
maximum 449-byte configuration fits the default 512-byte Transport configuration.
Neither codec inspects the other's configuration. Transport delivers opaque bytes
unchanged, and `DELIVERY_CONFIRMED` acknowledges byte delivery only. A malformed
Application payload can be delivered and acknowledged normally, then rejected by
`ApplicationCodec.decode()`.

The runnable [codec example](../../examples/python/application_codec.py) needs no
hardware and shows all three messages plus these explicit Transport calls. See
the [Transport servicing example](../../examples/python/transport_servicing.py)
for caller-owned byte-stream servicing.

## Sparse updates and variable results

`UpdateInstruction` (type 21) carries a tuple of `LogicalOperation` values.
`VariableTestResult` (type 34) carries a tuple of `CapturedRecord` values. Each
record names a `PeripheralType`, a logical channel, and immutable `bytes`.

```python
from hil_rig_protocol import (
    ApplicationCodec,
    ApplicationConfig,
    CapturedRecord,
    LogicalOperation,
    PeripheralType,
    TestId,
    UpdateInstruction,
    VariableTestResult,
)

codec = ApplicationCodec(ApplicationConfig())
test_id = TestId(bytes(range(16)))  # Example only; allocate a fresh ID per real test.
update = UpdateInstruction(
    test_id=test_id,
    tick_number=10,
    operations=(
        LogicalOperation(PeripheralType.DIGITAL_OUTPUT, 0, b"\x05\x00"),
        LogicalOperation(PeripheralType.UART, 1, b"hello"),
        LogicalOperation(PeripheralType.SPI, 0, b"\x02\x01\x02\x12\x34\x56"),
    ),
)
assert codec.decode(codec.encode(update)) == update

result = VariableTestResult(
    test_id=test_id,
    tick_number=10,
    records=(CapturedRecord(PeripheralType.UART, 1, b"reply"),),
)
assert codec.decode(codec.encode(result)) == result
```

Flags `0` completes the tick; flags `1` means more chunks follow for that tick.
The caller splits streams into payloads of at most 255 bytes and no more than
`MAX_VARIABLE_CHUNKS_PER_TICK` (8) messages per tick. Eight 512-byte messages
bound a chunked tick to 4096 complete encoded bytes. The ninth Type 21 chunk
gets a negative Tick Response and invalidates the upload; firmware must never
emit a ninth Type 34 chunk, and receiving one enters recovery. The codec
preserves flags and tick numbers; it does not split, accumulate, order, or
execute chunks. Duplicate fixed peripheral/channel pairs across an assembled
tick are rejected by endpoint integration, while UART/SPI/CAN records may
repeat in message and record order. I2C records are not implemented.

For one Test ID, select either fixed Test Instruction or Type 21 for the
instruction stream, and either fixed Test Result or Type 34 for the result
stream; do not mix families within either stream. For Type 21, continuations are
contiguous and retain Test ID and tick number. For Type 34, continuations are
contiguous, retain Test ID and tick number, and repeat `condition` and
`problem_detail`. Python considers a Type 34 tick complete only after its
`COMPLETE_TICK` chunk. These are integration rules, not codec state.

On capture overflow, firmware emits the retained prefix as normal Type 34
records with `ResultCondition.PARTIAL` and
`RESULT_PROBLEM_DETAIL_CAPTURE_OVERFLOW`, then completes the tick normally.
`EXECUTION_PROBLEM` takes precedence if completion is prevented.

SPI update payloads contain a packet count, one nonzero size byte per packet,
then the concatenated packet data. SPI result payloads are raw received bytes.
The [wire specification](../application_layer/application_wire_format.md) defines
digital, analogue, PWM and CAN payloads. Update operations must be nonempty;
results may have no records, including pure fault reports. An `OK` result requires
zero `problem_detail`.

Python constructors check representation (exact enum types, unsigned integer
widths, immutable byte spans and tuples). Peripheral rules and protocol semantics
remain in C. Decoding allocates aligned storage for native record descriptors and
payloads, checks their bounds and pointers, and returns detached Python values.
Native storage can be larger than the encoded message.

After binding changes, rebuild the native extension before running Python tests:

```sh
python -m pip install -e ".[test]"
python -m pytest tests/python/test_application_update_instruction.py
python -m pytest tests/python
```

The package build generates CFFI source and compiles it through CMake;
`bindings/python/build_ffi.py` alone generates C source and does not build the extension.

## Deferred scope

No test lifecycle, active-test state, role enforcement, tick sequencing,
every-tick/state-change translation, hardware I/O or consuming
Python API/MCU integration is provided. These require separate future work; no
typed Application methods are added to `Transport`.
