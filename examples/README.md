# Examples

[`python/transport_servicing.py`](python/transport_servicing.py) demonstrates
the public Python Transport caller boundary: explicit monotonic time, exact
receive-prefix consumption, event and opaque Application-data draining, partial
external writes, and one final output commit. It deliberately does not open
hardware or provide a production connection driver.

The library defaults keep retransmission timing disabled for deterministic
callers. The example therefore constructs its HOST with an explicit non-zero
retransmission timeout and retry count as a hardware-oriented illustration. The
specific values are not universal: a real timeout must account for physical
transmission time, OS/driver buffering, scheduler delay, and the expected peer
response time.

A physical disconnect is a caller-owned boundary. A real integration should:

1. stop or quiesce the physical driver;
2. flush/discard old driver receive and transmit queues;
3. invalidate retained Python receive bytes, cached output and partial-write
   offsets;
4. notify Transport that the link is disconnected;
5. re-establish the physical connection;
6. notify Transport that the link is connected; and
7. resume servicing the replacement session.

The same Python servicing state is invalidated before an explicit local
Transport reset. Do not clear it merely because a `SESSION_RESET` event was
read: peer RESET and replacement handshake traffic may validly be processed in
one native receive call. Asynchronous integrations must additionally reject
late read/write completions from an older physical connection generation.

Normal protocol recovery uses the disconnect/reconnect boundary above. A
Transport `FAULT` is different: link cycling does not repair it. The defined
local `reset()` operation is required, and a FAULT during otherwise valid test
traffic should be treated as an integration/test failure rather than silently
continuing.

See the [Python Transport caller guide](../docs/python/transport.md) for the
complete contract.
