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
semantic outcome.

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
| Test Instruction | Python | Firmware | Active Test ID and tick | Configuration accepted; expected next zero-based tick | Tick Response after all declared data |
| Variable Instruction Data | Python | Firmware | Active Test ID, tick, peripheral, channel | Matching nonzero declaration in fixed instruction | Included in correlated Tick Response |
| Execution Control START | Python | Firmware | Accepted Test ID and START | Complete Test `ACCEPTED` | Execution-Control Response reports actual operation outcome |
| Execution Control ABORT | Python | Firmware | Identified active Test ID and ABORT | Matching active transaction/operation | `COMPLETED` prevents previous transaction continuing normally |
| Global Control RESET_APPLICATION | Python | Firmware | No Test ID | None | `COMPLETED` clears active Application transaction data/conditions; Transport unchanged |
| Test Result | Firmware | Python | Accepted Test ID and tick | Execution complete and result available | No per-result Response in initial model |
| Variable Result Data | Firmware | Python | Test ID, tick, peripheral, channel | Matching declaration in fixed result | No per-result Response in initial model |
| Application Response | Usually firmware | Usually Python | Scope-dependent | A correlated request/data acceptance decision | Carries semantic outcome and transaction effect |
| Application Error | Usually firmware | Usually Python | Optional Test ID/tick | Broader fault rather than one request rejection | Integration-dependent recovery |

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
- communication rate: bits per second; and
- analogue sample rate: hertz.

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
- authoritative `expected_tick_count` N, defining ticks 0 through N - 1;
- future versioned `flags`, initially zero;
- typed `peripherals` and `peripheral_count`; and
- reserved length-delimited `extension_data`.

The codec eventually validates tag/member and basic unit/count consistency.
Firmware validates supported hardware, electrical ranges, rates, timing, and
retention capacity.

A configuration-scoped `ACCEPTED` Response creates the active upload
transaction. `REJECTED` or `FAILED` creates no transaction; instructions for
that Test ID are not accepted, and the host starts a new upload from Test
Configuration with a fresh ID.

## Zero-based tick numbering

Tick numbering is normative and shared by instructions, variable data, fixed
results, variable results, Responses, and Errors that identify a tick. For
`expected_tick_count == N`, valid ticks are exactly 0 through N - 1.

Instructions are sent in increasing order. The codec may structurally bound
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

Each nonzero `HIL_Application_Data_Declaration_T::byte_length` requires exactly
one matching Variable Instruction Data message in the initial design. A zero
declaration requires no variable message.

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
- every nonzero declaration has exactly matching bytes;
- no required declaration is missing;
- semantic validation succeeds; and
- integration takes responsibility for retaining the complete fixed and
  variable tick data.

Tick `ACCEPTED` allows the host to continue. It does not require a completed
NAND write: retention may be NAND, RAM, or an accepted storage-manager queue.

A rejected tick or associated variable-data message invalidates the upload.
The host restarts from Test Configuration with a fresh Test ID. Messages already
in flight for the invalidated transaction may also be rejected.

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

Firmware may make Test Results available after execution completes.
`HIL_Application_Test_Result_T` contains:

- zero-based `tick_number` matching the instruction;
- all 10 captured digital input values;
- both captured analogue input values;
- both captured PWM input measurements;
- variable-result declaration array/count;
- result `condition`: `OK`, `PARTIAL`, or `EXECUTION_PROBLEM`; and
- integration-defined `problem_detail`.

Problems detected while executing are Application Error messages. A non-OK
fixed-result condition is a later summary and does not replace the earlier Error.

## Variable Result Data and completion

- Type: `VARIABLE_RESULT_DATA`
- Subtype: `NONE`
- Test ID: required
- Direction: firmware to Python

Each nonzero fixed-result declaration is followed by one matching complete
Variable Result Data message correlated by Test ID, tick, peripheral, and
channel.

For a test with N ticks, Python considers result transfer complete only after
successfully decoding every fixed Test Result for ticks `0..N-1` and every
variable message declared by those results.

Firmware considers all result messages handed off according to its surrounding
Transport integration after every complete encoded result message has been
accepted for delivery. Whether retained results wait for a Transport delivery
acknowledgement is firmware/Transport policy. Application defines no simultaneous
endpoint state change, per-result Response, or completion command.

Result ranges, resume after reconnect, and Application-level retransmission are
deferred.

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

Responses do not create mandatory stop-and-wait. Several instruction/data
messages may be in flight subject to Transport flow control, but later delivery
does not revive an invalidated transaction.

## Application Error

- Type: `ERROR`
- Subtype: `NONE`
- Test ID: optional
- Direction: primarily firmware to Python

Initial categories are Hardware, Execution, Timeout, Retained Data, Protocol,
and Internal. A test-specific error carries the Test ID when known; a global
hardware/power error may omit it. An Error is not a rejection of one request,
not a local codec status, and not a Transport corruption/delivery failure.

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
- peripheral configuration tag/channel consistency;
- digital values limited to 0/1;
- PWM duty no greater than 10000;
- nonzero Test Configuration tick duration; and
- Application Error tick-presence consistency.

Firmware/Python integration separately validates active Test ID, tick range and
order, declaration matching/completeness, retention capacity, whole-test
consistency, transaction prerequisites, hardware support/safety, and
execution-manager decisions.

Structurally malformed examples include a forbidden/missing Test ID, wrong
subtype, NULL pointer with nonzero count, invalid channel family, inconsistent
length, overflow, invalid fixed value, incomplete fixed array, or Response with
irrelevant correlation fields populated.

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
Python may continue with tick 1
```

### Rejected or out-of-order tick

```text
Configuration for A expects next tick 1
Python -> Firmware: Test Instruction(A, tick 2)
Firmware -> Python: Response(Tick 2, REJECTED, INVALID_TICK, A)
Upload transaction A is invalidated
In-flight messages for A may be rejected
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

### Transport session loss during upload

```text
Upload transaction A is active
Transport reports session loss/reset to firmware and Python integration
Transport itself does not mutate Application data
Integration invalidates upload A; in-flight messages may be rejected
Transport reconnects
Python restarts with Test Configuration and a fresh Test ID B
```

## Decisions still TODO

- final common envelope and body field order;
- fixed wire widths and byte order;
- compatibility/version negotiation and unknown-field behavior;
- extension-data and diagnostic schemas;
- communication flag bit assignments;
- detailed Error and diagnostic taxonomy;
- whether future versions permit multipart Application variable data;
- upload/result resumption and result ranges;
- Application-level retransmission;
- production codec limits;
- golden wire vectors; and
- executable codec, firmware, and Python conformance tests.

Golden vectors and cross-endpoint executable conformance tests are required
before the protocol is considered implemented.
