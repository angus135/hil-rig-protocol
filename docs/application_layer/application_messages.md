# Application messages

> **Wire-format quick reference:** See [Application wire-format reference](application_wire_format.md)
> for the common-envelope diagram, current body byte layouts, and literal golden-vector example.

## Status

This document describes the public typed messages and the common wire envelope implemented by the stateless C codec. The public C structures are API representations, not packed wire structures: native enum width, `size_t`, padding, unions and pointer representation do not define encoded bytes. Response and Error wire operations are supported. Type 18 and Type 33 are retired and reserved; production endpoint conversation orchestration remains outside the codec.

## Common envelope

Every complete encoded Application message starts with this fixed 23-byte envelope. The same layout
is shown visually, together with current payload layouts, in the
[Application wire-format reference](application_wire_format.md):

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 byte | Overall HIL-RIG protocol major version |
| 1 | 1 byte | Overall HIL-RIG protocol minor version |
| 2 | 1 byte | Test-ID-present flag, exactly 0 or 1 |
| 3 | 16 bytes | Opaque Test ID |
| 19 | 1 byte | Message type |
| 20 | 1 byte | Message subtype |
| 21 | 2 bytes | Payload length, little-endian `uint16_t` |
| 23 | N bytes | Payload |

There is no payload-end marker. A decoder receives the actual length of one complete message and accepts it only when that length is exactly `23 + payload_length`. Missing bytes and trailing bytes are errors. Message type and subtype are explicit one-byte wire values. Every multi-byte integer in encoded bodies is little-endian. Local capacities and offsets remain `size_t`; only encoded fixed-width fields use their specified integer widths.

The envelope version comes from `hil_rig_protocol/version.h`. Only the repository-wide major and minor bytes are carried in every envelope; patch is not part of the common envelope. Ordinary messages require exact major/minor equality and return `HIL_APPLICATION_STATUS_VERSION_MISMATCH` otherwise. The BASIC System Information Request/Response pair is accepted with foreign major/minor only when it has no Test ID and its body repeats the envelope values. Integration then calls `HIL_APPLICATION_Check_Protocol_Version()` and gates all test traffic on exact major/minor/patch equality. There are no version ranges, fallback codecs, or negotiation; each new Transport session needs confirmation.

For `has_test_id == 0`, the encoder writes sixteen zero Test-ID bytes and the decoder requires those bytes to be zero. For `has_test_id == 1`, all sixteen bytes are opaque and an all-zero Test ID is valid.

There is no Application sequence number. Transport delivery acknowledgement remains distinct from Application semantic Response messages and from local C return statuses.

### Message type identifiers

| Identifier | Numeric value |
| --- | ---: |
| `INVALID` | 0 |
| `SYSTEM_INFO_REQUEST` | 1 |
| `SYSTEM_INFO_RESPONSE` | 2 |
| `TEST_CONFIGURATION` | 16 |
| `TEST_INSTRUCTION` | 17 |
| Retired/reserved | 18 |
| `EXECUTION_CONTROL` | 19 |
| `GLOBAL_CONTROL` | 20 |
| `UPDATE_INSTRUCTION` | 21 |
| `TEST_RESULT` | 32 |
| Retired/reserved | 33 |
| `VARIABLE_TEST_RESULT` | 34 |
| `RESPONSE` | 48 |
| `ERROR` | 49 |
| `RESERVED` | 255 |

The explicit values are stable one-byte wire identifiers. Changing an assigned value requires protocol-version compatibility review.
Wire values 18 and 33 are retired and reserved: they historically identified the
removed Variable Instruction Data and Variable Result Data placeholder families,
respectively, and neither value may be reused.

## Test ID

`HIL_Application_Test_Id_T` contains
`HIL_APPLICATION_TEST_ID_SIZE == 16` opaque bytes.

Codec rules:

- the codec carries but does not generate an ID;
- any 16-byte value, including all-zero, is structurally representable;
- no value has timestamp, ordering, or arithmetic meaning; and
- absence is controlled only by `has_test_id`.

Integration rules:

- the Python host generates a fresh random 128-bit ID for every new upload;
- every message belonging to that test uses the same bytes;
- firmware compares all 16 bytes against the active transaction;
- a restarted upload gets a fresh random ID; and
- Transport session identity and Application Test ID are unrelated.

Presence rules:

| Message type | Test ID |
| --- | --- |
| System Information request/response | forbidden |
| Test Configuration | required |
| Test Instruction | required |
| Update Instruction | required |
| Execution Control | required |
| Global Control | forbidden |
| Test Result | required |
| Variable Test Result | required |
| Application Response | required for test scopes; forbidden for Global Control scope |
| Application Error | optional: present for a test-specific fault, absent for a global fault |

## Message matrix

"Protocol prerequisite" is a cross-message integration rule. Codec functions
do not retain a transaction and therefore do not enforce it.

| Message | Sender | Receiver | Correlation | Protocol prerequisite | Response/effect |
| --- | --- | --- | --- | --- | --- |
| System Information Request | Python | Firmware | No Test ID | None | System Information Response |
| System Information Response | Firmware | Python | No Test ID | Matching request as defined by integration | Diagnostic data only; no transaction effect |
| Test Configuration | Python | Firmware | Fresh Test ID | Starts a new upload attempt | Configuration Response; `ACCEPTED` creates active upload transaction |
| Test Instruction | Python | Firmware | Active Test ID and tick | Configuration accepted; tick T is the expected next tick and T - 1 was accepted when T > 0 | Tick Response after all declared data; no later tick is yet submitted |
| Update Instruction | Python | Firmware | Active Test ID and tick | Configuration accepted; tick T is the expected next tick | Tick Response |
| Execution Control START | Python | Firmware | Accepted Test ID and START | Complete Test `ACCEPTED` | Execution-Control Response reports actual operation outcome |
| Execution Control ABORT | Python | Firmware | Identified active Test ID and ABORT | Matching active transaction/operation | `COMPLETED` prevents previous transaction continuing normally |
| Global Control RESET_APPLICATION | Python | Firmware | No Test ID | None | `COMPLETED` clears active Application transaction data/conditions; Transport unchanged |
| Test Result | Firmware | Python | Accepted Test ID and tick | START completed; execution completed or stopped early and result set is available | Exactly one fixed result for every configured tick in increasing order; no per-result Response |
| Variable Test Result | Firmware | Python | Accepted Test ID and tick | START completed; execution completed or stopped early and sparse records produced | Sent in tick order; no per-result Response |
| Application Response | Firmware | Python | Scope-dependent | A correlated request/data acceptance decision | Carries semantic outcome and transaction effect |
| Application Error | Firmware | Python | Optional Test ID/tick | Broader fault rather than one request rejection | Integration-dependent recovery |

These directions are normative. The codec remains direction-neutral and
stateless: encoding or decoding a structurally valid message does not determine
which endpoint is using it. Firmware and Python handlers enforce direction
after decoding. `HIL_Application_Context_T` contains no endpoint role.

## Decode storage alignment

Caller storage supplied to `HIL_APPLICATION_Decode_Message` must have at least
`HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT` alignment. The constant is usable as
a C11 `_Alignas` operand and a C++ `alignas` operand and is sufficient for every
public typed object placed in decode storage. The size query reports usable byte capacity assuming that alignment. This foundation does not add a new runtime alignment policy; existing message-specific storage behaviour is preserved while variable-storage families remain unfinished.

```c
_Alignas(HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT)
static uint8_t decode_storage[2048u];
```

```cpp
alignas(HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT)
static uint8_t decode_storage[2048u];
```

## Test upload as individual messages

A test upload is a transaction made from exactly one Test Configuration and a
sequence of per-tick instruction messages. A tick may be one fixed
`TEST_INSTRUCTION` message or one or more `UPDATE_INSTRUCTION` chunks. Results
may likewise use fixed `TEST_RESULT` messages or `VARIABLE_TEST_RESULT` chunks.
The codec encodes and validates each complete message independently; endpoint
integration assembles chunks across messages.

The MVP Transport carries each complete Application message in one frame. The
Application codec's `max_variable_data_size` and `max_encoded_message_size`
bound individual messages. Integration must configure Transport's maximum
Application-message size to at least the Application Layer's maximum encoded
message size.

## Common channel and value types

`HIL_Application_Channel_Id_T` combines a protocol-level peripheral family and
a logical `uint16_t` channel number. A channel is not an MCU address, register,
pin encoding, DMA stream, or driver handle.

Peripheral identifiers are `DIGITAL_INPUT`, `DIGITAL_OUTPUT`,
`ANALOG_INPUT`, `ANALOG_OUTPUT`, `PWM_INPUT`, `PWM_OUTPUT`, `UART`, `SPI`,
`I2C`, and `CAN`, plus invalid/reserved sentinels.

Unit-explicit fixed values are:

- digital fixed instruction/result value: 0 or 1;
- analogue fixed instruction/result value: microvolts;
- Digital/PWM Test Configuration voltage selection: one-byte protocol enum;
- tick duration: microseconds;
- PWM periods: nanoseconds;
- PWM duty: permyriad, 0 through 10000;
- communication rate: bits per second.

Hardware capability and electrical safety remain firmware semantic decisions.

### Fixed channel arrays

| Fixed array | Constant | Elements | Index mapping |
| --- | --- | ---: | --- |
| Digital outputs | `HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT` | 10 | index i is DIGITAL_OUTPUT channel i |
| Digital inputs | `HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT` | 10 | index i is DIGITAL_INPUT channel i |
| Analogue outputs | `HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT` | 6 | index i is ANALOG_OUTPUT channel i |
| Analogue inputs | `HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT` | 2 | index i is ANALOG_INPUT channel i |
| PWM outputs | `HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT` | 2 | index i is PWM_OUTPUT channel i |
| PWM inputs | `HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT` | 2 | index i is PWM_INPUT channel i |
| CAN | `HIL_APPLICATION_CAN_CHANNEL_COUNT` | 2 | index i is CAN channel i |
| SPI | `HIL_APPLICATION_SPI_CHANNEL_COUNT` | 2 | index i is SPI channel i |
| UART | `HIL_APPLICATION_UART_CHANNEL_COUNT` | 2 | index i is UART channel i |
| I2C | `HIL_APPLICATION_I2C_CHANNEL_COUNT` | 2 | index i is I2C channel i |

These are external logical channels, not MCU pins or peripheral registers.
Firmware owns hardware mapping. Test Configuration uses the same fixed index
identity: records contain no peripheral or channel identifier. Fixed arrays are
always complete, with no sparse entries, duplicates, omitted-channel defaults,
or implicit retention from a prior configuration. Variable UART/SPI/I2C/CAN
operation and capture payloads are carried by the Type 21 and Type 34 records
described below, not by these fixed configuration arrays.

Each fixed result contains exactly one analogue input element per physical
analogue input channel. For a configured channel, that element is its one sample
for the tick, captured at the test tick rate. The initial protocol has no
independent analogue sampling rate or multiple samples per tick. For any fixed
input channel whose capture is disabled or not configured, firmware encodes
deterministic zero and Python treats that element as semantically invalid.

Encoded sizing uses named fixed channel counts and fixed wire-width constants rather than native C structure or enum sizes. Test Configuration structural semantics are implemented by the codec; hardware capability and workflow semantics remain firmware-owned.

## System Information

### Request

- Type: `SYSTEM_INFO_REQUEST`
- Subtype: `BASIC`
- Test ID: forbidden
- Direction: Python to firmware

`HIL_Application_System_Info_Request_T` contains an initial `BASIC` query and a
flag requesting an optional firmware Git hash. The fully supported `BASIC`
payload has this normative layout:

| Payload offset | Width | Field | Initial encoding |
| ---: | ---: | --- | --- |
| 0 | 1 byte | `request_firmware_git_hash` | boolean, exactly `0x00` or `0x01` |
| 1 | 1 byte | `query` | `BASIC == 0x01` |
| 2 | 2 bytes | application protocol major | `uint16_t` little-endian |
| 4 | 2 bytes | application protocol minor | `uint16_t` little-endian |
| 6 | 2 bytes | application protocol patch | `uint16_t` little-endian |

The complete System Information Request is therefore 31 bytes: the 23-byte
common envelope followed by this eight-byte payload. Outbound request protocol
fields must exactly equal the compiled library version. The decoded triplet is
only a discovery observation until endpoint integration explicitly checks it.

### Response

- Type: `SYSTEM_INFO_RESPONSE`
- Subtype: `BASIC`
- Test ID: forbidden
- Direction: firmware to Python

`HIL_Application_System_Info_Response_T` retains its existing version fields as
diagnostics for the repository-wide HIL-RIG protocol version, firmware semantic
version, optional Git hash bytes, and optional diagnostic bytes. The typed
`application_protocol_major`, `application_protocol_minor`, and
`application_protocol_patch` fields must exactly equal the compiled
`HIL_RIG_PROTOCOL_VERSION_MAJOR`, `HIL_RIG_PROTOCOL_VERSION_MINOR`, and
`HIL_RIG_PROTOCOL_VERSION_PATCH` values. The encoder writes those canonical
compiled values after validation, so a caller cannot supply one version and
silently encode another.

Firmware-specific state may appear as opaque diagnostic data, but it does not
define a shared protocol or firmware state machine. Hash and diagnostic schemas,
limits, and capability discovery remain deferred. Reported versions do not
negotiate or select the codec version; Python does not proceed with
configuration or execution after detecting incompatibility.

## Test Configuration

- Type: `TEST_CONFIGURATION`
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

This is exactly one first test-specific message for each new upload. The Python
host supplies a fresh random Test ID. `HIL_Application_Test_Configuration_T`
contains the global tick settings, ten fixed configuration arrays, and a
length-delimited extension byte span. Array index `i` is logical channel `i`; no
fixed record carries a redundant channel or peripheral identifier.

The arrays are encoded in this order: Digital Input, Digital Output, Analogue
Input, Analogue Output, PWM Input, PWM Output, CAN, SPI, UART, then I2C. Their
physical extents are 10, 10, 2, 6, 2, 2, 2, 2, 2, and 2 records respectively.
The fixed payload, including the one-byte extension length, is 203 bytes. With
the 23-byte common envelope an empty-extension configuration is 226 bytes; an
extension of N bytes is `226 + N`, up to 481 bytes for N = 255. The one-byte
wire field therefore permits at most 255 extension bytes, while each initialized
codec context may impose a smaller local limit through
`context->config.max_variable_data_size`. The exact record offsets and byte
layouts are normative in
[Application wire-format reference](application_wire_format.md#test-configuration).

Every record begins with one-byte `enabled`: `0` is disabled, `1` is enabled,
and other values are invalid. A disabled record is canonical only when every
remaining field is zero. Protocol enums therefore reserve zero as INVALID for
that disabled representation; valid values begin at one, with 255 reserved
where a reserved sentinel is defined.

The public records are:

- Digital Input: enabled, voltage level.
- Digital Output: enabled, voltage level, initial high/low state.
- Analogue Input: enabled only.
- Analogue Output: enabled only.
- PWM Input: enabled, voltage level.
- PWM Output: enabled, voltage level, initial period in nanoseconds, initial duty
  cycle in permyriad.
- CAN: enabled, bit rate, capture limit in bytes, standard receive filter ID,
  and standard receive filter mask. Only 11-bit standard identifiers are
  supported. A zero mask accepts all standard identifiers; filter-bank allocation
  remains firmware-internal. CAN termination is not software-configurable through
  the protocol and physical termination must be fixed or handled outside the
  Application configuration.
- SPI: enabled, bit rate, master/slave role, 8/16-bit data width, bit order,
  clock polarity, clock phase, capture limit in bytes.
- UART: enabled, baud rate, electrical mode, word length, parity, stop bits, RX
  enabled, TX enabled, capture limit in bytes.
- I2C: enabled, bit rate, master/slave role, own 7-bit address, 3.3/5 V voltage
  level, pull-up selection, capture limit in bytes.

The codec validates the supported tick-duration set of `10000`, `1000`, `100`,
or `10` microseconds; nonzero `expected_tick_count` within the configured limit;
zero test-wide flags; the extension pointer/length invariant and extension
length against `context->config.max_variable_data_size`; Booleans; canonical
disabled records; recognized enums; PWM duty/period combinations; nonzero rates
for enabled communications; communication capture limits against the same
configured variable-data limit; UART RX/TX constraints; and I2C
role/address constraints, plus 11-bit CAN filter ID/mask bounds. Analogue
input/output deliberately have no protocol-selectable electrical parameters in
this version.

Those are structural protocol rules only. Firmware later decides whether a
channel exists, an exact rate is supported, rails and hardware are safe, MCU
timing is achievable, drivers conflict, complete-test storage is available, or
the current workflow state permits the configuration. The stateless codec does
not make those decisions. Filter-bank allocation, analogue-input sampling
frequency, analogue-output reference selection, and hardware-specific rate/timing
choices remain firmware policies. Unsupported hardware configurations must be
rejected rather than silently substituted. Communication configuration and the
bounded Type 21/34 record payloads are implemented here; cross-message assembly
remains an endpoint responsibility.

A configuration-scoped `ACCEPTED` Response creates the active upload
transaction. `REJECTED` or `FAILED` creates no transaction; instructions for
that Test ID are not accepted, and the host starts a new upload from Test
Configuration with a fresh ID.

## Zero-based tick numbering

Tick numbering is normative and shared by instructions, instruction chunks,
fixed results, result chunks, Responses, and Errors that identify a tick. For
`expected_tick_count == N`, valid ticks are exactly 0 through N - 1.

Submitted instruction ticks are strictly increasing but need not be contiguous.
The codec may structurally bound `expected_tick_count`, but only integration can
compare a later tick against the active Test Configuration or expected next tick.
An omitted instruction tick means no output update at that tick and the executor
retains the preceding output state.

## Test Instruction

- Type: `TEST_INSTRUCTION`
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

The current `HIL_Application_Test_Instruction_T` and fixed wire body describe
exactly one tick and contain only:

- zero-based `tick_number`;
- all 10 digital output values in channel-index order;
- all 6 analogue output values in channel-index order; and
- both PWM output settings in channel-index order.

The fixed payload is exactly 50 bytes, so the complete encoded message is
73 bytes including the 23-byte common envelope:

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | `tick_number`, little-endian `uint32_t` |
| 4 | 10 | 10 Digital Output bytes, channel indexes 0 through 9 |
| 14 | 24 | 6 Analogue Output values, each little-endian `uint32_t` microvolts |
| 38 | 12 | 2 PWM Output records, each little-endian `uint32_t` period nanoseconds followed by little-endian `uint16_t` duty permyriad |

The codec requires `tick_number < context->config.max_expected_tick_count`,
each Digital Output value to be `0` or `1`, each PWM duty to be at most
`10000`, and a zero PWM period to have zero duty. The tick limit is only a
stateless structural ceiling. Integration remains responsible for comparing a
tick with the active Test Configuration's actual `expected_tick_count` and for
enforcing tick ordering. The codec does not impose an Analogue Output range or
hardware-specific PWM feasibility limits, and it does not validate values
against enabled or disabled Test Configuration channels.

No scheduling function, interrupt configuration, timer selection, hardware
register, or execution-manager value appears in this message.

## Update Instruction and Tick Response

- Type: `UPDATE_INSTRUCTION` (Type 21)
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

The Type 21 wire body carries one chunk for one Test ID and one tick. The
`HAS_MORE_CHUNKS` flag means another Type 21 message for that tick follows;
`COMPLETE_TICK` means this is the final or only chunk. All chunks for an
incomplete tick are contiguous, use the same Test ID and tick number, and keep
the same instruction family. No different tick or instruction family may be
inserted before the incomplete tick is completed or rejected.

Within the assembled tick, each fixed-state DIGITAL_OUTPUT, ANALOG_OUTPUT and
PWM_OUTPUT peripheral/channel pair may occur at most once. UART, SPI and CAN
operations may repeat across chunks when needed, and endpoint integration
preserves message order and record order. Submitted instruction ticks are
strictly increasing but need not be contiguous. The stateless codec validates
each chunk and does not enforce these cross-message rules.

Successful Tick Response is sent only after the final chunk has been assembled,
cross-chunk rules have been checked, semantic validation has passed, and
retention responsibility has been accepted. An endpoint may send a negative
Tick Response as soon as rejection is known. After rejection, partial assembly
is discarded and the existing upload-recovery contract applies. Reset,
disconnect, session loss, or successful abort discards an incomplete tick. The
host must not submit a later instruction tick before the completed tick receives
its positive Tick Response. A Transport ACK confirms delivery only; it does not
accept the tick semantically.

## Automatic Complete Test Response

There is no Finalize Test message in the codec. After the endpoint accepts the
upload according to its instruction-family and tick policy, firmware integration
may perform whole-test validation and create a Complete Test Response. The exact
upload-finalisation behaviour for sparse instructions remains open work for this
version.

Complete Test `ACCEPTED` means the whole test was retained, validated, and is
available for a subsequent START request. It does not itself begin execution.
A rejected or failed whole-test validation invalidates the transaction and
requires a new upload from Test Configuration. There is no ARM command or public
phase/state value between upload completion and START.

## Execution Control

- Type: `EXECUTION_CONTROL`
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

The minimal initial commands are:

| Command | Protocol prerequisite | `COMPLETED` meaning | Recovery after negative outcome |
| --- | --- | --- | --- |
| `START` | Complete Test `ACCEPTED` for the same Test ID | Firmware performed the requested start operation | `REJECTED` means execution did not start; `FAILED` requires recovery and no assumed execution status |
| `ABORT` | Matching active upload, accepted test, execution, or result transaction | Firmware safely stopped/abandoned it; the previous transaction cannot continue normally | `REJECTED` means abort was not performed; `FAILED` requires firmware-specific recovery |

Decoding never executes either command. Firmware integration asks the execution
manager whether an operation is allowed, performs it if possible, and returns
the actual outcome. `OPERATION_NOT_ALLOWED`, `HARDWARE_NOT_READY`, or a more
specific reason may explain rejection. Rejection is not a codec or Transport
failure.

After ABORT `COMPLETED`, any new upload begins from Test Configuration with a
fresh Test ID. There is no resumable behavior in the initial protocol.

There is no `ARM`, `FINALIZE_TEST`, or command whose sole purpose is forcing a
named firmware state.

## Global Control

- Type: `GLOBAL_CONTROL`
- Subtype: `NONE`
- Test ID: forbidden
- Direction: Python to firmware

`RESET_APPLICATION` requests that firmware clear active Application transaction
data and recoverable Application protocol conditions. It is available when no
Test ID is known. Decoding does not perform recovery.

Global Control `COMPLETED` means firmware performed the requested Application
cleanup. Firmware decides how cleanup maps to its internal state, storage, and
execution manager. `REJECTED` or `FAILED` means the host cannot assume cleanup.
The command never resets, reconnects, or reinitializes Transport.

## Test Result

- Type: `TEST_RESULT`
- Subtype: `NONE`
- Test ID: required
- Direction: firmware to Python

After a successfully completed START request for a test configured with N
ticks, firmware produces exactly one fixed Test Result for every tick `0..N-1`.
The current `HIL_Application_Test_Result_T` and fixed wire body contain:

- zero-based `tick_number` matching the instruction;
- all 10 captured digital input values;
- both captured analogue input values;
- both captured PWM input measurements;
- result `condition`: `OK`, `PARTIAL`, or `EXECUTION_PROBLEM`; and
- integration-defined `problem_detail`.

The fixed payload is exactly 39 bytes, so the complete encoded message is
62 bytes including the 23-byte common envelope:

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | `tick_number`, little-endian `uint32_t` |
| 4 | 10 | 10 Digital Input bytes, channel indexes 0 through 9 |
| 14 | 8 | 2 Analogue Input values, each little-endian `uint32_t` microvolts |
| 22 | 12 | 2 PWM Input records, each little-endian `uint32_t` period nanoseconds followed by little-endian `uint16_t` duty permyriad |
| 34 | 1 | result `condition` |
| 35 | 4 | `problem_detail`, little-endian `uint32_t` |

The codec requires `tick_number < context->config.max_expected_tick_count`,
each Digital Input value to be `0` or `1`, each PWM duty to be at most `10000`,
and a zero PWM period to have zero duty. Only `OK`, `PARTIAL`, and
`EXECUTION_PROBLEM` are valid result conditions; unknown and reserved values
are rejected. `problem_detail` and Analogue Input values are structurally
unconstrained. The codec does not validate fixed values against enabled or
disabled Test Configuration channels, and integration remains responsible for
the active configuration's actual tick range, tick ordering, and hardware
feasibility.

Problems detected while executing are Application Error messages. A non-OK
fixed-result condition records that tick's result quality. An Error may be sent
when a problem is detected, but it never replaces or reorders the complete fixed
result set.

The MVP conditions have exact endpoint meanings. All three values are
structurally representable by the fixed codec:

- `OK`: every configured fixed capture represented by the result is valid.
- `PARTIAL`: every configured fixed capture remains valid, but one or more
  requested variable communication captures failed or are incomplete.
- `EXECUTION_PROBLEM`: the complete set of fixed captured values is semantically
  invalid and Python ignores it.

If any configured fixed capture cannot be trusted, firmware uses
`EXECUTION_PROBLEM`. The MVP cannot represent selective validity among fixed
digital, analogue, or PWM fields. If execution stops or fails before all ticks
execute, firmware still produces a fixed result for every remaining tick; ticks
without valid fixed execution/capture data use `EXECUTION_PROBLEM`.

Fixed capture channels disabled or absent from configuration are encoded as
deterministic zero and ignored by Python. Their presence alone causes neither
`PARTIAL` nor `EXECUTION_PROBLEM`. No validity masks or additional fixed-capture
messages are introduced.

## Variable Test Result and completion

- Type: `VARIABLE_TEST_RESULT` (Type 34)
- Subtype: `NONE`
- Test ID: required
- Direction: firmware to Python

All chunks for one result tick are contiguous and use the same Test ID and tick.
`condition` and `problem_detail` are identical in every chunk for that tick.
`HAS_MORE_CHUNKS` means more Type 34 messages for the same tick follow;
`COMPLETE_TICK` completes that result tick. Within the assembled tick, each
fixed-state DIGITAL_INPUT, ANALOG_INPUT and PWM_INPUT peripheral/channel pair
may occur at most once. UART, SPI and CAN captured records may repeat across
chunks, with message and record order preserved.

No next result tick begins before the current tick reaches `COMPLETE_TICK`.
Variable result ticks are sent in increasing order. Every configured tick from
`0` through `expected_tick_count - 1` has a final Type 34 message. The final
message may contain zero records, allowing an empty or fault-only result. No
Application Response is sent for result chunks. Python considers a result tick
complete only after its `COMPLETE_TICK` chunk, and the complete result stream
ends only after the final configured tick is complete.

The stateless codec validates each message and does not enforce cross-message
assembly, ordering, or family selection. Incomplete result assembly is
discarded on reset, disconnect, or session loss. Result resumption and range
requests remain deferred.

## Application Response

- Type: `RESPONSE`
- Subtype: `NONE`
- Test ID: required for test scopes; forbidden for Global Control scope
- Direction: firmware to Python

`HIL_Application_Response_T` contains:

- `scope`: Test Configuration, Tick, Complete Test, Execution Control, or Global
  Control;
- `outcome`: Accepted, Rejected, Completed, or Failed;
- expandable protocol-oriented `reason`;
- `tick_number`, interpreted by endpoint workflow;
- execution and global command correlation fields; and
- integration-defined `detail`.

Test-scoped Responses require the correlated Test ID. Global Control Responses
forbid one. Endpoint integrations normally use zero/INVALID for irrelevant
correlation fields, but the codec preserves and accepts every defined command
value without imposing that convention.

Initial reasons cover unsupported input, operation not allowed, inconsistent
Test ID, invalid tick, length mismatch, storage unavailable, validation failure,
hardware not ready, and internal failure. Structurally malformed Application
data produces a local codec status rather than an Application Response reason.

Endpoint workflow normally uses these success combinations:

- Configuration, Tick, and Complete Test use `ACCEPTED`;
- Execution Control and Global Control use `COMPLETED`.

`REJECTED` means the requested semantic acceptance or operation did not occur.
`FAILED` means processing began but could not complete. Transaction effects and
required recovery follow the scope-specific rules above. The shared codec does
not enforce this scope/outcome/reason/command/tick matrix; firmware and Python
decide whether a decoded outcome is appropriate for their current workflow.

For Type 21, only the assembled tick may await semantic acceptance. Later
instruction ticks are not submitted until the current tick receives its
positive Tick Response; later delivery of messages for a rejected incomplete
tick does not revive the upload. The final chunk is the point at which the
endpoint can decide whether to send that response.

The initial protocol has no Application request ID or sequence number. Python
may have only one response-requiring operation outstanding at a time. A Type 21
chunk sequence collectively counts as one Tick operation. While awaiting a
Response, Python does not repeat an
indistinguishable System Information Request, Test Configuration, START, ABORT,
or RESET_APPLICATION request. Upload finalisation after sparse instruction ticks
is still an integration decision for this version.

If Transport/session failure makes the operation's outcome uncertain, Python
enters recovery rather than blindly retrying. Once it explicitly abandons that
operation, it may send RESET_APPLICATION and ignores any subsequently received
Response for the abandoned operation. This serialization is implemented by
endpoint integration and adds no request tracking to the codec context.

## Application Error

- Type: `ERROR`
- Subtype: `NONE`
- Test ID: optional
- Direction: firmware to Python

Initial categories are Hardware, Execution, Timeout, Retained Data, Protocol,
and Internal. A global Error has no Test ID or tick, a test-wide Error has a Test
ID and no tick, and a tick-specific Error has both. Categories do not determine
Test-ID presence, recoverability, endpoint state, or a required recovery action.
An Error is not a rejection of one request,
not a local codec status, and not a Transport corruption/delivery failure.
For a successfully started test, an execution Error does not replace any of the
N required fixed results. Only session/communication loss or reset removes the
normal complete-set guarantee.

The host may request test-scoped ABORT when it knows the active Test ID, or
test-independent RESET_APPLICATION when it does not. Firmware owns actual
recovery and internal state transitions.

## Validation rules

This foundation implements the common structural boundary and preserves the
message-specific validation that already exists. The common codec currently
checks:

- exact repository-wide HIL-RIG protocol major/minor version for ordinary
  messages; BASIC System Information Request/Response without a Test ID may
  instead carry foreign major/minor solely for discovery, while requiring its
  body major/minor to repeat the envelope;
- valid one-byte message type and subtype representations, rejecting invalid,
  reserved, and unknown values;
- Boolean Test-ID presence and the type/scope-specific Test-ID rules already
  represented by the typed messages;
- zero-filled Test-ID bytes when the presence flag is zero;
- exact encoded body length with no missing or trailing bytes;
- checked header/payload arithmetic and fixed-width length conversion;
- byte spans against encoded input and caller decode-storage capacity; and
- the existing validation implemented for supported fixed message families.

The following feature-specific rules remain later work and must not be inferred
from this codec: analogue range/unit conversion beyond the defined microvolt
wire representation, hardware-specific PWM feasibility, cross-message chunk
assembly/correlation, Response and Error semantic combinations, comparison
with an active Test Configuration's actual `expected_tick_count`, and workflow
state. Test Configuration structural validation, including communication
configuration and reserved-zero test-wide flags, is implemented. Fixed Test
Instruction and Test Result structural value validation is also implemented:
ticks use the configured structural ceiling, Digital values are Boolean, PWM
duty is `0..10000` with zero duty required for a zero period, and Test Result
conditions are limited to `OK`, `PARTIAL`, and `EXECUTION_PROBLEM`. Each
supported family is validated only within the one message presented to the
codec; cross-message family selection, chunk ordering, aggregate limits, and
retention are integration responsibilities.

Firmware/Python integration separately owns active-Test-ID checks, tick range
and stop-and-wait order, cross-message chunk matching/completeness, duplicate
fixed-state records across chunks, retention capacity, whole-test consistency,
transaction prerequisites, hardware support/safety, and execution-manager
decisions.

At the implemented common boundary, structurally malformed examples include a
forbidden or missing Test ID, a non-Boolean Test-ID-present flag, nonzero Test-ID
bytes with an absent flag, invalid/reserved type or subtype, incompatible
protocol version, inconsistent payload length, arithmetic overflow, truncation,
or trailing bytes.

Structurally valid but semantically rejectable endpoint cases include a wrong
active Test ID, out-of-range or out-of-order tick, incomplete chunk assembly,
unsupported hardware, insufficient retention capacity, START before Complete
Test acceptance, or execution-manager refusal.

## Shared conformance scenarios

These sequences specify endpoint agreement. Codec tests exercise implemented common framing and existing supported bodies; endpoint transaction behaviour remains outside the stateless codec.

### Successful configuration and upload

```text
Python creates fresh random Test ID A
Python -> Firmware: Test Configuration(A, expected_tick_count 2)
Firmware -> Python: Response(Configuration, ACCEPTED, A)
Active upload transaction A now exists
Python -> Firmware: Update Instruction(A, tick 0, UART0 record, HAS_MORE_CHUNKS)
Python -> Firmware: Update Instruction(A, tick 0, CAN record, COMPLETE_TICK)
Firmware -> Python: Response(Tick 0, ACCEPTED, A)
No update is submitted for omitted tick 1; output state is retained
Python -> Firmware: Update Instruction(A, tick 2, COMPLETE_TICK)
Firmware -> Python: Response(Tick 2, ACCEPTED, A)
Firmware automatically validates the whole test
Firmware -> Python: Response(Complete Test, ACCEPTED, A)
Test A is available for a subsequent START request
```

### Rejected configuration

```text
Python -> Firmware: Test Configuration(A)
Firmware -> Python: Response(Configuration, REJECTED, reason, A)
No upload transaction A is created
Python restarts with Test Configuration and fresh random Test ID B
```

### Rejected incomplete update tick

```text
Configuration for A was ACCEPTED
Python -> Firmware: Update Instruction(A, tick 0, SPI1 record, HAS_MORE_CHUNKS)
Python submits a different tick or instruction family before tick 0 completes
Firmware -> Python: Response(Tick 0, REJECTED, INVALID_TICK, A)
Partial tick 0 assembly is discarded and upload recovery applies
```

### Rejected or out-of-order tick

```text
Configuration for A has an incomplete tick 0
Python -> Firmware: Update Instruction(A, tick 2, COMPLETE_TICK)
Firmware -> Python: Response(Tick 2, REJECTED, INVALID_TICK, A)
Upload transaction A is invalidated
No later tick is submitted before the completed tick receives its positive Response
Python restarts from Test Configuration with a fresh Test ID
```

### Wrong Test ID

```text
Active upload uses Test ID A
Python -> Firmware: Test Instruction(B, tick 0)
Firmware -> Python: Response(Tick 0, REJECTED, INCONSISTENT_TEST_ID, B)
Message B is not accepted into transaction A
The incomplete or rejected assembly is discarded
Python restarts from Test Configuration with a fresh Test ID
```

### Successful complete-test validation

```text
Endpoint upload policy accepts the submitted instruction chunks for A
Firmware performs whole-test validation according to that policy
Firmware -> Python: Response(Complete Test, ACCEPTED, A)
Python may now request START(A)
```

### Failed whole-test validation

```text
Endpoint whole-test validation for A fails
Firmware whole-test validation fails
Firmware -> Python: Response(Complete Test, REJECTED, VALIDATION_FAILED, A)
Transaction A is invalidated and retained-data cleanup is firmware-owned
Python restarts from Test Configuration with a fresh Test ID
```

### START accepted and rejected

```text
Complete Test(A) was ACCEPTED
Python -> Firmware: Execution Control(START, A)
Firmware asks its execution manager and performs the allowed request
Firmware -> Python: Response(Execution Control START, COMPLETED, A)

Complete Test(B) was not accepted, or firmware cannot currently execute it
Python -> Firmware: Execution Control(START, B)
Firmware -> Python: Response(Execution Control START, REJECTED,
                             OPERATION_NOT_ALLOWED or specific reason, B)
No codec or Transport failure is implied
```

### ABORT

```text
Python -> Firmware: Execution Control(ABORT, A)
Firmware performs safe stop/abandonment and cleanup according to its own modules
Firmware -> Python: Response(Execution Control ABORT, COMPLETED, A)
Transaction A cannot continue normally
Any new upload starts with Test Configuration and a fresh Test ID
```

### Global reset without Test ID

```text
Python -> Firmware: Global Control(RESET_APPLICATION, no Test ID)
Firmware clears active Application transaction data and recoverable conditions
Firmware -> Python: Response(Global Control RESET_APPLICATION, COMPLETED,
                             no Test ID)
Transport remains connected and is not reset
```

### Complete variable result transfer

```text
Firmware execution for A completes and results become available
Firmware -> Python: Variable Test Result(A, tick 0, OK, CAN and UART records,
                                          HAS_MORE_CHUNKS)
Firmware -> Python: Variable Test Result(A, tick 0, OK, UART records,
                                          COMPLETE_TICK)
Firmware -> Python: Variable Test Result(A, tick 1, OK, zero records,
                                          COMPLETE_TICK)
...result ticks continue in increasing order through tick N-1...
Python successfully completes each tick at its COMPLETE_TICK chunk
Python considers result transfer complete after tick N-1 completes
Firmware releases retained results according to its Transport/storage policy
```

No Application Response is sent for result chunks. Transport ACKs do not change
the Application ordering contract.

### Partial variable capture with valid fixed captures

```text
Firmware has valid configured digital, analogue and PWM captures for tick T
One requested SPI capture failed; a requested CAN capture is valid
Firmware -> Python: Variable Test Result(A, tick T, PARTIAL, CAN0 record,
                                          COMPLETE_TICK)
Python treats every configured fixed capture as valid
Python does not expect a record for the failed SPI capture
```

### Early execution failure with deterministic remaining results

```text
Test A has expected_tick_count 3 and START completed successfully
Firmware -> Python: Test Result(A, tick 0, OK, one analogue sample per channel)
Firmware detects an execution problem before tick 1 and may send Error(A, tick 1)
Firmware -> Python: Variable Test Result(A, tick 1, EXECUTION_PROBLEM,
                                          zero records, COMPLETE_TICK)
Firmware -> Python: Variable Test Result(A, tick 2, EXECUTION_PROBLEM,
                                          zero records, COMPLETE_TICK)
Python completes all 3 result ticks despite the reported problem
Python considers normal result transfer complete despite the reported problem
```

The optional Error does not replace a result chunk or permit tick 2 to be sent
before tick 1 reaches `COMPLETE_TICK`.

### Serialized response-requiring operations

```text
Python -> Firmware: System Information Request
Python waits; it does not repeat that indistinguishable request
Firmware -> Python: System Information Response
Python -> Firmware: Test Configuration(A)
Firmware -> Python: Response(Configuration, ACCEPTED, A)
...Python uploads complete instruction ticks one at a time...
Python waits for Response(Complete Test, ACCEPTED, A)
Only then does Python -> Firmware: Execution Control(START, A)
```

If Transport loss makes START's outcome uncertain, Python does not blindly
retry START. It abandons that pending operation, enters recovery, may request
test-independent RESET_APPLICATION, and ignores a late Response for the
abandoned START.

### Incompatible protocol version

```text
Envelope carries repository-wide HIL-RIG protocol major/minor bytes
Received envelope major/minor does not exactly match the compiled version
Ordinary-message codec reports HIL_APPLICATION_STATUS_VERSION_MISMATCH
Endpoint integration does not proceed with configuration or execution
```

The common envelope has no independent Application version and does not encode the repository patch version. BASIC System Information is the sole foreign-envelope discovery exception; body and envelope major/minor must agree, and `HIL_APPLICATION_Check_Protocol_Version()` requires exact major/minor/patch equality. System Information cannot select or negotiate a different encoding version.

### Transport session loss during upload

```text
Upload transaction A is active
Transport reports session loss/reset to firmware and Python integration
Transport itself does not mutate Application data
Integration invalidates upload A and discards any incomplete instruction or
result tick assembly
Transport reconnects
Python restarts with Test Configuration and a fresh Test ID B
```

Transport cannot automatically restart an upload, execution request, or result
transaction. After Transport delivery failure or session loss, host or firmware
integration decides whether and how to restart the higher-level operation.

## Foundation decisions completed

The current C foundation now defines and tests:

- the 23-byte common envelope and current fixed-body field ordering;
- explicit fixed wire widths and little-endian multi-byte fields;
- literal golden vectors for the foundation wire contract; and
- executable C codec tests for the supported foundation behavior.

These items are no longer open design decisions. Later protocol versions may
extend them only through the documented versioning process.

## Remaining incomplete v0.3.0 work

This version does not finalise:

- upload-finalisation behaviour for every sparse-instruction edge case;
- firmware assembly capacities;
- Python and firmware state-machine implementation;
- maximum aggregate operations or bytes across all chunks for one tick;
- streaming validation or memory-efficient firmware decoding;
- I2C variable records;
- result resumption or range requests; and
- multi-version negotiation or compatibility.

Cross-endpoint firmware/Python conformance remains required before those endpoint
implementations can be considered protocol-conformant.
