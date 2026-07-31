# Application messages

## Status

This is the message-level companion to
[Application Layer design](application_layer.md). It documents the typed API in
`include/hil_rig_protocol/application/`. The binary envelope, field widths,
byte order, compatibility behavior, and all codec algorithms remain TODO. Every
current `.c` function is an intentional `NOT_IMPLEMENTED` stub.

The structures below are C API representations, not packed wire layouts.
`size_t`, enum representation, padding, unions, and pointers must never be
copied directly onto the wire.

## Common envelope

Every complete message is represented by `HIL_Application_Message_T`:

| Field | Meaning |
| --- | --- |
| `type` | Explicit `HIL_Application_Message_Type_T` selecting one body union member. |
| `subtype` | `BASIC` for System Information; `NONE` for current test-specific families. |
| `has_test_id` | Explicit test-ID presence; no byte value represents absence. |
| `test_id` | Opaque 16-byte identifier when present. |
| `body` | Tagged union member selected by `type`. |

There is no Application sequence number. Transport supplies reliable ordered
delivery. Application correlation uses Test ID when applicable, Response scope,
tick number, peripheral/channel, and the relevant control command. A Transport
ACK confirms byte/frame delivery; an Application Response separately confirms a
semantic outcome. Initial instruction upload uses stop-and-wait at tick
granularity rather than an Application sequence field.

### Message type identifiers

| Identifier | Numeric value |
| --- | ---: |
| `INVALID` | 0 |
| `SYSTEM_INFO_REQUEST` | 1 |
| `SYSTEM_INFO_RESPONSE` | 2 |
| `TEST_CONFIGURATION` | 16 |
| `TEST_INSTRUCTION` | 17 |
| `VARIABLE_INSTRUCTION_DATA` | 18 |
| `EXECUTION_CONTROL` | 19 |
| `GLOBAL_CONTROL` | 20 |
| `TEST_RESULT` | 32 |
| `VARIABLE_RESULT_DATA` | 33 |
| `RESPONSE` | 48 |
| `ERROR` | 49 |
| `RESERVED` | 255 |

The explicit values are intended as future stable identifiers. Changing them
after publication may break firmware/Python compatibility. Their encoded field
width is not selected by this PR.

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
| Variable Instruction Data | required |
| Execution Control | required |
| Global Control | forbidden |
| Test Result | required |
| Variable Result Data | required |
| Application Response | required for test scopes; forbidden for Global Control scope |
| Application Error | optional: present for a test-specific fault, absent for a global fault |

## Message matrix

"Protocol prerequisite" is a cross-message integration rule. Codec functions
do not retain a transaction and therefore do not enforce it.

| Message | Normal sender | Normal receiver | Correlation | Protocol prerequisite | Response/effect |
| --- | --- | --- | --- | --- | --- |
| System Information Request | Python | Firmware | No Test ID | None | System Information Response |
| System Information Response | Firmware | Python | No Test ID | Matching request as defined by integration | Diagnostic data only; no transaction effect |
| Test Configuration | Python | Firmware | Fresh Test ID | Starts a new upload attempt | Configuration Response; `ACCEPTED` creates active upload transaction |
| Test Instruction | Python | Firmware | Active Test ID and tick | Configuration accepted; tick T is the expected next tick and T - 1 was accepted when T > 0 | Tick Response after all declared data; no later tick is yet submitted |
| Variable Instruction Data | Python | Firmware | Active Test ID, tick, peripheral, channel | One matching unique, nonzero declaration in the outstanding fixed instruction | Included in correlated Tick Response; duplicates are invalid |
| Execution Control START | Python | Firmware | Accepted Test ID and START | Complete Test `ACCEPTED` | Execution-Control Response reports actual operation outcome |
| Execution Control ABORT | Python | Firmware | Identified active Test ID and ABORT | Matching active transaction/operation | `COMPLETED` prevents previous transaction continuing normally |
| Global Control RESET_APPLICATION | Python | Firmware | No Test ID | None | `COMPLETED` clears active Application transaction data/conditions; Transport unchanged |
| Test Result | Firmware | Python | Accepted Test ID and tick | START completed; execution completed or stopped early and result set is available | Exactly one fixed result for every configured tick; no per-result Response |
| Variable Result Data | Firmware | Python | Test ID, tick, peripheral, channel | Matching declaration in fixed result | No per-result Response in initial model |
| Application Response | Usually firmware | Usually Python | Scope-dependent | A correlated request/data acceptance decision | Carries semantic outcome and transaction effect |
| Application Error | Usually firmware | Usually Python | Optional Test ID/tick | Broader fault rather than one request rejection | Integration-dependent recovery |

## Decode storage alignment

Caller storage supplied to `HIL_APPLICATION_Decode_Message` must have at least
`HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT` alignment. The constant is usable as
a C11 `_Alignas` operand and a C++ `alignas` operand and is sufficient for every
public typed object placed in decode storage. The size query reports usable byte
capacity assuming that alignment. A future decoder returns
`HIL_APPLICATION_STATUS_INVALID_ARGUMENT` for a non-null misaligned pointer;
current stubs intentionally perform no runtime check.

```c
_Alignas(HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT)
static uint8_t decode_storage[2048u];
```

```cpp
alignas(HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT)
static uint8_t decode_storage[2048u];
```

## Test upload as individual messages

A test upload is a transaction made from exactly one Test Configuration, one
fixed Test Instruction per zero-based tick, and separate variable
UART/SPI/I2C/CAN Application messages correlated by Test ID, tick, peripheral,
and channel. It is not one monolithic package, and a complete tick is not folded
into one large Application message.

Transport may transparently fragment each complete Application message. The
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

- digital value: 0 or 1;
- analogue values/ranges: signed microvolts;
- tick and PWM periods: nanoseconds;
- PWM duty: permyriad, 0 through 10000;
- communication rate: bits per second.

Hardware capability and electrical safety remain firmware semantic decisions.

### Fixed signal-channel arrays

| Fixed array | Constant | Elements | Index mapping |
| --- | --- | ---: | --- |
| Digital outputs | `HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT` | 10 | index i is DIGITAL_OUTPUT channel i |
| Digital inputs | `HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT` | 10 | index i is DIGITAL_INPUT channel i |
| Analogue outputs | `HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT` | 6 | index i is ANALOG_OUTPUT channel i |
| Analogue inputs | `HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT` | 2 | index i is ANALOG_INPUT channel i |
| PWM outputs | `HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT` | 2 | index i is PWM_OUTPUT channel i |
| PWM inputs | `HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT` | 2 | index i is PWM_INPUT channel i |

These are external logical channels, not MCU pins or peripheral registers.
Firmware owns hardware mapping. Fixed arrays contain values only and are always
complete: there are no sparse entries, duplicates, omitted-channel defaults, or
implicit retention from the previous tick. Variable UART/SPI/I2C/CAN payloads
remain separate declaration and byte-span messages.

Each fixed result contains exactly one analogue input element per physical
analogue input channel. For a configured channel, that element is its one sample
for the tick, captured at the test tick rate. The initial protocol has no
independent analogue sampling rate or multiple samples per tick. For any fixed
input channel whose capture is disabled or not configured, firmware encodes
deterministic zero and Python treats that element as semantically invalid.

Future encoded sizing includes exactly the named number of fixed elements. It
must add future fixed-width fields with checked arithmetic rather than use
`sizeof` on C structures. Exact wire widths, order, and byte order remain TODO.

## System Information

### Request

- Type: `SYSTEM_INFO_REQUEST`
- Subtype: `BASIC`
- Test ID: forbidden
- Direction: Python to firmware

`HIL_Application_System_Info_Request_T` contains an initial `BASIC` query and a
flag requesting an optional firmware Git hash.

### Response

- Type: `SYSTEM_INFO_RESPONSE`
- Subtype: `BASIC`
- Test ID: forbidden
- Direction: firmware to Python

`HIL_Application_System_Info_Response_T` contains Application protocol version
diagnostics, firmware semantic version, optional Git hash bytes, and optional
diagnostic bytes. Firmware-specific state may appear as opaque diagnostic data,
but it does not define a shared protocol or firmware state machine. Hash and
diagnostic schemas, limits, capability discovery, and negotiation remain
deferred.

## Test Configuration

- Type: `TEST_CONFIGURATION`
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

This is exactly one first test-specific message for each new upload. The Python
host supplies a fresh random Test ID. `HIL_Application_Test_Configuration_T`
contains:

- nonzero `tick_duration.nanoseconds`;
- nonzero authoritative `expected_tick_count` N, defining ticks 0 through N - 1;
- reserved `flags`, which must be zero;
- typed `peripherals` and `peripheral_count`; and
- reserved length-delimited `extension_data`, which must be empty.

Each `(peripheral, channel)` may have at most one configuration record.
Duplicates are structurally invalid; there is no first-wins or last-wins
behavior. Analogue configuration contains only its channel and microvolt range:
each configured analogue input is sampled once per tick at the test tick rate.

The codec eventually validates tag/member, uniqueness, and basic unit/count
consistency. Nonzero reserved flags or nonempty extension data produce
`HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE`. Firmware validates supported
hardware, electrical ranges, timing, and retention capacity.

A configuration-scoped `ACCEPTED` Response creates the active upload
transaction. `REJECTED` or `FAILED` creates no transaction; instructions for
that Test ID are not accepted, and the host starts a new upload from Test
Configuration with a fresh ID.

## Zero-based tick numbering

Tick numbering is normative and shared by instructions, variable data, fixed
results, variable results, Responses, and Errors that identify a tick. For
`expected_tick_count == N`, valid ticks are exactly 0 through N - 1.

Instructions are sent in increasing stop-and-wait order. Tick T + 1 is not
submitted until Tick T receives `ACCEPTED`. The codec may structurally bound
`expected_tick_count`, but only integration can compare a later tick against the
active Test Configuration or expected next tick.

## Test Instruction

- Type: `TEST_INSTRUCTION`
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

Each `HIL_Application_Test_Instruction_T` describes exactly one tick:

- zero-based `tick_number`;
- all 10 digital output values in channel-index order;
- all 6 analogue output values in channel-index order;
- both PWM output settings in channel-index order; and
- variable-data declaration array/count.

Every `HIL_Application_Data_Declaration_T::byte_length` must be nonzero. A
channel with no variable data is omitted. Within the fixed tick, each
`(peripheral, channel)` pair appears at most once, and each declaration requires
exactly one matching Variable Instruction Data message. Duplicate matching
variable messages are invalid; no declaration ID or sequence field is added.

No scheduling function, interrupt configuration, timer selection, hardware
register, or execution-manager value appears in this message.

## Variable Instruction Data and Tick Response

- Type: `VARIABLE_INSTRUCTION_DATA`
- Subtype: `NONE`
- Test ID: required
- Direction: Python to firmware

`HIL_Application_Variable_Instruction_Data_T` contains the tick, peripheral,
logical channel, and one complete declared byte span. Multiple declarations
produce multiple separately encoded messages.

Firmware integration returns a Tick Response only after:

- Test ID matches the active upload;
- tick is the expected zero-based tick in range;
- every peripheral/channel was declared;
- every unique, nonzero declaration has exactly matching bytes;
- no duplicate variable message was received for a declaration;
- no required declaration is missing;
- semantic validation succeeds; and
- integration takes responsibility for retaining the complete fixed and
  variable tick data.

The fixed instruction and every associated variable message collectively form
the one outstanding tick. Tick `ACCEPTED` allows the host to submit tick T + 1;
before that Response, no later tick may be submitted. This stop-and-wait rule is
an initial Application transaction rule, not Transport state or firmware
execution-manager state. A Transport ACK remains only a delivery confirmation,
and Transport may fragment and reliably deliver each complete Application
message independently. Future pipelining requires a versioned capability.

Tick acceptance does not require a completed NAND write: retention may be NAND,
RAM, or an accepted storage-manager queue.

A rejected or failed tick or associated variable-data message invalidates the
upload. The host restarts from Test Configuration with a fresh Test ID.
Remaining messages for that one outstanding tick may be rejected; a later tick
must not already have been submitted.

## Automatic Complete Test Response

There is no Finalize Test message. After firmware accepts every tick `0..N-1`
and all associated variable data, firmware integration automatically performs
whole-test validation and creates a Complete Test Response.

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
`HIL_Application_Test_Result_T` contains:

- zero-based `tick_number` matching the instruction;
- all 10 captured digital input values;
- both captured analogue input values;
- both captured PWM input measurements;
- variable-result declaration array/count;
- result `condition`: `OK`, `PARTIAL`, or `EXECUTION_PROBLEM`; and
- integration-defined `problem_detail`.

Problems detected while executing are Application Error messages. A non-OK
fixed-result condition records that tick's result quality. An Error may be sent
when a problem is detected, but it never replaces the complete fixed result set.

If execution stops or fails before all ticks execute, firmware still produces a
fixed result for every remaining tick. A tick without valid execution/capture
data uses `EXECUTION_PROBLEM`; its captured-value fields remain structurally
present but are semantically invalid and Python ignores them. It has no variable
declarations unless valid variable data exists. Fixed capture channels disabled
or absent from configuration are encoded as deterministic zero and ignored by
Python. Fixed fields in an `EXECUTION_PROBLEM` result are likewise ignored.

`PARTIAL` means some data for that specific tick is valid. Its declarations
identify only the valid variable data that will follow. Without a declaration,
no empty variable-result message is sent. No validity masks or additional
capture messages are introduced.

## Variable Result Data and completion

- Type: `VARIABLE_RESULT_DATA`
- Subtype: `NONE`
- Test ID: required
- Direction: firmware to Python

Each fixed-result declaration has nonzero length and a unique
`(peripheral, channel)` pair. It is followed by exactly one matching complete
Variable Result Data message correlated by Test ID, tick, peripheral, and
channel. Duplicate matching variable messages are invalid.

For a test with N ticks, Python considers result transfer complete only after
successfully decoding every fixed Test Result for ticks `0..N-1` and every
variable message declared by those results.

Firmware considers all result messages handed off according to its surrounding
Transport integration after every complete encoded result message has been
accepted for delivery. Whether retained results wait for a Transport delivery
acknowledgement is firmware/Transport policy. Application defines no simultaneous
endpoint state change, per-result Response, result-finalization message, or
result-summary message.

Transport/session loss, reset, or inability to communicate is the exception:
the complete set cannot be guaranteed and integration reports recovery is
required. Result ranges, resume after reconnect, Application-level
retransmission, higher-rate/multi-sample analogue capture, and result summaries
are deferred.

## Application Response

- Type: `RESPONSE`
- Subtype: `NONE`
- Test ID: required for test scopes; forbidden for Global Control scope
- Direction: primarily firmware to Python

`HIL_Application_Response_T` contains:

- `scope`: Test Configuration, Tick, Complete Test, Execution Control, or Global
  Control;
- `outcome`: Accepted, Rejected, Completed, or Failed;
- expandable protocol-oriented `reason`;
- `tick_number`, meaningful only for Tick scope;
- `control_command`, meaningful only for Execution Control scope;
- `global_control_command`, meaningful only for Global Control scope; and
- integration-defined `detail`.

Test-scoped Responses require the correlated Test ID. Global Control Responses
forbid one. Irrelevant correlation fields use zero/INVALID.

Initial reasons cover unsupported input, operation not allowed, inconsistent
Test ID, invalid tick, length mismatch, storage unavailable, validation failure,
hardware not ready, and internal failure. Structurally malformed Application
data produces a local codec status rather than an Application Response reason.

Scope/outcome success combinations are:

- Configuration, Tick, and Complete Test use `ACCEPTED`;
- Execution Control and Global Control use `COMPLETED`.

`REJECTED` means the requested semantic acceptance or operation did not occur.
`FAILED` means processing began but could not complete. Transaction effects and
required recovery follow the scope-specific rules above.

Tick Responses implement mandatory stop-and-wait for the initial upload. Only
the current fixed-plus-variable tick may await semantic acceptance. Later ticks
are not submitted until the current Tick Response is `ACCEPTED`; later delivery
of messages for an invalidated outstanding tick does not revive the upload.

## Application Error

- Type: `ERROR`
- Subtype: `NONE`
- Test ID: optional
- Direction: primarily firmware to Python

Initial categories are Hardware, Execution, Timeout, Retained Data, Protocol,
and Internal. A test-specific error carries the Test ID when known; a global
hardware/power error may omit it. An Error is not a rejection of one request,
not a local codec status, and not a Transport corruption/delivery failure.
For a successfully started test, an execution Error does not replace any of the
N required fixed results. Only session/communication loss or reset removes the
normal complete-set guarantee.

The host may request test-scoped ABORT when it knows the active Test ID, or
test-independent RESET_APPLICATION when it does not. Firmware owns actual
recovery and internal state transitions.

## Validation rules

Future typed and encoded codec validation checks:

- valid supported type and correct subtype;
- exact type/scope-specific test-ID presence;
- selected union member;
- exact encoded body length with no trailing bytes;
- every enum excluding invalid/reserved sentinels;
- allowed Response scope/outcome/correlation combinations;
- every fixed signal-array element and value range;
- variable pointer/count and NULL/zero pairs;
- variable counts against codec configuration;
- checked addition/multiplication/alignment;
- byte spans against variable-data limits;
- nonzero variable declaration and variable-message lengths;
- unique `(peripheral, channel)` declarations within one fixed tick;
- unique `(peripheral, channel)` records within one Test Configuration;
- peripheral configuration tag/channel consistency;
- digital values limited to 0/1;
- PWM duty no greater than 10000;
- nonzero Test Configuration tick duration and `expected_tick_count`;
- zero values for Test Configuration, Execution Control, Global Control, and
  Communication Configuration reserved `flags`;
- empty Test Configuration `extension_data`; and
- Application Error tick-presence consistency.

Nonzero reserved flags or nonempty unsupported Test Configuration extension
data return `HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE`.

Firmware/Python integration separately validates active Test ID, tick range and
stop-and-wait order, cross-message declaration matching/completeness and
duplicate variable messages, retention capacity, whole-test consistency,
transaction prerequisites, hardware support/safety, and execution-manager
decisions.

Structurally malformed examples include a forbidden/missing Test ID, wrong
subtype, NULL pointer with nonzero count, invalid channel family, inconsistent
length, overflow, invalid fixed value, incomplete fixed array, or Response with
irrelevant correlation fields populated. Zero-length or duplicate declarations,
duplicate configuration records, and zero expected tick count are also
structurally invalid. Reserved flags and unsupported extension data are
structurally well formed but unsupported by this protocol version.

Structurally valid but semantically rejectable examples include wrong active
Test ID, out-of-range/out-of-order tick, missing declared data, unsupported
hardware, insufficient retention capacity, START before Complete Test
acceptance, or execution-manager refusal.

## Shared conformance scenarios

These sequences specify endpoint agreement. Current tests only construct their
typed messages and compile against intentional stubs.

### Successful configuration and upload

```text
Python creates fresh random Test ID A
Python -> Firmware: Test Configuration(A, expected_tick_count 2)
Firmware -> Python: Response(Configuration, ACCEPTED, A)
Active upload transaction A now exists
Python -> Firmware: Test Instruction(A, tick 0, declares UART0 length 6)
Python -> Firmware: Variable Instruction Data(A, tick 0, UART0, 6 bytes)
Firmware -> Python: Response(Tick 0, ACCEPTED, A)
Only now may Python submit tick 1
Python -> Firmware: Test Instruction(A, tick 1, no variable declarations)
Firmware -> Python: Response(Tick 1, ACCEPTED, A)
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

### Successful fixed-plus-variable tick acceptance

```text
Configuration for A was ACCEPTED
Python -> Firmware: Test Instruction(A, tick 0, declares SPI1 length 4)
Python -> Firmware: Variable Instruction Data(A, tick 0, SPI1, 4 bytes)
Firmware validates and accepts responsibility for retaining the complete tick
Firmware -> Python: Response(Tick 0, ACCEPTED, A)
Only now may Python submit tick 1
```

### Rejected or out-of-order tick

```text
Configuration for A expects next tick 1
Python -> Firmware: Test Instruction(A, tick 2)
Firmware -> Python: Response(Tick 2, REJECTED, INVALID_TICK, A)
Upload transaction A is invalidated
Remaining messages for outstanding tick 2 may be rejected
No later tick was submitted
Python restarts from Test Configuration with a fresh Test ID
```

### Wrong Test ID

```text
Active upload uses Test ID A
Python -> Firmware: Test Instruction(B, tick 0)
Firmware -> Python: Response(Tick 0, REJECTED, INCONSISTENT_TEST_ID, B)
Message B is not accepted into transaction A
Under the initial rejected-tick rule, active upload A is invalidated
Python restarts from Test Configuration with a fresh Test ID
```

### Successful complete-test validation

```text
All ticks 0..N-1 and declared variable data for A were ACCEPTED
Firmware automatically performs whole-test validation
Firmware -> Python: Response(Complete Test, ACCEPTED, A)
Python may now request START(A)
```

### Failed whole-test validation

```text
All individual ticks for A were accepted
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

### Complete fixed-plus-variable result transfer

```text
Firmware execution for A completes and results become available
Firmware -> Python: Test Result(A, tick 0, declares CAN0 length 8)
Firmware -> Python: Variable Result Data(A, tick 0, CAN0, 8 bytes)
...fixed results for every tick and every declared variable result...
Python successfully decodes the complete set for ticks 0..N-1
Python considers result transfer complete
Firmware releases retained results according to its Transport/storage policy
```

### Early execution failure with deterministic remaining results

```text
Test A has expected_tick_count 3 and START completed successfully
Firmware -> Python: Test Result(A, tick 0, OK, one analogue sample per channel)
Firmware detects an execution problem before tick 1 and may send Error(A, tick 1)
Firmware -> Python: Test Result(A, tick 1, EXECUTION_PROBLEM,
                                zero/ignored fixed values, no declarations)
Firmware -> Python: Test Result(A, tick 2, EXECUTION_PROBLEM,
                                zero/ignored fixed values, no declarations)
Python decodes all 3 fixed results and all declared variable data
Python considers normal result transfer complete despite the reported problem
```

### Transport session loss during upload

```text
Upload transaction A is active
Transport reports session loss/reset to firmware and Python integration
Transport itself does not mutate Application data
Integration invalidates upload A; remaining messages for its outstanding tick
may be rejected, and no later tick was submitted
Transport reconnects
Python restarts with Test Configuration and a fresh Test ID B
```

## Decisions still TODO

- final common envelope and body field order;
- fixed wire widths and byte order;
- compatibility/version negotiation and unknown-field behavior;
- extension-data and diagnostic schemas;
- communication flag bit assignments;
- higher-rate or multi-sample analogue capture;
- detailed Error and diagnostic taxonomy;
- whether future versions permit multipart Application variable data;
- versioned instruction-tick pipelining;
- upload/result resumption and result ranges;
- Application-level retransmission;
- result summary/finalization mechanisms, if ever required;
- production codec limits;
- golden wire vectors; and
- executable codec, firmware, and Python conformance tests.

Golden vectors and cross-endpoint executable conformance tests are required
before the protocol is considered implemented.
