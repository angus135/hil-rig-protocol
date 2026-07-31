# Application Layer codec and transaction design

## Status and scope

This document defines the intended public C message-codec API and the shared
Application transaction contract. Every function in `src/application/` remains
an intentional `HIL_APPLICATION_STATUS_NOT_IMPLEMENTED` stub. No binary
encoding, decoding, structural validation, firmware handler, Python client,
storage manager, execution integration, hardware behavior, or runtime
transaction logic is implemented by this PR.

The shared codec converts between typed data and exactly one complete
Application message:

```text
typed firmware/Python-facing data
    -> HIL_APPLICATION_Encode_Message
    -> one complete Application message
    -> Transport
    -> possibly several Transport frames
```

Reception is the reverse:

```text
Transport
    -> one complete reassembled Application message
    -> HIL_APPLICATION_Decode_Message
    -> typed firmware/Python-facing data
```

Transport fragmentation is invisible to the codec. The codec never receives a
partial Transport frame or combines Transport fragments.

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
implement those rules around the codec.

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

| Operation | Future codec contract |
| --- | --- |
| `HIL_APPLICATION_Default_Config` | Initialize structural bounds to deterministic policy-disabled values. |
| `HIL_APPLICATION_Init` | Validate and copy structural codec bounds into a lightweight context. |
| `HIL_APPLICATION_Encoded_Size` | Validate typed structure and report exact future encoded size. |
| `HIL_APPLICATION_Encode_Message` | Produce one complete Application message in caller output. |
| `HIL_APPLICATION_Decode_Storage_Size` | Validate complete encoded structure and report caller decode workspace. |
| `HIL_APPLICATION_Decode_Message` | Decode one complete message into typed caller-owned storage. |
| `HIL_APPLICATION_Validate_Message` | Perform typed codec-level structural validation. |
| `HIL_APPLICATION_Validate_Encoded_Message` | Validate complete encoded structure without publishing typed output. |

The declarations specify future behavior. Current stubs return
`NOT_IMPLEMENTED` and defensively clear documented output lengths/structures.

## Stateless context and single owner

`HIL_Application_Context_T` contains only:

- a copied `HIL_Application_Config_T`; and
- an `initialized` flag.

The context stores no caller pointers, messages, encoded output, decode storage,
active Test ID, expected tick, outstanding variable declaration, accepted tick,
result progress, retention ownership, execution state, or transaction status.
It remains a stateless codec context and must not gain mutable transaction data.

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
- maximum peripheral configurations in one typed configuration;
- maximum variable-data declarations in one typed tick; and
- maximum permitted `expected_tick_count` field value.

These are local structural limits, not final wire maxima. They reserve no test
storage and do not mean the codec can retain or track the configured number of
ticks. Endpoint integration separately decides whether retention and hardware
capacity are available.

`max_variable_data_size` bounds each variable UART/SPI/I2C/CAN Application
message, and `max_encoded_message_size` bounds each complete encoded Application
message. They do not describe one monolithic test or tick package. Integration
must configure Transport's maximum Application-message size to be at least the
Application Layer's `max_encoded_message_size`; the codec does not inspect or
change Transport configuration.

Zero disables the corresponding capacity. This design deliberately chooses no
production limits. A future `Default_Config` initializes every field without
inventing policy. A future `Init` validates limit relationships and copies the
configuration without allocation or pointer retention.

Fixed GPIO, analogue, and PWM arrays are protocol-sized rather than
caller-configured. Their named channel counts and deterministic index mappings
are described in [Application messages](application_messages.md).

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

## Encoding and decoding ownership

The caller constructs `HIL_Application_Message_T` and selects one typed body
through its explicit `type` tag. Fixed signal arrays are inline in the typed
message. Variable arrays and byte spans are borrowed only during the synchronous
sizing, validation, or encoding call.

Future successful encoding copies all message content into caller output and
retains no pointer. The caller submits those complete bytes to Transport. On
`BUFFER_TOO_SMALL`, `output_size` reports required bytes and partial output is
not a message. Sizing and encoding never mutate the context.

Transport supplies one complete reassembled Application message for decoding.
Future decoding copies every variable array/span into caller-owned
`decode_storage`. Pointers in the decoded message point only into that region,
not the encoded input. The Transport receive item can therefore be released
after successful decode. The codec retains neither pointer.

Decoding a configuration, control, reset, Response, Error, or result never
accepts data, performs an operation, creates or invalidates a transaction,
changes firmware state, or generates a reply. Those are endpoint-integration
actions after a successful structural decode.

## Validation and rejection boundaries

There are three distinct failure boundaries.

### 1. Transport-invalid data

Transport framing, integrity, ordering, and session rules run before Application
decoding. Data rejected by Transport is not an Application message and produces
no Application Response merely because Transport rejected it.

### 2. Structurally invalid Application data

The future codec checks:

- message type/subtype and exact test-ID presence;
- exact complete encoded length;
- required fields and enum values;
- every element of fixed GPIO, analogue, and PWM arrays;
- variable pointer/count and NULL/zero combinations;
- configured codec bounds;
- declared-length consistency;
- channel-family consistency;
- checked size arithmetic; and
- safe decoding without trailing or missing bytes.

Failures are local `HIL_Application_Status_T` values. They are not serialized as
Application Responses or Errors. The codec has no active Test Configuration and
cannot compare a later Test ID/tick against an active transaction.

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

## Initial message transaction contract

The following table replaces any shared protocol-phase or firmware-state model.

| Exchange | Sender -> receiver | Required correlation | Protocol prerequisite | Successful Response and transaction effect | Rejection/failure and recovery | Integration-owned decisions |
| --- | --- | --- | --- | --- | --- | --- |
| Test Configuration | Python -> firmware | Fresh Test ID; no tick | No active upload being continued; exactly one configuration starts a new upload attempt | Configuration `ACCEPTED` creates the active upload transaction for that Test ID | `REJECTED`/`FAILED` creates no transaction; host starts a new upload from Test Configuration with a fresh Test ID | Hardware support, safety, timing, and retention capacity |
| Test Instruction plus declared Variable Instruction Data | Python -> firmware | Active Test ID, zero-based tick, and peripheral/channel for each variable message | Configuration accepted; ticks sent in increasing order; each nonzero declaration has one separate matching variable message | Tick `ACCEPTED` means the complete fixed-plus-variable tick was validated and accepted for retention, so the host may continue | Any rejected tick or associated variable data invalidates the upload; restart from Test Configuration; in-flight messages may be rejected | Active-ID/tick tracking, completeness, retention mechanism, and cleanup |
| Automatic whole-test validation | Firmware -> Python Response | Active Test ID; Complete Test scope; no tick | Every tick `0..N-1` and all declared variable data accepted | Complete Test `ACCEPTED` means the complete test was retained, validated, and is available for a later START request; upload is complete | `REJECTED`/`FAILED` invalidates the transaction; restart from Test Configuration | Whole-test consistency and release of invalid retained data |
| START | Python -> firmware | Accepted test's Test ID and START command | Complete Test `ACCEPTED` was received for that Test ID | Execution Control START `COMPLETED` means firmware performed the start request | `REJECTED` means execution did not start; `FAILED` requires recovery and the host must not assume execution status | Execution-manager permission, hardware readiness, actual firmware transitions, and whether retry is safe |
| ABORT | Python -> firmware | Identified active Test ID and ABORT command | A matching upload, accepted test, execution, or result transaction exists | Execution Control ABORT `COMPLETED` means safe stop/abandonment was performed and the previous transaction cannot continue normally; a new upload starts from Test Configuration | `REJECTED` means abort was not performed; `FAILED` requires firmware-specific recovery and no assumed cleanup | Execution-manager transitions, safe stop, retained-data cleanup, and firmware recovery |
| Test Result plus declared Variable Result Data | Firmware -> Python | Accepted Test ID, zero-based tick, and peripheral/channel for variable messages | Firmware execution completed and corresponding results are available | No initial per-result Application Response; successful decode contributes to host result completion | Structural decode failure is local; protocol mismatch or session loss requires recovery because result resume is undefined | Result availability, ordering/handoff, retention, and release policy |
| RESET_APPLICATION | Python -> firmware | No Test ID; Global Control scope | May be requested independently of a known test | Global Control `COMPLETED` means active Application transaction data and recoverable Application protocol conditions were cleared | `REJECTED`/`FAILED` means the host cannot assume cleanup and must follow firmware recovery policy | Mapping to internal firmware state, cleanup, and whether reset can be completed |
| Application Error | Usually firmware -> Python | Test ID/tick present only when known and relevant | A broader fault exists rather than rejection of one request | No implicit transaction success; integration interprets category/recoverability | Host follows indicated recovery, commonly ABORT or RESET_APPLICATION | Error generation, firmware state, hardware response, and diagnostics |

## Required upload and execution sequence

The initial contract is:

1. The Python host generates a fresh random 128-bit Test ID.
2. It sends exactly one Test Configuration carrying that ID.
3. Firmware structurally decodes it, performs firmware-specific semantic checks,
   and returns a configuration-scoped Response.
4. Configuration `ACCEPTED` creates the active upload transaction.
5. The host sends fixed Test Instructions in increasing zero-based tick order.
6. Each nonzero variable-data declaration is followed by its separately encoded
   Variable Instruction Data message with matching Test ID, tick, peripheral,
   and channel.
7. Firmware returns a Tick Response only after the complete fixed-plus-variable
   tick was received, validated, and accepted for retention.
8. After ticks `0..N-1` and all declared variable data are accepted, firmware
   automatically performs whole-test validation.
9. Complete Test `ACCEPTED` means the test was retained, validated, and is
   available for a subsequent START request.
10. The host sends START separately.
11. Firmware asks its execution manager whether execution can begin, performs
    the request if allowed, and reports the actual outcome.
12. Firmware may make results available after execution completes.

There is no Begin Upload, ARM, or FINALIZE_TEST command. Test Configuration
acceptance starts upload; Complete Test acceptance completes upload; START is a
separate request whose success is never predicted by the codec or host.

## Transaction invalidation and recovery

The initial recovery rules are normative:

- rejected Test Configuration creates no upload transaction;
- a rejected tick or associated variable-data message invalidates the active
  upload transaction;
- failed whole-test validation invalidates the active transaction;
- after invalidation, the host must restart from Test Configuration with a fresh
  random Test ID;
- messages already in flight for the invalidated Test ID may be rejected with
  `INCONSISTENT_TEST_ID`, `INVALID_TICK`, or `OPERATION_NOT_ALLOWED` as
  appropriate; and
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

For a test with `N` ticks, the Python host considers result transfer complete
only after it has successfully decoded:

- one fixed Test Result for every tick `0..N-1`; and
- every Variable Result Data message declared by those fixed results.

The host correlates all result messages by Test ID, tick, peripheral, and
channel. No shared phase transition or simultaneous endpoint state change
is implied by completion.

Firmware considers its result set handed off according to the surrounding
Transport integration after every complete encoded fixed and variable result
message has been accepted for delivery. Whether firmware waits for a Transport
delivery acknowledgement before releasing retained results is Transport and
firmware policy, not an Application codec rule. The initial Application design
has no per-result Response or completion message.

Result ranges, resume after reconnect, Application-level retransmission, and
partial-result recovery remain deferred.

## Transport session loss

Transport does not directly create, complete, or invalidate Application data.
It reports session loss/reset to endpoint integration. Under the initial
Application integration policy, session loss during upload invalidates the
active upload; in-flight messages may be rejected, and the Python host starts a
new upload from Test Configuration with a fresh Test ID after reconnect.

If session loss interrupts result transfer, resumption is not defined by this
version. The client reports recovery is required rather than assuming which
results firmware retained. `RESET_APPLICATION` remains an Application request
and does not reset or reconnect Transport.

## Future firmware integration

The intended firmware boundary is:

1. Transport returns one complete reassembled Application message.
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

## Future Python integration

The intended Python boundary is:

1. A future client method creates typed configuration, instruction, or control
   data.
2. Python uses the shared C codec through bindings to encode one complete
   Application message.
3. Python submits that message through its Transport integration.
4. Received complete Application messages are decoded through the same codec.
5. The client correlates Responses using Test ID, scope, tick, and command.
6. It tracks client-side progress such as accepted configuration, sent and
   accepted ticks, Complete Test acceptance, START outcome, and received results.
7. It exposes success, rejection, protocol mismatch, and recovery requirements
   to the API user.

Python may prevent an obviously invalid local action such as requesting START
before Complete Test acceptance. It must not predict firmware hardware
readiness or execution-manager state; firmware Responses are authoritative.

The Python client, bindings, serial/USB integration, asynchronous behavior,
exceptions, and transaction controller are future work and are not implemented
in this PR.

## Required future conformance work

Compile-level API-design tests in this PR construct messages for the documented
success, rejection, control, reset, result, and session-loss scenarios. They do
not execute the transaction because all codec functions are still stubs.

Before the protocol is considered implemented, future work must add:

- an approved binary envelope/body layout, fixed widths, byte order, and
  compatibility rules;
- codec implementation and structural-validation unit tests;
- golden wire vectors shared by C and Python;
- executable firmware/Python conformance tests for every transaction scenario;
- malformed-message and boundary testing;
- detailed diagnostic/error schemas and capability discovery;
- production codec bounds; and
- integration implementations for firmware handling, retention, execution,
  Python workflow, and recovery.

Advanced upload/result resumption, range requests, and Application-level
retransmission remain intentionally deferred.
