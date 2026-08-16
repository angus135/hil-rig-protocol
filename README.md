# HIL-RIG Protocol

Shared C11 protocol boundary for HIL-RIG firmware and host-side Python
software. The library is communication-medium-agnostic: USB, UART, serial,
DMA, RTOS integration, clocks, memory, and physical I/O remain caller-owned.

## Architecture

The current design has two public layers:

- **Application Layer** — defines typed messages and the exchange contract for
  system information, test configuration, instructions, execution control,
  results, responses, and errors. Its future stateless codec converts between
  typed data and one complete Application message.
- **Transport Layer** — carries each complete, opaque Application message over
  a caller-owned byte stream. Its facade is designed to own framing, integrity,
  session establishment, ordered reliable delivery, and recovery without
  depending on Application semantics or physical I/O.

The intended send path is:

```text
typed firmware/Python data
    -> Application encode
    -> one complete Application message
    -> Transport
    -> framed byte stream
    -> caller-owned USB/UART/serial I/O
```

Reception follows the reverse path. Transport must return one complete
reassembled Application message before the Application codec decodes it.
Transport fragmentation, when supported by a future profile, remains invisible
to Application.

The layer boundary is deliberate:

- Application owns message structure, correlation, directions, response
  meaning, and the transaction contract. Firmware and Python integrations
  enforce cross-message order and semantics; the stateless codec does not
  retain transaction state. Firmware remains authoritative for its internal
  state and hardware decisions.
- Transport owns delivery mechanics but never interprets Application message
  types, Test IDs, commands, or Responses. A Transport acknowledgement confirms
  delivery, not semantic acceptance; Application Responses communicate the
  latter.
- Application Test IDs and Transport session identities are independent.
- Integrations must configure Transport's maximum Application-message size to
  be at least Application's maximum encoded-message size.

## Current status

Application encoding, decoding, and structural validation are intentional
`HIL_APPLICATION_STATUS_NOT_IMPLEMENTED` stubs, and the common Application wire
envelope is not defined. The MVP Transport wire path is implemented: versioned
little-endian frames use CRC-32/ISO-HDLC for accidental-corruption detection,
standard COBS encoding, and a trailing `0x00` stream delimiter. Workspace
sizing, initialization, and the bounded stream parser are also implemented.

The MVP also implements independent one-item reliable and control-output
lifecycles plus public arbitration between them. Reliable output retains one
already encoded frame byte-for-byte across commit, ACK wait, timed retries, and
retry exhaustion. Control output retains up to 20 already encoded bytes and is
preferred when no output is already pinned. A complete successful peek pins the
opaque selected item until commit or reset; size queries and undersized buffers
do not pin or cause fallback to the other lifecycle.

Public `Peek_Output()`, `Commit_Output()`, `Get_Status()`, and `Reset()` use the
arbiter. Control commit releases its slot immediately and ignores the supplied
time, while reliable commit starts ACK timing and retains the encoded bytes.
`output_pending` aggregates both lifecycles, while
`reliable_delivery_pending` remains limited to reliable ownership. Dedicated
lifecycle tests use opaque sentinel bytes; handshake tests additionally decode
the real INITIATE, RESPONSE, CONFIRM, ACK, and RESET output they coordinate.

The MVP also embeds private storage for four complete high-level Transport
events. Private modules can publish complete event values into this bounded
FIFO without retaining caller pointers. `Read_Event()` returns the oldest event
and consumes exactly that event only after a successful complete copy;
`NOT_READY` and other errors preserve the destination. A full FIFO returns
`CAPACITY_EXHAUSTED` without overwriting older events, and public status exposes
only whether any event is pending, not the private depth or count. Explicit
`Reset()` releases all queued event ownership without clearing inaccessible slot
bytes. Link changes publish `LINK_STATE_CHANGED`; completed handshakes publish
exactly one `SESSION_ESTABLISHED` per endpoint; automatic session abandonment
preserves older events and attempts to append `SESSION_RESET`.

The semantic MVP session handshake is implemented through a private coordinator:
host INITIATE, rig RESPONSE, host CONFIRM, and rig ACK establish both endpoints.
It reuses the codec and output lifecycles, supports exact duplicate recovery and
timed retries, and abandons exhausted attempts through the existing session
recovery path. `HIL_TRANSPORT_Process()` validates and records all three MVP
operating modes, progresses Transport-owned receive work before retry expiry,
publishes pending handshake work, advances reliable timing, and starts later
recovery attempts. Public `Receive_Bytes()` now feeds arbitrary
stream chunks through the bounded parser and non-copying decoder view, then
dispatches complete frames into that coordinator transactionally. A completed
parser body remains retained across temporary event, reliable-output, or
control-output blockage, so callers retry only the exact unconsumed input suffix.
Oversized bodies resynchronize at their delimiter and retain a pending error
notification if the event FIFO is temporarily full.

Outbound Application submission and reliable delivery are implemented. A caller
may submit one complete message after session establishment; Transport copies it,
encodes one APPLICATION_MESSAGE frame, retains the exact bytes through retry, and
completes delivery on the matching ACK with one `DELIVERY_CONFIRMED` event. Retry
exhaustion retains one `DELIVERY_FAILED` event before abandoning the uncertain
session and starting replacement-session recovery. Established-session ACKs with
no active Application delivery are treated as stale protocol input for both host
and rig roles rather than being reinterpreted as handshake traffic.

Inbound Application delivery is also implemented for the MVP. An expected
APPLICATION_MESSAGE is copied into the sole unread-message region only when the
required ACK can also be retained; the receive sequence is committed after that
ACK publication succeeds. A repeat of the last accepted Application sequence is re-ACKed
without exposing the payload twice. A new expected frame is retained
transactionally when the unread-message slot or control-output slot is occupied,
and `HIL_TRANSPORT_Read_Application_Data()` supports size query, undersized retry,
and consume-on-success reads of complete opaque messages. Fragmentation,
reassembly, receive queues, and reorder windows remain unimplemented.

The default MVP Transport profile is designed for one complete Application
message per frame and one outstanding reliable transmission. The private event
FIFO does not imply message or output queueing. Extended fragmentation,
reassembly, flow control, keepalives, and message queueing remain future design
only. Public headers, documentation, unit tests, and public two-endpoint integration tests define
the integration contracts.

## Build and test

Configure the default MVP Transport profile and build all tests with:

```sh
cmake -S . -B build \
  -DHIL_RIG_PROTOCOL_TRANSPORT_PROFILE=MVP \
  -DHIL_RIG_PROTOCOL_BUILD_TESTS=ON \
  -DHIL_RIG_PROTOCOL_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Format C, C++, and headers with:

```sh
./run_checks.sh format
```

The library target is `hil_rig_protocol::hil_rig_protocol`; installed-style
includes use the `hil_rig_protocol/...` namespace. The core library performs no
heap allocation.

## Application Layer

Normal Application integrations include only:

```c
#include "hil_rig_protocol/application/application.h"
```

The public context holds structural codec bounds only. Callers own encoded
buffers and aligned decode storage, and the codec retains no message or buffer
pointers after a call. The MVP API has one compiled-in Application protocol
version, 1.0; callers cannot select or negotiate another version.

- [Application codec and transaction contract](docs/application_layer/application_layer.md)
- [Application message definitions](docs/application_layer/application_messages.md)
- [Application caller workflow](docs/application_layer/application_api_usage.mmd)

## Transport Layer

Normal Transport integrations include only:

```c
#include "hil_rig_protocol/transport/transport.h"
```

Each Transport context has one owning execution context and one aligned,
caller-owned workspace. The caller reports link state and monotonic time, feeds
arbitrary received byte chunks, and performs external writes using the
facade's peek/commit ownership model.

The MVP transmits `COBS(decoded frame) || 0x00`. Each decoded frame has a
14-byte fixed header, one optional complete opaque Application message, and a
four-byte CRC. See the normative Transport document for byte offsets, frame
types, field rules, integrity coverage, and size bounds.

- [Normative public facade contract](docs/transport_layer/transport_layer.md)
- [Transport caller workflow](docs/transport_layer/transport_api_usage.mmd)
- [Non-normative extended design](docs/transport_layer/extended_transport_design.md)

Detailed setup and workflow examples live in the layer documentation and
compile-level tests rather than this repository overview.
