# Application Layer codec and transaction design

> **Wire-format quick reference:** See [Application wire-format reference](application_wire_format.md)
> for the byte diagrams, fixed field widths, payload layouts, and literal System Information Request
> vector. This document focuses on API ownership, validation boundaries, and transaction design.

## Status and scope

The C Application layer is a stateless message codec. This foundation implements the common architecture-independent envelope, bounded encode/decode behaviour, exact complete-message length checks, common Test-ID/type/subtype validation, and the existing supported message-specific body paths. It does not implement firmware state machines, test-upload tracking, hardware control, Transport behaviour or Python bindings.

Existing partially implemented message families remain in place and return `HIL_APPLICATION_STATUS_NOT_IMPLEMENTED` where their sizing, variable storage, validation or body semantics are deliberately deferred.

The shared codec converts between typed data and exactly one complete Application message:

```text
typed firmware/Python-facing data
    -> HIL_APPLICATION_Encode_Message
    -> one complete Application message
```

Reception is the reverse. The codec expects the actual byte length of exactly one encoded Application message and rejects both truncation and trailing bytes.

## Ownership model

The shared Application protocol specification owns:

- typed message definitions and message direction;
- structural validation rules for typed and encoded messages;
- encoding and decoding of one complete Application message;
- Test ID, tick, peripheral, channel, Response-scope, and command correlation;
- the required order and meaning of protocol exchanges;
- Response scopes, outcomes, reasons, and their transaction effects;
- the meaning of accepted, rejected, completed, and failed operations;
- rules for creating, completing, and invalidating an Application transaction;
  and
- the actions each endpoint may perform after a particular Response.

The shared codec library implements only the typed/structural subset of that
contract. It does not retain the information needed to enforce cross-message
order or semantic transaction rules. Firmware and Python integration must
implement those rules around the codec. Message directions are normative, but
the codec is direction-neutral: each endpoint handler enforces sender direction
after decoding a structurally valid message.

The shared Application protocol and codec do not own:

- firmware states or state transitions;
- the firmware execution manager;
- hardware readiness or safety decisions;
- storage allocation or retention implementation;
- current test execution;
- firmware scheduling, interrupts, drivers, DMA, or RTOS behavior;
- the host Python client workflow implementation;
- Transport framing, sequencing, acknowledgements, flow control, or sessions;
  or
- mutable upload or result-transfer state inside the codec.

Firmware remains authoritative for its internal state and every transition. A
future Python client tracks its own progress through protocol exchanges. Neither
endpoint's internal state is the protocol state of the other endpoint, and this
design deliberately defines no public enum for protocol phases or firmware
states.

## Normal public API

Firmware and bindings include:

```c
#include "hil_rig_protocol/application/application.h"
```

| Operation | Current codec contract |
| --- | --- |
| `HIL_APPLICATION_Default_Config` | Populate a configuration accepted by `Init`, using a 512-byte default complete-message profile. |
| `HIL_APPLICATION_Init` | Validate/copy local structural limits; reduced valid complete-message maxima are accepted, and `config` may alias `context->config`. |
| `HIL_APPLICATION_Encoded_Size` | Clear output, validate the common envelope fields, dispatch existing body sizing, add the fixed header with checked arithmetic. |
| `HIL_APPLICATION_Encode_Message` | Encode the bounded common header and selected existing body; publish a nonzero size only after complete success. |
| `HIL_APPLICATION_Decode_Storage_Size` | Bounded-parse one complete message, validate the exact width of supported fixed bodies, enforce configured Test Configuration extension storage limits, and report required storage; unfinished variable-storage families return `NOT_IMPLEMENTED`. |
| `HIL_APPLICATION_Decode_Message` | Decode exactly one complete message; body decoders see only the declared payload extent. |
| `HIL_APPLICATION_Validate_Message` | Perform common typed validation and existing message-specific validation. |
| `HIL_APPLICATION_Validate_Encoded_Message` | Reuse the bounded decode path without publishing caller output. |

### Current message-family implementation status

This foundation deliberately does not complete every message family. The current public façade behaviour is:

| Family | Current status |
| --- | --- |
| System Information Request | Encode, decode, validate and encoded-size calculation implemented. |
| System Information Response | Existing encode/decode body retained; typed validation requires the canonical compiled protocol version and structurally valid byte spans. Message-specific encoded-size and decode-storage sizing remain unfinished. |
| Test Configuration | Fully supported by the codec: typed validation, body sizing, encode/decode, decode-storage scanning, fixed Digital/Analogue/PWM/CAN/SPI/UART/I2C arrays, and the length-delimited extension are implemented. The extension has a 255-byte wire maximum and is additionally bounded by `context->config.max_variable_data_size`. |
| Test Instruction | Fully supported fixed codec family: 50-byte payload / 73-byte complete message, fixed sizing, encode/decode, zero decode storage, Digital/PWM/tick structural validation, and encoded-message validation are implemented. Variable declarations/data remain deferred. |
| Execution Control | Existing encode/decode/validation retained, including zero reserved flags; message-specific encoded-size calculation remains unfinished. |
| Global Control | Existing encode/decode/validation retained, including zero reserved flags; message-specific encoded-size calculation remains unfinished. |
| Test Result | Fully supported fixed codec family: 39-byte payload / 62-byte complete message, fixed sizing, encode/decode, zero decode storage, Digital/PWM/tick/condition structural validation, and encoded-message validation are implemented. Variable declarations/data remain deferred. |
| Variable Instruction Data | Reserved structures and identifiers retained; body/storage workflow remains `NOT_IMPLEMENTED`. |
| Variable Result Data | Reserved structures and identifiers retained; body/storage workflow remains `NOT_IMPLEMENTED`. |
| Application Response | Existing structures/body code retained; public validation and message-specific sizing remain `NOT_IMPLEMENTED`. |
| Application Error | Existing structures/body code retained; public validation/sizing and variable diagnostic-storage sizing remain `NOT_IMPLEMENTED`. |

`HIL_APPLICATION_Encoded_Size` propagates a message-specific `NOT_IMPLEMENTED` result for unfinished families rather than guessing body sizes. Test Configuration, fixed Test Instruction, and fixed Test Result are supported. Analogue range/hardware feasibility, variable-data correlation, active-Test-Configuration tick comparison, and endpoint transaction state remain later integration or protocol work.

## Public C Application-to-Transport integration coverage

The public C integration suite now carries the currently supported fixed
Application subset through the existing Transport pair harness. Complete Test
Configuration messages are 220 through 475 bytes for extension lengths 0 through
255, fixed Test Instructions are 73 bytes, and fixed Test Results are 62 bytes.
The default maximum complete Application message and default Transport maximum
Application-message payload are both 512 bytes.

The integration tests include only public Application and Transport headers and
exercise exact opaque-byte preservation, the 475-byte capacity boundary,
byte-stream chunking, reliable retry, Transport-valid malformed/invalid
Application input, and Transport corruption before Application exposure. The
Application codec remains stateless and direction-neutral. Any configuration,
tick, complete-test, or execution-completed checkpoint in an ordered scenario is
test-owned orchestration only, not an encoded Application Response or retained
codec state. A Transport `DELIVERY_CONFIRMED` event confirms Transport delivery
only and is not Application semantic acceptance.

The ordered scenario is therefore a fixed-subset message-path test, not a
complete response-gated Application transaction. Full response-gated transaction
coverage remains incomplete until Application Responses and the remaining
control-family sizing are implemented.

## Common wire version and envelope

The codec uses the repository-wide `HIL_RIG_PROTOCOL_VERSION_MAJOR` and `HIL_RIG_PROTOCOL_VERSION_MINOR` values from `hil_rig_protocol/version.h`. There is no independent Application version constant. The common envelope is summarized below; the normative byte diagrams and payload layouts are in the
[Application wire-format reference](application_wire_format.md).

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | overall protocol major |
| 1 | 1 | overall protocol minor |
| 2 | 1 | Test-ID-present flag |
| 3 | 16 | Test ID |
| 19 | 1 | message type |
| 20 | 1 | message subtype |
| 21 | 2 | payload length, little-endian `uint16_t` |
| 23 | N | payload |

The decoder initially requires an exact major/minor match. A missing Test ID is flag zero followed by sixteen zero bytes; an all-zero Test ID remains valid when the flag is one. There is no payload-end marker. Native enum widths, native byte order, `size_t`, C padding and pointer representation never define this envelope.

## Stateless context and single owner

`HIL_Application_Context_T` contains only:

- a copied `HIL_Application_Config_T`; and
- an `initialized` flag.

The context stores no caller pointers, messages, encoded output, decode storage,
active Test ID, expected tick, outstanding variable declaration, accepted tick,
result progress, retention ownership, execution state, endpoint role, selected
protocol version, Application request identity, outstanding operation, or
transaction status. It remains a stateless codec context and must not gain
mutable transaction data.

Fields are visible so firmware and bindings can allocate the context without a
heap or private-size query, but are library-private. Callers initialize it
through `HIL_APPLICATION_Init` and must not mutate fields directly. With no
mutable transaction or queue state, no codec reset operation is required.

Each context still has one owning task, thread, or execution context. All
operations for that context are expected to run in that owner. Splitting calls
across tasks, callbacks, or interrupts is unsupported even with external
locking. The library adds no locks, atomics, callbacks, or RTOS dependencies.
Separate contexts may have separate owners.

After successful initialization, sizing, validation, encoding, and decoding are
logically read-only with respect to the context.

## Structural codec configuration and message bounds

`HIL_Application_Config_T` supplies explicit bounds for:

- maximum complete encoded Application message size;
- maximum variable byte-span size;
- maximum variable-data declarations in one typed tick; and
- maximum permitted `expected_tick_count` field value and exclusive structural ceiling for fixed instruction/result `tick_number`.

These are local structural limits or reserved local policy bounds.
`max_encoded_message_size` is an operational bound, not a requirement to
provision the theoretical representable maximum. All Test Configuration arrays
are protocol-sized by the physical-channel constants. Element `i` is logical
channel `i`; there is no caller-configurable peripheral count and no sparse
configuration list. These values reserve no test storage and do not mean the
codec can retain or track the configured number of ticks. Endpoint integration
separately decides whether retention and hardware capacity are available.

`max_variable_data_size` bounds the Test Configuration extension byte span,
each Test Configuration communication-capture limit, and the byte spans used by
variable Application families. The extension still has an absolute 255-byte
wire maximum because its encoded length is one byte.
`max_encoded_message_size` bounds each complete encoded Application message.
They do not describe one monolithic test or tick package. Integration must
configure Transport's maximum Application-message size to be at least the
Application Layer's `max_encoded_message_size`; the codec does not inspect or
change Transport configuration.

`HIL_APPLICATION_Default_Config` uses a 512-byte complete-message profile and
the existing structural maxima for the other fields.
`HIL_APPLICATION_Init` accepts any complete-message maximum from
`HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE` (25 bytes: the 23-byte envelope plus
the two-byte System Information Request payload) through
`HIL_APPLICATION_HEADER_SIZE_BYTES + UINT16_MAX`, validates the other configured
limits, and copies the configuration without allocation or pointer retention.

Test Configuration validation enforces a nonzero `expected_tick_count` not
greater than `context->config.max_expected_tick_count`, the supported tick
durations, zero test-wide flags, canonical disabled records, valid protocol
enums/Booleans, PWM structural limits, communication rate/capture constraints,
and the UART/I2C structural combinations defined by the wire protocol. It does
not validate hardware availability, exact supported rates, power state, MCU
timing, cross-driver conflicts, complete-test retention capacity, or workflow
state.

All fixed configuration arrays are protocol-sized rather than caller-configured.
Their named channel counts and deterministic index mappings are described in
[Application messages](application_messages.md).

## Test ID integration contract

`HIL_Application_Test_Id_T` is an opaque 16-byte codec value. The codec may
structurally carry any 16-byte sequence, never generates an ID, and assigns no
timestamp, ordering, or arithmetic meaning to the bytes.

For every new upload, Python integration must:

1. generate a fresh random 128-bit Test ID;
2. use the same 16 bytes in every message belonging to that test; and
3. generate another fresh value when a transaction must restart.

Firmware integration compares all 16 bytes when correlating a message with its
active transaction. Application Test ID and Transport session identity are
unrelated. Transport reconnect does not preserve, derive, or replace a Test ID.

## Normative message directions

The initial protocol directions are exact:

| Python to firmware | Firmware to Python |
| --- | --- |
| System Information Request | System Information Response |
| Test Configuration | Test Result |
| Test Instruction | Variable Result Data |
| Variable Instruction Data | Application Response |
| Execution Control | Application Error |
| Global Control | |

The shared codec remains stateless and direction-neutral. Successfully encoding
or decoding a structurally valid message does not determine which endpoint is
using it. Firmware and Python integration handlers enforce these directions
after decoding; the codec context contains no endpoint role.

## Encoding and decoding ownership

Encoding and decoding borrow all caller pointers only for the duration of a call. A successful encode publishes exactly header-plus-payload bytes. A failed encode leaves `output_size == 0`; the buffer contents are unspecified and are not cleared as a side effect.

Decoding receives the actual byte count of one complete message. It requires a
complete fixed header, validates repository-wide major/minor version bytes,
checks header-plus-payload arithmetic before addition, and requires the supplied
input length to equal the declared total. Input shorter than the envelope-declared
total is `TRUNCATED_MESSAGE`; trailing bytes are `MALFORMED_MESSAGE`. The selected
body decoder receives exactly the declared payload length and must consume it
exactly. A declared fixed body that is too short is `MALFORMED_MESSAGE`. On every
failed decode, the public output type is `INVALID` and decoded-storage usage is
zero; the remaining output contents are unspecified.

Byte spans use an encoded one-byte length. The decoder proves the length field
and declared bytes are present before access. Missing encoded span bytes are
`MALFORMED_MESSAGE`; `BUFFER_TOO_SMALL` is reserved for insufficient
caller-provided decoded-data storage. After proving caller storage capacity, the
decoder copies exactly the declared data bytes and only then publishes the output
span pointer.

Wire lengths and multi-byte body fields use fixed-width little-endian encodings. `size_t` remains appropriate only for local capacities, indexes and memory sizes.

## Validation and rejection boundaries

There are three distinct failure boundaries.

### 1. Transport-invalid data

Transport framing, integrity, ordering, and session rules run before Application
decoding. Data rejected by Transport is not an Application message and produces
no Application Response merely because Transport rejected it.

### 2. Structurally invalid Application data

The current foundation checks the common structural boundary: exact repository-wide major/minor version, one-byte message type/subtype representation, Boolean Test-ID presence and its zero-fill rule, configured complete-message bounds, checked header-plus-payload arithmetic, declared payload extent, exact complete-message consumption, and the existing typed validation implemented for each supported fixed family.

Feature-specific semantic validation remains deliberately deferred where the corresponding family is unfinished. Test Configuration is structurally complete in the codec, including Digital/Analogue/PWM and CAN/SPI/UART/I2C records. Fixed Test Instruction and Test Result validation is also implemented: ticks must be below `context->config.max_expected_tick_count`, Digital values are Boolean, PWM duty is `0..10000` with zero duty required for a zero period, and Test Result conditions are limited to `OK`, `PARTIAL`, and `EXECUTION_PROBLEM`. Analogue ranges, hardware capability checks, variable-data declaration/correlation rules, comparison with an active Test Configuration's actual `expected_tick_count`, and stateful transaction ordering remain outside this codec.

Failures are local `HIL_Application_Status_T` values. They are not serialized as Application Responses or Errors. The codec has no active Test Configuration and cannot compare a later Test ID/tick against an active transaction. Its fixed tick check is only the configured structural ceiling, not the uploaded test's actual tick count.

### 3. Structurally valid but semantically unacceptable data

Endpoint integration returns an Application Response when a structurally valid
message cannot be accepted or an operation cannot be performed. Examples are:

- Test ID does not match the active transaction;
- tick is outside `0..N-1` or arrives out of order;
- variable data is missing or inconsistent with its declaration;
- hardware configuration is unsupported or unsafe;
- retention capacity is insufficient;
- START arrives before Complete Test acceptance; or
- the firmware execution manager rejects an execution request.

Use the most specific Response reason. `OPERATION_NOT_ALLOWED` describes a
structurally valid request that cannot currently be performed when no more
specific reason applies. Semantic rejection is not a codec or Transport failure.

## Response outcome contract

| Outcome | Meaning |
| --- | --- |
| `ACCEPTED` | Data for a configuration, tick, or complete test was semantically validated and accepted with its scope-specific retention/transaction effect. |
| `REJECTED` | A structurally valid message or request was semantically unacceptable; the requested acceptance/operation did not occur. |
| `COMPLETED` | A requested Execution Control or Global Control operation was actually performed. |
| `FAILED` | Processing or an operation began but could not complete; the sender must follow the scope-specific recovery contract and must not assume success. |

For configuration, tick, and Complete Test scopes, successful Responses use
`ACCEPTED`. For Execution Control and Global Control scopes, successful
Responses use `COMPLETED`. The codec can eventually validate permitted
scope/outcome/correlation combinations, but only integration decides the real
outcome.

## Outstanding response-requiring operation

The MVP has no Application request ID or sequence number. To keep Responses
unambiguous, Python may have only one response-requiring Application operation
outstanding at a time. The fixed Test Instruction for tick T and every Variable
Instruction Data message it declares collectively count as one outstanding Tick
operation; the existing tick-level stop-and-wait rules remain mandatory.

While awaiting a Response, Python must not repeat an indistinguishable System
Information Request, Test Configuration, START, ABORT, or RESET_APPLICATION
request. After the final Tick Response, automatic whole-test validation remains
the outstanding response-requiring operation until the Complete Test Response;
Python must receive it before submitting START. Result messages do not require
Responses and are governed separately by their deterministic ordering contract.

If Transport/session failure makes an operation's outcome uncertain, Python
enters recovery rather than blindly retrying it. After explicitly abandoning
the previous operation, Python may send RESET_APPLICATION as part of recovery;
it must ignore any Response for the abandoned operation that arrives later.
This serialization is an Application integration rule. It adds no request ID,
sequence number, endpoint role, or outstanding-operation tracking to the codec
context.

## Initial message transaction contract

The following table replaces any shared protocol-phase or firmware-state model.

| Exchange | Sender -> receiver | Required correlation | Protocol prerequisite | Successful Response and transaction effect | Rejection/failure and recovery | Integration-owned decisions |
| --- | --- | --- | --- | --- | --- | --- |
| Test Configuration | Python -> firmware | Fresh Test ID; no tick | No active upload being continued; exactly one configuration starts a new upload attempt | Configuration `ACCEPTED` creates the active upload transaction for that Test ID | `REJECTED`/`FAILED` creates no transaction; host starts a new upload from Test Configuration with a fresh Test ID | Hardware support, safety, timing, and retention capacity |
| Test Instruction plus declared Variable Instruction Data | Python -> firmware | Active Test ID, zero-based tick, and peripheral/channel for each variable message | Configuration accepted; tick T - 1 accepted before T is submitted; each unique nonzero declaration has one separate matching variable message | Tick `ACCEPTED` means the complete fixed-plus-variable tick was validated and accepted for retention, so the host may submit tick T + 1 | Any rejected tick or associated variable data invalidates the upload; restart from Test Configuration; remaining messages for that outstanding tick may be rejected and no later tick may already have been submitted | Active-ID/tick tracking, completeness, duplicate-message detection, retention mechanism, and cleanup |
| Automatic whole-test validation | Firmware -> Python Response | Active Test ID; Complete Test scope; no tick | Every tick `0..N-1` and all declared variable data accepted | Complete Test `ACCEPTED` means the complete test was retained, validated, and is available for a later START request; upload is complete | `REJECTED`/`FAILED` invalidates the transaction; restart from Test Configuration | Whole-test consistency and release of invalid retained data |
| START | Python -> firmware | Accepted test's Test ID and START command | Complete Test `ACCEPTED` was received for that Test ID | Execution Control START `COMPLETED` means firmware performed the start request | `REJECTED` means execution did not start; `FAILED` requires recovery and the host must not assume execution status | Execution-manager permission, hardware readiness, actual firmware transitions, and whether retry is safe |
| ABORT | Python -> firmware | Identified active Test ID and ABORT command | A matching upload, accepted test, execution, or result transaction exists | Execution Control ABORT `COMPLETED` means safe stop/abandonment was performed and the previous transaction cannot continue normally; a new upload starts from Test Configuration | `REJECTED` means abort was not performed; `FAILED` requires firmware-specific recovery and no assumed cleanup | Execution-manager transitions, safe stop, retained-data cleanup, and firmware recovery |
| Test Result plus declared Variable Result Data | Firmware -> Python | Accepted Test ID, zero-based tick, and peripheral/channel for variable messages | START completed; firmware execution completed or stopped early and results are available | Exactly one fixed result is sent for every tick `0..N-1` in increasing order; each fixed result is followed by its declared variable results in declaration order before the next fixed result; successful decode of the full sequence completes normal transfer | Early execution failure uses `EXECUTION_PROBLEM` fixed results without changing ordering; session loss, reset, or inability to communicate instead reports recovery required because completion cannot be guaranteed | Capture validity, storage/retention mechanics, Transport handoff, and release policy |
| RESET_APPLICATION | Python -> firmware | No Test ID; Global Control scope | May be requested independently of a known test | Global Control `COMPLETED` means active Application transaction data and recoverable Application protocol conditions were cleared | `REJECTED`/`FAILED` means the host cannot assume cleanup and must follow firmware recovery policy | Mapping to internal firmware state, cleanup, and whether reset can be completed |
| Application Error | Firmware -> Python | Test ID/tick present only when known and relevant | A broader fault exists rather than rejection of one request | No implicit transaction success; integration interprets category/recoverability | Host follows indicated recovery, commonly ABORT or RESET_APPLICATION | Error generation, firmware state, hardware response, and diagnostics |

## Required upload and execution sequence

The initial contract is:

1. The Python host generates a fresh random 128-bit Test ID.
2. It sends exactly one Test Configuration carrying that ID.
3. Firmware structurally decodes it, performs firmware-specific semantic checks,
   and returns a configuration-scoped Response.
4. Configuration `ACCEPTED` creates the active upload transaction.
5. The host sends the fixed Test Instruction for zero-based tick T.
6. Each declaration, all of which is nonzero and unique by channel, is followed
   by its one separately encoded Variable Instruction Data message with matching
   Test ID, tick, peripheral, and channel.
7. Firmware returns a Tick Response only after the complete fixed-plus-variable
   tick was received, validated, and accepted for retention. The host submits
   tick T + 1 only after receiving Tick T `ACCEPTED`.
8. After ticks `0..N-1` and all declared variable data are accepted, firmware
   automatically performs whole-test validation.
9. Complete Test `ACCEPTED` means the test was retained, validated, and is
   available for a subsequent START request.
10. The host sends START separately.
11. Firmware asks its execution manager whether execution can begin, performs
    the request if allowed, and reports the actual outcome.
12. Firmware makes exactly N fixed results available after execution completes
    or stops early, using `EXECUTION_PROBLEM` for ticks without valid execution
    or capture data, except when communication/reset prevents complete transfer.
    It sends fixed results in increasing tick order; each fixed result is
    followed by its declared variable results in declaration order before the
    next fixed result.

There is no Begin Upload, ARM, or FINALIZE_TEST command. Test Configuration
acceptance starts upload; Complete Test acceptance completes upload; START is a
separate request whose success is never predicted by the codec or host.

Only one tick may await semantic acceptance. Its fixed message and associated
variable messages collectively form that outstanding tick. Transport delivery
and Application semantic acceptance remain independent: a Transport ACK confirms
reliable frame/byte delivery, while Tick `ACCEPTED` confirms acceptance and retention of
the complete tick. Stop-and-wait is an initial Application transaction rule,
not Transport state or firmware execution-manager state. Versioned capability
negotiation may permit future pipelining.

## Transaction invalidation and recovery

The initial recovery rules are normative:

- rejected Test Configuration creates no upload transaction;
- a rejected tick or associated variable-data message invalidates the active
  upload transaction;
- failed whole-test validation invalidates the active transaction;
- after invalidation, the host must restart from Test Configuration with a fresh
  random Test ID;
- remaining fixed/variable messages for the one outstanding invalidated tick
  may be rejected with `INCONSISTENT_TEST_ID`, `INVALID_TICK`, or
  `OPERATION_NOT_ALLOWED` as appropriate; no later tick may already have been
  submitted; and
- firmware decides how retained data is released and whether its internal state
  or execution manager needs any recovery action.

Invalidation is a protocol effect on the identified transaction, not a shared
firmware transition. Documentation and Responses must not describe it as
entering or returning to a protocol-defined firmware state.

## Execution and recovery controls

Execution Control contains only `START` and `ABORT` in the initial design. Both
are requests to firmware integration, both require a Test ID, and decoding
either command performs nothing.

For START, Complete Test `ACCEPTED` is the protocol prerequisite. Firmware still
queries its execution manager and hardware policy. START `COMPLETED` means the
requested start operation was performed. START `REJECTED` means it was not
performed and does not imply codec or Transport failure.

ABORT asks firmware to safely stop or abandon the identified active transaction
or operation. ABORT `COMPLETED` means the operation was performed and that
transaction can no longer continue normally. A new upload must begin from Test
Configuration unless a future version defines resumption. Firmware owns actual
execution-manager transitions and cleanup.

No additional command exists solely to force firmware into a named state.
Global `RESET_APPLICATION` remains test-independent and carries no Test ID. It
requests clearing active Application transaction data and recoverable protocol
conditions, but never resets or reconnects Transport. Firmware decides how
reset maps to its own internal state and whether it can be completed.

## Result-transfer completion

After START is successfully completed for a test with `N` ticks, firmware
produces exactly one fixed Test Result for every tick `0..N-1`. The Python host
considers normal result transfer complete only after it has successfully
decoded:

- one fixed Test Result for every tick `0..N-1`; and
- every Variable Result Data message declared by those fixed results.

Firmware sends fixed Test Results in increasing order from tick 0 through
`N-1`. For tick T, it sends the fixed result first, then every Variable Result
Data message in the order its declarations appear, and completes those messages
before sending the fixed result for tick T+1. Variable data never precedes the
fixed result that declares it. Result messages have no Application Response or
Application-level stop-and-wait acknowledgement. Transport acknowledgement and
retransmission remain Transport responsibilities. Fragmentation/reassembly
is outside the Transport MVP.

If execution stops or fails before all ticks execute, firmware still produces
the remaining fixed results. Each tick without valid execution/capture data uses
`HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM`; all fixed captured-value
fields remain present for structural consistency but are semantically invalid
and Python ignores them. Such a result declares no variable data unless valid
variable data actually exists. An Application Error may be sent when the
problem is detected, but it does not replace any of the N fixed results.

Result conditions have exact MVP meanings:

- `OK`: every configured fixed capture represented by the result is valid, and
  every variable declaration identifies valid variable data that follows.
- `PARTIAL`: every configured fixed capture remains valid, but one or more
  requested variable communication captures failed or are incomplete;
  declarations identify only valid variable results that follow.
- `EXECUTION_PROBLEM`: the complete set of fixed captured values for that tick
  is semantically invalid and Python ignores it. Valid variable data may still
  be declared if any exists.

If any configured fixed capture cannot be trusted, firmware must use
`EXECUTION_PROBLEM`; the initial protocol cannot express selective validity
among fixed digital, analogue, or PWM fields. Fixed capture channels disabled
or absent from configuration are encoded deterministically as zero and ignored
by Python. Their presence alone causes neither `PARTIAL` nor
`EXECUTION_PROBLEM`. Each configured analogue input contributes exactly one
sample per fixed result at the test tick rate. There is no independent analogue
sample rate, multi-sample result, validity mask, result-finalization message, or
result-summary message in the MVP.

The host correlates all result messages by Test ID, tick, peripheral, and
channel. No shared phase transition or simultaneous endpoint state change
is implied by completion.

Firmware considers its result set handed off according to the surrounding
Transport integration after every complete encoded fixed and variable result
message has been accepted for delivery. Whether firmware waits for a Transport
delivery acknowledgement before releasing retained results is Transport and
firmware policy, not an Application codec rule. The initial Application design
has no per-result Response or completion message.

Early execution failure does not change result ordering. An Application Error
may report a problem when detected, but it does not replace or reorder the
required result set. Future pipelining, interleaving, ranges, or out-of-order
result delivery require a versioned extension.

Transport/session loss, reset, or inability to communicate is the exception to
the complete-set guarantee. Integration reports recovery is required rather
than claiming normal completion. Result ranges, resume after reconnect,
Application-level retransmission, higher-rate analogue capture, result
summaries, result pipelining/interleaving, and partial-result recovery remain
deferred.

## Transport session loss

Transport does not directly create, complete, or invalidate Application data.
It reports session loss/reset to endpoint integration. Under the initial
Application integration policy, session loss during upload invalidates the
active upload; remaining messages for its one outstanding tick may be rejected,
and the Python host starts a new upload from Test Configuration with a fresh
Test ID after reconnect. No later tick may already have been submitted.

If session loss interrupts result transfer, the N-result guarantee cannot be
met and resumption is not defined by this version. The client reports recovery
is required rather than assuming which results firmware retained.
`RESET_APPLICATION` likewise ends any guarantee that a pending complete result
set can be communicated; it remains an Application request and does not reset or
reconnect Transport.

## Future firmware integration

The intended firmware boundary is:

1. Transport returns one complete Application message from one MVP frame.
2. Firmware calls the shared Application decoder and structural validator.
3. A future firmware Application handler examines the decoded message.
4. That handler tracks transaction data such as active Test ID, expected tick,
   outstanding variable declarations, accepted/retained ticks, and results.
5. It queries storage, hardware, and execution-manager modules for semantic
   decisions.
6. It constructs the appropriate typed Application Response, Error, or result
   message.
7. The shared codec encodes that one complete message.
8. Firmware submits the encoded message to Transport.

The handler is not part of this PR or the stateless codec. It belongs in
`hil-rig-mcu-firmware`. Its transaction bookkeeping remains separate from the
execution manager's authoritative firmware state. This repository must not add
execution-manager headers, callbacks, state enums, or firmware-specific
dependencies.

For a successfully started N-tick test, that handler also ensures the result
transaction contains N fixed results even after early execution failure, plus
exactly the variable messages declared by those fixed results. It enforces the
shared order: increasing fixed-result ticks, with each tick's declared variable
messages in declaration order before the next fixed result. Storage, hardware
capture, retention, and Transport handoff mechanics remain firmware-owned. This
is handler bookkeeping around the codec, not mutable codec state or an
independent firmware choice of result ordering.

## Future Python integration

The intended Python boundary is:

1. A future client method creates typed configuration, instruction, or control
   data.
2. Python uses the shared C codec through bindings to encode one complete
   Application message.
3. Python submits that message through its Transport integration.
4. Received complete Application messages are decoded through the same codec.
5. The client correlates Responses using Test ID, scope, tick, and command.
6. It serializes response-requiring operations, tracks client-side progress such
   as accepted configuration, the one outstanding upload tick, Complete Test
   acceptance, START outcome, and validates the ordered sequence of all N fixed
   results and every variable result declared by them.
7. It exposes success, rejection, protocol mismatch, and recovery requirements
   to the API user.

Python may prevent an obviously invalid local action such as requesting START
before Complete Test acceptance. It must stop configuration/execution after
detecting an incompatible repository-wide protocol version and enter recovery when a pending
operation's outcome becomes uncertain. It must not predict firmware hardware
readiness or execution-manager state; firmware Responses are authoritative.

The Python client, bindings, serial/USB integration, asynchronous behavior,
exceptions, and transaction controller are future work and are not implemented
in this PR.

## Remaining conformance work

Public C integration now executes the supported fixed Configuration,
Instruction, Result, and existing Execution Control START paths through
Transport. The ordered fixed-subset scenario uses test-owned semantic checkpoints
rather than Responses and does not represent a complete production transaction.

Future conformance work still includes:

- Variable Instruction Data and Variable Result Data, including their deferred
  declarations and storage workflows;
- Application Response and Application Error, plus completion of System
  Information Response support;
- typed sizing for Execution Control and Global Control;
- golden wire vectors shared with future language bindings;
- production firmware and Python endpoint integration for Test-ID correlation,
  tick ordering, semantic acceptance, retention, execution, and recovery; and
- expanded executable conformance coverage as deferred message families become
  implemented.

Advanced upload/result resumption, range requests, and Application-level
retransmission remain intentionally deferred. Multi-version encoding,
backward-compatible decoding, minor-version compatibility, and result
pipelining/interleaving also require a future versioned API.
