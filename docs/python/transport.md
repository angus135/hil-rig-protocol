# Python Transport caller guide

## Package identity and supported wheels

The distribution is named `hil-rig-protocol`; callers import
`hil_rig_protocol`. `hil_rig_protocol._native` is the private, CPython-ABI-specific
CFFI extension and is not a supported public API.

The initial verified binary-wheel matrix is:

| Platform | Architecture | Python |
| --- | --- | --- |
| Linux `manylinux_2_28` | x86-64 | CPython 3.12, 3.13, 3.14 |
| Windows | AMD64/x86-64 | CPython 3.12, 3.13, 3.14 |

There are no prebuilt wheels in this release for macOS, Linux ARM64/aarch64,
musllinux, Windows ARM64, 32-bit platforms, PyPy, GraalPy, or free-threaded or
stable-ABI (`abi3`) interpreters. Source builds may work on other compatible
systems, but only the matrix above is built and tested as binary artifacts.

A source build needs CPython 3.12 or newer and its development headers, a C11
compiler, CMake 3.17 or newer, CFFI 1.17.1 or newer, and the PEP 517 build tools
declared by `pyproject.toml`. Normal firmware builds do not need Python.

## Installation

Install from a repository or submodule checkout:

```sh
python -m pip install /path/to/hil-rig-protocol
```

For binding development, use an editable installation:

```sh
python -m pip install -e "/path/to/hil-rig-protocol[test,dev]"
```

Install a built wheel directly with `python -m pip install path/to/wheel.whl`.
Confirm both layers and distribution metadata with:

```sh
python -c "import importlib.metadata; import hil_rig_protocol; import hil_rig_protocol._native; print(importlib.metadata.version('hil-rig-protocol'))"
```

Each wheel statically contains the C core inside `_native`; no separately
installed `hil_rig_protocol` shared library is required.

## Construction, ownership, and lifetime

`Role.HOST` initiates a session and `Role.RIG` responds. Construct a
`TransportConfig` and pass it with the fixed lifetime role:

```python
from hil_rig_protocol import Role, Transport, TransportConfig

with Transport(Role.HOST, TransportConfig()) as transport:
    effective_config = transport.config
```

A HOST configuration with `session_seed=None` receives a cryptographically
generated non-reserved seed. Tests may supply an explicit deterministic HOST
seed. A RIG must use `None` (resolved to zero) or explicit zero; nonzero RIG
seeds are rejected.

Prefer a context manager or deterministic `close()`. Garbage collection is only
a fallback. `close()` is idempotent; every later native operation raises
`TransportClosedError`. A Transport is single-threaded and single-owner. Keep
all servicing in one coordinating synchronous owner or one asyncio task; moving
individual calls among workers is unsupported.

## Caller-owned time

The binding never reads a clock. `now_ms` remains explicit for link
notifications, `process()`, and `commit_output()`. Use monotonic time rather
than a wall clock, convert milliseconds to the wrapped `uint32_t` domain, and
use one coherent domain for the Transport lifetime:

```python
now_ms = int(monotonic_seconds * 1000) & 0xFFFF_FFFF
```

The public configuration defaults keep retransmission timing disabled so tests
and deterministic callers do not acquire hidden timing behaviour. Hardware
integrations should select an explicit non-zero retransmission timeout and retry
count. Size the timeout for physical transmission time, OS/driver buffering,
scheduler latency, and the expected peer response time rather than copying an
example value blindly.

## Receive servicing and backpressure

External I/O owns raw received bytes. Retain them in a caller buffer and offer
that buffer to `receive_bytes()`. The result may accept only a prefix: remove
exactly `ReceiveResult.bytes_consumed`, retain the exact unconsumed suffix, and
never drop or resend the already consumed prefix.

`CAPACITY_EXHAUSTED` is normal bounded backpressure. Drain pending events and
opaque Application data and ensure pending control or reliable output is fully
written and committed. Then call `receive_bytes(b"")` to resume any completed
work retained internally before offering more external bytes. A zero-byte call
is meaningful and is forwarded to native Transport.

## Output servicing

`peek_output()` returns a Python-owned immutable copy while the corresponding
native item remains pinned. The caller owns that `bytes` value and a partial
write offset. Pass only `pending[offset:]` to the external writer, preserve the
same value across partial writes, and do not call `commit_output()` early.

After the writer accepts every byte, call `commit_output(now_ms)` exactly once.
Repeated peeks before commit represent the same pinned item. The binding neither
caches output nor performs I/O. The tested
[caller-owned servicing example](../../examples/python/transport_servicing.py)
shows this state explicitly.

## Events and opaque Application bytes

Call `read_event()` until it returns `None`; unread events occupy bounded
capacity. Call `read_application_data()` explicitly for each complete opaque
Application message. Transport does not decode Application messages.

`EventType.DELIVERY_CONFIRMED` means the peer Transport accepted the bytes. It
does not mean the peer Application layer decoded, validated, or semantically
accepted them. Semantic acceptance requires a future Application-level
response.

## Link changes, reset, and recovery

Report physical connection and disconnection with `notify_link_state()` and an
explicit time. A hard physical reconnect is also a caller-owned byte-lifetime
boundary. The correct ordering is:

1. stop or quiesce the physical driver;
2. flush/discard old driver receive and transmit queues;
3. invalidate caller-retained receive bytes, cached peeked output, partial-write
   offsets, and completion bookkeeping;
4. notify Transport of `DISCONNECTED`;
5. re-establish the physical connection;
6. notify Transport of `CONNECTED`; and
7. resume fresh-session servicing.

The same external servicing state must be invalidated before an explicit local
`reset()`. Do **not** clear caller state merely because a `SESSION_RESET` event
is observed: a peer RESET and replacement-handshake bytes may validly be
processed in the same receive call, and unconditional event-driven clearing can
discard new-session output. A synchronous owner can enforce this ordering
directly. An asynchronous integration should also tag operations with a
connection generation (or equivalent) and reject late read/write completions
from an earlier generation.

Once an external writer has accepted bytes, they belong to that physical write
operation. If the later native `commit_output()` reports `NOT_READY`, do not
resend those already accepted bytes. Observe the returned status and current
Transport state instead. `INVALID_ARGUMENT`, `INTERNAL_ERROR`, or other
unexpected servicing results are integration faults and should not be silently
ignored.

Normal protocol recovery may replace a session through the physical
disconnect/reconnect path. A terminal `FAULT` is different: link cycling does
not repair it. Use the defined local `reset()` operation, and treat FAULT during
otherwise valid test traffic as an integration/test failure. The higher-level
consumer decides whether and how to restart uploads, commands, or result
transactions; this package does not impose Application-level recovery policy.

## Deliberate layer boundary

This package provides no serial or USB discovery, serial read/write methods,
background threads, asyncio connection task, automatic timing, automatic
reconnection policy, Application message types or decoding, or higher-level
test-workflow recovery. Those responsibilities belong in `hil-rig-python-api`
or a future Application binding. The public `Transport` remains a synchronous,
caller-driven protocol boundary.
