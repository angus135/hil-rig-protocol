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

The MVP also implements the private one-item reliable output lifecycle: an
already encoded frame can be published, repeatedly peeked, committed, retained
byte-for-byte across timed retransmissions, completed by an exact ACK, exhausted
without choosing recovery policy, or abandoned by reset. Public
`Peek_Output()`, `Commit_Output()`, `Get_Status()`, and `Reset()` use that
primitive. Dedicated tests drive it directly with opaque sentinel bytes because
the later handshake and Application submission paths do not yet publish frames.

The broader Transport runtime—session handshake, received-ACK dispatch,
receive-side duplicate handling, ACK generation, Application submission and
delivery, delivery events, and session recovery—remains as intentional
`HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED` stubs. The public send/receive workflow
is therefore not operational end to end yet.

The default MVP Transport profile is designed for one complete Application
message per frame and one outstanding reliable transmission. Extended
fragmentation, reassembly, flow control, keepalives, and queueing remain future
design only. Public headers, documentation, and compile-level tests currently
define the integration contracts.

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
