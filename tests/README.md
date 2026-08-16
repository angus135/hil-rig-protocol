# hil-rig-protocol tests

The test tree separates component-level verification from black-box Transport integration and consumer build checks.

## Transport unit tests

`tests/c/unit/` contains focused tests for Transport implementation modules and public lifecycle contracts. These tests may include private headers through the unit-test target's `${PROJECT_SOURCE_DIR}/src` include path when a component must be exercised directly.

## Transport integration tests

`tests/c/integration/` validates complete Transport behaviour through the public API. Integration support deliberately does **not** receive the private `src/` include path. Existing integration scenarios are grouped by caller-visible purpose rather than by private implementation module:

- `test_transport_facade.cpp` keeps public facade, lifecycle, and API-shape checks.
- `pipeline/` contains normal two-endpoint delivery scenarios.
- `recovery/` contains the existing end-to-end recovery scenarios.
- `error_injection/` and `malformed_frames/` remain available for later scenario growth.

No integration scenario includes private Transport headers or reaches into a `TransportTestEndpoint` context directly. The reusable harness under `tests/c/integration/support/` contains three layers. Its own structural self-test also lives in `support/`, keeping harness verification separate from production Transport scenario categories.

### `TransportTestEndpoint`

`TransportTestEndpoint` owns one production `HIL_Transport_Context_T`, constructs its public configuration, asks `HIL_TRANSPORT_Required_Storage_Size()` for the required caller-owned workspace, and wraps public API calls without assuming they must return `OK`.

Normal `PeekOutput()` and `ReadApplication()` calls allocate the configured maximum caller buffer. Overloads accepting an explicit capacity, plus `QueryOutputSize()` and `QueryApplicationSize()`, keep size-query and undersized-buffer scenarios inside the same black-box wrapper rather than forcing tests to bypass it. `DrainEvents()` returns both drained events and the first non-`OK` `Read_Event()` status; normal complete draining therefore ends explicitly with `NOT_READY` instead of silently hiding another error.

### `TransportTestLink`

`TransportTestLink` models the external full-duplex byte-stream connection. An output item is first peeked, copied into link-owned storage, and committed only when the simulated external writer accepts it. After commit, the link can retain, drop, duplicate, corrupt, join, or split opaque bytes before calling the peer's `Receive_Bytes()`.

Normal output acceptance derives direction from the sender's public role, and normal receive delivery derives direction from the receiver's public role. Tests specify a direction explicitly only when manipulating link-owned or externally injected traffic. This prevents a host endpoint from accidentally being wired into the rig-to-host stream by a mismatched helper argument.

Each accepted output receives a stable `TransportTestOutputHandle` containing its direction and a unique link ordinal. Exact drop, duplicate, hold, release, corruption, take, and delivery operations accept that handle, so complex fault scenarios do not depend on queue order or repeat a separate direction argument that could disagree with ownership. The simpler `*NextAccepted()` helpers remain available for scenarios where FIFO behaviour is itself sufficient. Convenience operations that claim to accept and immediately transfer one particular output always use its handle, so an older delayed item in the same direction cannot be substituted for the newly accepted output.

The ownership sequence intentionally matches a real caller:

```text
Transport Peek_Output()
        |
        v
external writer accepts complete item
        |
        v
Transport Commit_Output()
        |
        v
simulated link owns the transmitted bytes
        |
        v
arbitrary byte chunks reach peer Receive_Bytes()
```

`TransportTestLink::DeliverReady()` removes only the exact `bytes_consumed` prefix returned by `Receive_Bytes()`. Any unconsumed suffix stays caller-owned in the simulated link for a later call. This is important when Transport internally retains a complete blocked frame while returning `CAPACITY_EXHAUSTED`. If there are no ready bytes, no production receive call occurs and the delivery result contains no Transport status; this is intentionally distinct from Transport itself returning `NOT_READY`.

Integration support never decodes frames to make routing decisions. Frame type, sequence, session identity, COBS, CRC, handshake phase, and parser state remain production Transport concerns.

### `TransportPairHarness`

`TransportPairHarness` composes one host endpoint, one rig endpoint, the persistent simulated link, and independent deterministic caller clocks. Pair initialization validates the structural host/rig role assignment before initializing production contexts. The ordinary `InitializeConnected(..., now)` overload sets both clocks together, while the four-argument overload permits unrelated host and rig epochs.

`ProcessHost()` and host-side output commits use only the host clock; `ProcessRig()` and rig-side commits use only the rig clock. `ProcessBoth()` always services both endpoints using those independent values and returns both real Transport statuses independently. A non-`OK` host result therefore does not prevent the rig caller from being serviced. Higher-level helpers keep support-layer failures separate from production `HIL_Transport_Status_T` values, so a harness step limit or item-bookkeeping failure cannot be mistaken for a Transport `INTERNAL_ERROR`.

`SetHostTime()`, `SetRigTime()`, `AdvanceHostTime()`, and `AdvanceRigTime()` support tests where only one local clock approaches `uint32_t` wrap. `SetBothTimes()` and `AdvanceBothTimes()` are concise conveniences for ordinary scenarios. There is deliberately no ambiguous single `Now()` accessor: code that uses an endpoint-local time explicitly selects `HostNow()` or `RigNow()`. `EstablishCleanSession()` never rewinds either clock and advances both by the same service-step delta.

The harness never drains Application messages or events automatically because unread public resources are part of the MVP backpressure contract. Recovery tests also retain explicit operation ordering where that ordering is part of the existing scenario.

## C consumer compile test

`tests/c/integration/test_transport_public_c.c` remains a separate C11 executable. It validates that a plain C consumer can include, link, and call the public API without depending on the C++ integration harness.

## Application tests and golden vectors

`tests/c/application/` contains Application-layer tests. `tests/golden_vectors/` is the language-neutral location for stable wire-format fixtures that may later be shared by C, firmware, and Python validation.
