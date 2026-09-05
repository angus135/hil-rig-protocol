# HIL-RIG Protocol

Shared C11 protocol boundary for HIL-RIG firmware and host-side Python
software. The library is communication-medium-agnostic: USB, UART, serial,
DMA, RTOS integration, clocks, memory, and physical I/O remain caller-owned.

## Architecture

The current design has two public layers:

- **Application Layer** — defines typed messages and the exchange contract for
  system information, test configuration, instructions, execution control,
  results, responses, and errors. Its stateless C codec converts between typed
  data and one complete, architecture-independent Application message. Several
  message-family operations remain deliberately `NOT_IMPLEMENTED`.
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

Reception follows the reverse path. Transport returns one complete opaque
Application message, which the stateless Application codec can decode and
structurally validate where that message family is currently supported. The MVP
places each message in one Transport frame; fragmentation and reassembly are
outside the MVP.

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

The MVP Transport implementation includes framing, COBS, CRC, reliable and
control output, output arbitration, bounded events, link/session recovery, the
handshake, transactional receive, and bidirectional complete-message delivery.
The C and Python consumer surfaces execute the same C Transport implementation;
the Python wheel statically contains that core behind a private CFFI extension.
Comprehensive public two-endpoint C and Python integration suites cover normal,
backpressure, reliability, corruption, reset, recovery, and ownership behavior.

The Application layer now has a fixed 23-byte architecture-independent common
envelope, bounded encode/decode paths, structural validation, and complete fixed
codec support for Test Configuration, Test Instruction, and Test Result. Fixed
Instruction/Result payloads are 50/39 bytes respectively, with Boolean Digital,
PWM, configured tick-ceiling, and result-condition validation. Variable-data
families and Response/Error work remain deliberately `NOT_IMPLEMENTED`; analogue
hardware ranges and stateful test workflow remain integration responsibilities.

Public C Application-to-Transport integration now exercises representative and
maximum Test Configuration messages, fixed Test Instructions from host to rig and Test Results
from rig to host, exact opaque-byte preservation, capacity boundaries, reliable
retry, and separation of Transport corruption from Application malformed or
validation failures. These tests use public APIs only; Transport delivery
confirmation is not treated as Application semantic acceptance.

The MVP Transport wire path is implemented: versioned little-endian
frames use CRC-32/ISO-HDLC for accidental-corruption detection,
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

The MVP also embeds private storage for a bounded FIFO of complete high-level
Transport events. Private modules can publish complete event values into this
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
three reliable negotiation frames (`INITIATE`, `RESPONSE`, and `CONFIRM`) plus a
final ACK acknowledging `CONFIRM`. The clean exchange is four wire transmissions.
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
and consume-on-success reads of complete opaque messages. Fragmentation and
reassembly are outside the MVP; receive queues and reorder windows are unsupported.

The default MVP Transport profile is designed for one complete Application
message per frame and one outstanding reliable transmission. The private event
FIFO does not imply message or output queueing. Possible extended-profile
fragmentation, reassembly, flow control, keepalives, and message queueing remain
future design only. Public headers, documentation, unit tests, and public
two-endpoint integration tests define the integration contracts.

`DELIVERY_CONFIRMED` means the peer Transport accepted a message, not that its
Application layer decoded or processed it. Unread received messages can be
discarded by reset, abandonment, or link loss. Application Responses and Test
IDs provide higher-level transaction semantics; after delivery failure or
session loss, firmware or host software decides whether and how to restart an
upload, execution request, or result transaction.

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
pointers after a call. The Application envelope uses the repository-wide
compiled protocol major/minor version; callers cannot select or negotiate a
separate Application version.

- [Application wire-format reference](docs/application_layer/application_wire_format.md)
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

Python callers should start with the
[Python Transport caller guide](docs/python/transport.md) and the tested
[caller-owned I/O example](examples/python/transport_servicing.py).

## Licence

The project is distributed under the root [MIT licence](LICENSE). The bundled
COBS implementation retains its own MIT licence and is identified in
[third-party notices](THIRD_PARTY_NOTICES.md); both licence texts are retained
in source and binary Python distributions.
