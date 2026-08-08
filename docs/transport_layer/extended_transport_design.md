# Extended Transport design notes (non-normative)

## Status

This document records possible future protocol design. It is not the public API
contract, does not describe implemented behavior, and does not require the MVP
to provide these features. A decision becomes normative only after approval and
promotion into the public contract and relevant private implementation design.

The MVP wire representation is now defined in `transport_layer.md`. Any future
extended wire representation still requires explicit decisions about additional
fields, version compatibility, control delivery classes, and crossed-session
behavior; this document does not override the implemented MVP format.

## MVP-private three-message handshake choice

The compiled MVP retains an explicit private phase model independent of public
operating mode and every Application lifecycle. The wire frame types are
implemented, but handshake progression remains a stub and is not public API:

1. host sends reliable `INITIATE` carrying a fresh caller-seeded Transport
   session identity and host initial sequence information;
2. HIL-RIG adopts the valid identity and sends reliable `RESPONSE` with its
   initial sequence information;
3. host sends reliable `CONFIRM`;
4. the HIL-RIG enters ESTABLISHED after receiving a valid `CONFIRM` and makes an
   ACK for that `CONFIRM` available; and
5. the host enters ESTABLISHED only after receiving the matching ACK.

The phase must be stored explicitly rather than inferred from endpoint role or
the high-level CONNECTING state. Candidate duplicate handling is idempotent:

- duplicate same-session INITIATE causes the HIL-RIG to resend RESPONSE;
- duplicate RESPONSE causes the host to resend CONFIRM;
- duplicate CONFIRM for an established HIL-RIG is re-ACKed without repeating
  the state transition; and
- an incompatible identity triggers the session reset/restart policy.

Handshake retransmission uses the same global `retransmit_timeout_ms` and
`max_retries` as other reliable protocol work. It must not invent a separate
hidden timing policy. If CONFIRM or its ACK is lost, the host retransmits the
exact same CONFIRM bytes, identity, and sequence under that policy. Exhausting
retries abandons the incomplete handshake and restarts establishment; the host
uses its next deterministically derived identity. It produces no Application
`DELIVERY_FAILED` event because no Application message was accepted.
Incompatible identities also cause complete abandonment and recovery rather
than partial handshake continuation. The normative MVP wire format defines the
binary representation of `INITIATE`, `RESPONSE`, `CONFIRM`, and `ACK`.
Handshake progression, state transitions, retry handling, and duplicate
behavior remain separate implementation work. A future extended profile may
define a different versioned representation without changing the public API.

The library never creates random identities. Host configuration supplies the
starting seed. A future deterministic progression skips invalid/reserved values,
handles wraparound, and avoids reusing an active identity. HIL-RIG adopts a
valid host identity. Transport identity remains independent of Application test
identity.

## Candidate reliability behavior

The MVP is intended to begin with genuine stop-and-wait: at most one reliable
frame exists while prepared, peeked, committed awaiting ACK, or waiting for
retransmission. Retransmission uses the same bytes, session identity, and
sequence. The matching ACK advances once; stale ACKs do not advance. A duplicate
received reliable frame is not delivered again, but its ACK may be sent again.

Explicitly unreliable ACK-only control traffic may bypass an occupied reliable
slot without becoming a second sequence domain. No control-priority rule may
reorder two reliable frames sharing a sequence domain.

Potential extended work includes bounded multi-message queues or multiple
sequence domains, but neither is approved. It requires an explicit ordering and
flow-control design plus tests before implementation.

For the MVP, retry exhaustion always makes the current session uncertain. An
ordinary accepted Application message produces `DELIVERY_FAILED`, all old
session work is abandoned, and recovery establishes a completely new session
before another Application message can be sent. Handshake exhaustion similarly
restarts establishment but produces no Application delivery event. Neither case
enters terminal FAULT, and neither may skip or reuse an uncertain sequence for a
different message in the old session. Reset/recovery clears all session,
handshake, sequence, ACK, retransmission, partial parsing, reassembly,
pinned-output, submitted-message, and unread-message state before a new session
begins.

## Candidate fragmentation and reassembly

The public facade already uses complete Application messages, so a future
extended profile can add transparent fragmentation without caller changes. A
candidate private frame design must represent:

- an identity shared by fragments of one Application message;
- the complete message length;
- fragment offset or equivalent position;
- one authoritative fragment length;
- safe beginning/completion detection; and
- enough retained coverage state to prove completeness.

Exact widths, byte order, flags, ordering, overlap handling, message-ID wrap,
coverage representation, and eviction rules remain open. A receiver must use
checked arithmetic, reject inconsistent metadata, reserve capacity before
acceptance, avoid counting duplicates twice, never expose partial messages, and
abandon incomplete state safely during reset.

## Candidate flow control and queueing

An extended profile may advertise bounded receive capacity, pace new data,
support keepalives, and retain multiple complete messages. Candidate advertised
capacity must never exceed storage actually available for parsing, reassembly,
completion, and metadata.

A zero Application-data window must not block ACK, recovery, keepalive, or
window-update control work. Transport cannot identify Application command
semantics, so all opaque Application messages are subject to the same byte-level
flow rules.

Open decisions include window units and width, update threshold, piggybacking,
control priority, queue depths, fairness, keepalive semantics, connection
timeout policy, and production values. None belongs in public queue-slot or
fragment structures.

## Common and profile-specific modules

The private `common` directory contains only genuinely shared seams for:

- integrity calculation; and
- opaque delimited-body accumulation with overflow discard and resynchronization.

These are not installed APIs. A parser must ignore empty delimiters, discard an
oversized body until the next delimiter, protect unread ready data, and report
exact consumed input to its profile. Frame codecs are profile-specific: the MVP
has a minimal one-complete-message codec seam, while the extended directory owns
the uncompiled fragment/window/keepalive frame skeleton. Any future codec must
use explicit fixed-width wire fields rather than packed/native C structures and
keep valid output retryable when a caller buffer is too small.

## Work required before enabling EXTENDED

The extended source skeleton must remain unavailable until it implements every
private profile operation and has focused tests for workspace sizing,
initialization failure atomicity, complete-message ownership, arbitrary input
chunks, stable peek/commit, reset cleanup, duplicate handling, reliability,
fragmentation/reassembly, flow control, and high-level diagnostics. Its wire
format requires a separate deliberate design decision and golden vectors.
