# Application wire-format reference

This document is the quick wire-level reference for the C Application codec. It describes the
architecture-independent bytes emitted and accepted by the current implementation. Message semantics,
transaction rules, and the intentional support boundary are described in
[Application messages](application_messages.md) and
[Application Layer codec and transaction design](application_layer.md).

The public C structures are **not** packed wire structures. Never derive an encoded layout from
`sizeof(enum)`, `sizeof(struct)`, native endianness, `size_t`, padding, unions, or pointer layout.
Every multi-byte wire integer described below is little-endian.

## Complete message at a glance

Every Application message is exactly one 23-byte common envelope followed by the payload declared in
that envelope:

```text
byte offset
  0       1       2       3                              19      20      21      23
  +-------+-------+-------+-------------------------------+-------+-------+-------+------------------+
  | major | minor |has ID |          Test ID              | type  |subtype|len LE |     payload      |
  | 1 B   | 1 B   | 1 B   |           16 B                | 1 B   | 1 B   | 2 B   |      N B         |
  +-------+-------+-------+-------------------------------+-------+-------+-------+------------------+
  |<-------------------------------- 23-byte envelope -------------------------------->|<--- N --->|
```

The exact envelope offsets are:

| Offset | Width | Field | Encoding rule |
| ---: | ---: | --- | --- |
| 0 | 1 byte | Overall HIL-RIG protocol major | `HIL_RIG_PROTOCOL_VERSION_MAJOR` |
| 1 | 1 byte | Overall HIL-RIG protocol minor | `HIL_RIG_PROTOCOL_VERSION_MINOR` |
| 2 | 1 byte | Test-ID-present flag | exactly `0x00` or `0x01` |
| 3 | 16 bytes | Test ID | opaque bytes; all zero is valid when present |
| 19 | 1 byte | Message type | explicit `uint8_t` protocol identifier |
| 20 | 1 byte | Message subtype | explicit `uint8_t` protocol identifier |
| 21 | 2 bytes | Payload length | little-endian `uint16_t` |
| 23 | N bytes | Payload | exactly the declared number of bytes |

There is no payload-end marker and no Application-specific version field. Patch version is not carried
in the common envelope. Ordinary messages require exact encoded major/minor equality with the compiled
repository version. A structurally valid BASIC System Information Request or Response with no Test ID is
accepted with foreign envelope major/minor only for discovery; its body major/minor must agree with the
envelope. It does not establish compatibility by decoding.

### Test ID encoding

When `has_test_id == 0`, bytes 3 through 18 **must all be zero**. When `has_test_id == 1`, all sixteen
bytes are opaque and every bit pattern is valid, including sixteen zero bytes.

```text
absent Test ID                              present Test ID
+------+----------------------+             +------+----------------------+
| 0x00 | 00 00 ... 00 (16 B) |             | 0x01 | opaque 16-byte value |
+------+----------------------+             +------+----------------------+
```

### Complete-message length rule

For an encoded input of `encoded_message_size` bytes:

```text
declared_total = 23 + payload_length

encoded_message_size < declared_total  -> TRUNCATED_MESSAGE
encoded_message_size > declared_total  -> MALFORMED_MESSAGE
encoded_message_size == declared_total -> body decoding may proceed
```

A malformed fixed body or malformed length-delimited byte span is also `MALFORMED_MESSAGE`.
A correctly shaped Test Configuration whose extension exceeds the initialized
`max_variable_data_size` is instead `VALIDATION_FAILED`; its shape is valid but it violates a local
structural policy bound. `BUFFER_TOO_SMALL` is reserved for insufficient caller-provided encode
capacity or decoded-data storage.

The default profile limits every complete Application message to 4096 bytes. A
context may select a smaller or larger `max_encoded_message_size`, up to the
65535-byte absolute ceiling. Legacy one-byte spans remain limited to 255
bytes; aligned variable records carry a two-byte length. A single Type 21 or
Type 34 tick may contain at most eight messages, so eight default-size messages
bound a chunked tick to 32768 complete encoded
bytes. These cross-message limits are enforced by endpoint integrations, not by
the stateless codec; no aggregate byte or record count is encoded.

## Fixed-width wire primitives

| Primitive | Width | Encoding |
| --- | ---: | --- |
| protocol/message enum identifier | 1 byte | explicit unsigned byte |
| `uint8_t` | 1 byte | unsigned byte |
| `uint16_t` | 2 bytes | little-endian |
| `uint32_t` | 4 bytes | little-endian |
| `uint64_t` | 8 bytes | little-endian |
| legacy byte span | `1 + N` bytes | one-byte length followed by exactly N bytes |
| aligned variable record span | `2 + N` bytes before padding | two-byte length followed by exactly N bytes |
| channel ID | 3 bytes | peripheral `uint8_t`, then channel `uint16_t` little-endian |

Legacy extension and diagnostic spans have a wire maximum of 255 data bytes:

```text
+----------+-------------------------------------+
| length   | data                                |
| uint8_t  | exactly length bytes                |
+----------+-------------------------------------+
    1 B                    N B
```

The C API still uses `size_t` for local capacities, offsets, and buffer sizes. The codec performs
checked conversion before putting a local length into a fixed-width wire field.

## System Information Request

This is the discovery request. The five-byte Execution Control and Global
Control payloads form the smallest currently supported complete message at 28
bytes; this request is 31 bytes.

- message type: `SYSTEM_INFO_REQUEST == 0x01`
- subtype: `BASIC == 0x01`
- Test ID: absent
- payload size: exactly 8 bytes
- complete message size: exactly 31 bytes

Payload:

```text
payload offset
  0       1       2       4       6       8
  +-------+-------+-------+-------+-------+
  | hash? | query | major | minor | patch |
  | 1 B   | 1 B   | u16LE | u16LE | u16LE |
  +-------+-------+-------+-------+-------+
```

| Payload offset | Width | Field | Initial valid values |
| ---: | ---: | --- | --- |
| 0 | 1 byte | `request_firmware_git_hash` | `0x00` or `0x01` |
| 1 | 1 byte | `query` | `BASIC == 0x01` |
| 2 | 2 bytes | application protocol major | little-endian `uint16_t` |
| 4 | 2 bytes | application protocol minor | little-endian `uint16_t` |
| 6 | 2 bytes | application protocol patch | little-endian `uint16_t` |

For repository protocol version 0.3.0, a BASIC request asking for the Git hash has this literal complete
wire vector:

```text
00 03 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
01 01 08 00
01 01 00 00 03 00 00 00
```

Broken down:

```text
00       protocol major = 0
03       protocol minor = 3
00       Test ID absent
00..00   16 zero Test-ID bytes
01       SYSTEM_INFO_REQUEST
01       BASIC subtype
08 00    payload length = 8
01       request firmware Git hash
01       BASIC query
00 00    protocol major = 0
03 00    protocol minor = 3
00 00    protocol patch = 0
```

The literal version bytes above are a golden example for protocol 0.3.0, not a rule that future protocol
versions remain 0.3.0.

## System Information Response

The response payload uses the following wire layout. All six public codec paths support it. The response
needs `D + G` caller decode-storage bytes, where D and G are diagnostic and Git-hash lengths. Each span
is bounded by `max_variable_data_size` and 255 wire bytes. Both spans may be 255 bytes, requiring 510
storage bytes and a 547-byte complete message when the configured complete-message limit permits it;
the default 4096-byte complete limit permits that message. Encoded sizing and encoding require
the response protocol triplet to equal the compiled library version; decode may accept a consistent
foreign discovery triplet for the explicit compatibility check.

```text
payload offset
  0       2       4       6       8      10      12               ...
  +-------+-------+-------+-------+-------+-------+----------+-----+----------+-----+
  |proto M|proto m|proto p| fw M  | fw m  | fw p  |diag len  |diag | git len  | git |
  | u16LE | u16LE | u16LE | u16LE | u16LE | u16LE |  u8      | N B |   u8     | M B |
  +-------+-------+-------+-------+-------+-------+----------+-----+----------+-----+
```

The two byte spans are encoded in **diagnostic-data first, firmware-Git-hash second** order, regardless
of their member order in the public C structure.

## Test Configuration

Test Configuration is fully supported by the stateless codec. It requires a
Test ID. The public C records are typed convenience structures only: their
padding, native enum widths, and native alignment never define this wire layout.
Every enum and Boolean occupies exactly one wire byte and every multi-byte
integer is little-endian.

The tick duration unit is **microseconds**. PWM periods use **nanoseconds** and
PWM duty uses **permyriad** (`0..10000`).

### Payload layout

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 12 | global fields: tick duration `uint32_t`, expected tick count `uint32_t`, flags `uint32_t` |
| 12 | 20 | 10 Digital Input records, 2 bytes each |
| 32 | 30 | 10 Digital Output records, 3 bytes each |
| 62 | 2 | 2 Analogue Input records, 1 byte each |
| 64 | 6 | 6 Analogue Output records, 1 byte each |
| 70 | 4 | 2 PWM Input records, 2 bytes each |
| 74 | 16 | 2 PWM Output records, 8 bytes each |
| 90 | 18 | 2 CAN records, 9 bytes each |
| 108 | 20 | 2 SPI records, 10 bytes each |
| 128 | 22 | 2 UART records, 11 bytes each |
| 150 | 20 | 2 I2C records, 10 bytes each |
| 170 | 1 | extension length, `uint8_t` |
| 171 | N | exactly N extension bytes |

The fixed payload, through and including the extension-length byte, is exactly
**171 bytes**. An empty-extension complete message is therefore `23 + 171 =
194` bytes. Extension length N produces `194 + N` complete bytes. The maximum
255-byte extension produces a 426-byte payload and a **449-byte complete
message**, which fits the 4096-byte default `max_encoded_message_size`. The
one-byte extension field sets the absolute wire maximum at 255 data bytes, but
encoding, decoding, decode-storage queries, and encoded-message validation also
enforce the initialized context's `max_variable_data_size`. A context may
therefore accept a smaller maximum without changing the wire layout.

The three global fields are encoded at payload offsets 0, 4, and 8. Supported
tick durations are 10000, 1000, 100, and 10 microseconds (100 Hz, 1 kHz, 10 kHz,
and 100 kHz respectively). Test-wide flags are reserved and must be zero.

### Fixed-array identity and disabled form

Every family is a fixed array whose extent is defined by its public physical
channel constant. Array element `i` configures logical channel `i`. No fixed
configuration record contains or encodes a peripheral identifier or channel ID.

Every record begins with one-byte `enabled`. `0x00` means disabled and `0x01`
means enabled; any other value is invalid. A disabled record is canonical only
when every remaining byte/field is zero. Enum value zero is therefore the
INVALID/canonical-disabled representation. Reserved enum sentinels use 255
where defined and are never valid enabled settings.

### Record layouts

Digital Input, 2 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 1 | voltage level |

Digital Output, 3 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 1 | voltage level |
| 2 | 1 | initial high state, Boolean |

Analogue Input and Analogue Output are each one byte: only `enabled`. There are
no protocol-selectable analogue electrical parameters in this version.

PWM Input, 2 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 1 | voltage level |

PWM Output, 8 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 1 | voltage level |
| 2 | 4 | initial period in nanoseconds, `uint32_t` LE |
| 6 | 2 | initial duty cycle in permyriad, `uint16_t` LE |

CAN, 9 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 2 | standard receive filter ID, `uint16_t` LE |
| 7 | 2 | standard receive filter mask, `uint16_t` LE |

Only 11-bit standard CAN identifiers are supported. A received frame matches when
`(received_standard_id & filter_mask) == (filter_id & filter_mask)`. A zero mask
accepts every standard identifier, including when `filter_id` is nonzero. Both
filter fields are host-selected protocol fields in the range `0x000..0x7FF`.
Filter-bank allocation remains firmware-internal. CAN-FD options, extended
identifiers, timing-register values, peripheral instances, and filter-bank
selection are not protocol fields. CAN termination is not software-configurable
through the Application protocol; physical termination must be fixed or handled
outside Application configuration.

SPI, 10 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 1 | master/slave role |
| 6 | 1 | data width |
| 7 | 1 | bit order |
| 8 | 1 | clock polarity |
| 9 | 1 | clock phase |

UART, 11 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | baud rate, `uint32_t` LE |
| 5 | 1 | electrical mode |
| 6 | 1 | word length |
| 7 | 1 | parity |
| 8 | 1 | stop bits |
| 9 | 1 | RX enabled, Boolean |
| 10 | 1 | TX enabled, Boolean |

I2C, 10 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 1 | master/slave role |
| 6 | 2 | own 7-bit address stored in `uint16_t`, LE |
| 8 | 1 | voltage level |
| 9 | 1 | pull-up selection |

### Enum assignments

All values below are one byte on the wire. `INVALID == 0` is also the zero value
required in a canonical disabled record. Each enum also defines `RESERVED ==
255`.

| Enum | Numeric assignments |
| --- | --- |
| Digital/PWM voltage | 1 = 3.3 V, 2 = 5 V, 3 = 12 V, 4 = 24 V |
| Bus role | 1 = master, 2 = slave |
| SPI data width | 1 = 8 bits, 2 = 16 bits |
| SPI bit order | 1 = MSB first, 2 = LSB first |
| SPI clock polarity | 1 = idle low, 2 = idle high |
| SPI clock phase | 1 = first edge, 2 = second edge |
| UART electrical mode | 1 = TTL 3.3 V, 2 = TTL 5 V, 3 = RS-232 |
| UART word length | 1 = 8 bits, 2 = 9 bits |
| UART parity | 1 = none, 2 = even, 3 = odd |
| UART stop bits | 1 = 1 stop bit, 2 = 2 stop bits |
| I2C voltage | 1 = 3.3 V, 2 = 5 V |
| I2C pull-up | 1 = 1 kΩ, 2 = 2.2 kΩ, 3 = 4.7 kΩ, 4 = 10 kΩ |

### Structural validation boundary

The codec validates the fixed wire shape and typed protocol rules: valid
Booleans/enums, canonical disabled records, a nonzero expected tick count within
the configured limit, supported tick duration, zero flags, extension length no
greater than `max_variable_data_size`, PWM duty no greater than 10000, zero duty
when period is zero, nonzero rates for enabled communications, 11-bit CAN
filter ID/mask bounds, UART RX/TX availability, and canonical disabled I2C
configuration. Enabled I2C is not implemented in v0.3.0 and returns
`HIL_APPLICATION_STATUS_NOT_IMPLEMENTED`; I2C masters use own address zero; I2C slaves use a
nonzero 7-bit address `1..127`.

The codec deliberately does not validate physical-channel availability, exact
supported rates, power-rail state, MCU timer/DMA/filter-bank settings,
complete-test storage capacity, cross-driver conflicts, or workflow state.
Analogue-input sampling frequency, analogue-output DAC/reference selection, and
hardware-specific rate/timing choices remain firmware policies. Unsupported
hardware configurations must be rejected by integration rather than silently
substituted. Those decisions belong to firmware integration. Type 21 and Type
34 communication operation/capture records are implemented bounded per-message
wire data; I2C operation and capture records remain invalid in v0.3.0. Their
cross-message assembly remains endpoint integration policy.

## Test Instruction fixed body

Test Instruction is a fully supported fixed codec family. It requires subtype `NONE` and a Test ID.
The payload is exactly 50 bytes and the complete message is exactly 73 bytes.

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | `tick_number`, `uint32_t` little-endian |
| 4 | 10 | Digital Output channels 0..9, one `uint8_t` each |
| 14 | 24 | Analogue Output channels 0..5, one `uint32_t` little-endian microvolt value each |
| 38 | 6 | PWM Output channel 0: `uint32_t` period ns + `uint16_t` duty permyriad, little-endian |
| 44 | 6 | PWM Output channel 1: same record |

The codec accepts only Digital values 0 and 1. PWM duty is valid from 0 through 10000, and a zero
period requires zero duty. `tick_number` must be less than `context->config.max_expected_tick_count`.
Analogue range and hardware-specific PWM feasibility are not validated. The codec retains no active
Test Configuration, so comparing the tick against that test's actual `expected_tick_count`, enabled
channels, or ordering is an integration responsibility.

## Update Instruction body

Update Instruction is a fully supported variable-length codec family (type 21). It requires subtype `NONE` and a Test ID. Each message contains one chunk of sparse logical peripheral operations and streaming serial data for one zero-based tick. Cross-message chunk order and family selection are endpoint integration rules.

### Payload header (8 bytes)

| Payload offset | Width | Field | Encoding rule |
| ---: | ---: | --- | --- |
| 0 | 4 | `tick_number` | little-endian `uint32_t`, must be `< max_expected_tick_count` |
| 4 | 1 | `operation_count` | unsigned byte, valid range `1..255` |
| 5 | 1 | `flags` | `0x00` (`COMPLETE_TICK`) or `0x01` (`HAS_MORE_CHUNKS`) |
| 6 | 2 | `reserved` | little-endian `uint16_t`, must be zero |

### Operation records (4-byte aligned TLV framing)

The header is immediately followed by `operation_count` records. Each record has a 4-byte header, followed by its payload bytes, and padded to a 4-byte boundary:

| Record offset | Width | Field | Encoding rule |
| ---: | ---: | --- | --- |
| 0 | 1 | `peripheral_type` | enum identifier (`DIGITAL_OUTPUT`, `ANALOG_OUTPUT`, `PWM_OUTPUT`, `UART`, `SPI`, `CAN`) |
| 1 | 1 | `channel` | logical channel index within peripheral family |
| 2 | 2 | `payload_length` | little-endian `uint16_t`, valid range `1..65535`, further limited by complete-message size and alignment |
| 4 | N | payload data | exactly `payload_length` bytes |
| 4 + N | 0..3 | zero padding | `(4 - (N % 4)) % 4` zero bytes to align to a 4-byte boundary |

Padding bytes must strictly be zero on the wire. No duplicate `(peripheral_type, channel)` pairs may appear in the same message.

#### Supported peripheral operation payloads:
- **`DIGITAL_OUTPUT`**: Channel must be 0 (bank 0). Payload length must be exactly 2 bytes (16-bit mask, bits 10..15 reserved zero).
- **`ANALOG_OUTPUT`**: Channel must be `< 6`. Payload length must be exactly 4 bytes (`uint32_t` little-endian microvolts).
- **`PWM_OUTPUT`**: Channel must be `< 2`. Payload length must be exactly 6 bytes (`uint32_t` period ns + `uint16_t` duty permyriad). Duty $\le 10000$, and zero period requires zero duty.
- **`UART`**: Channel must be `< 2`. Payload length must be `1..65535` bytes raw communication data, subject to message capacity.
- **`SPI`**: Channel must be `< 2`. Payload framing: `num_packets (1B)` + `packet_sizes (P bytes)` + `data (M bytes)`, where each packet size $\ge 1$ and $\sum \text{sizes} == M$.
- **`CAN`**: Channel must be `< 2`. Payload length must be a non-zero multiple of 12 bytes ($12 \times K$). Each frame is 2B `can_id` ($\le 0x7FF$) + 1B `dlc` ($\le 8$) + 8B data + 1B reserved zero.

## Execution Control and Global Control

Both current fixed control bodies are five bytes:

```text
+---------+-------------------+
| command | flags             |
| u8 enum | u32 little-endian |
+---------+-------------------+
   1 B             4 B
```

The initial protocol requires `flags == 0`. Execution Control requires a Test ID; Global Control
forbids one. `EXECUTION_CONTROL` is type 19 with subtype NONE: START is 1 and ABORT is 2.
`GLOBAL_CONTROL` is type 20 with subtype NONE: RESET_APPLICATION is 1 and GET_STATUS is 2. Every control complete message
is 28 bytes. Decoding never performs a control. START requires an accepted complete test; ABORT abandons
the identified operation; RESET_APPLICATION cleans Application state without resetting Transport. The
actual lifecycle checks and decisions about Application Responses remain endpoint integration work.

## Finalize Test Upload

`FINALIZE_TEST_UPLOAD` is type 22, subtype `NONE`, and is sent from Python to
firmware. It requires a Test ID and has a four-byte little-endian `flags` body;
`flags` is reserved and must be zero in v0.3.0. The payload is four bytes and
the complete message is exactly 27 bytes. It requires no decode storage.

The host sends it only after every submitted instruction tick has received a
positive Tick Response. It may also send it immediately after an accepted Test
Configuration when the sparse upload contains no instruction messages. The
request declares that no more instruction ticks or chunks will be submitted.
Firmware rejects it when a chunked tick is incomplete, the Test ID is wrong,
the upload is already invalid, storage is incomplete, or whole-test validation
fails. An accepted Complete Test Response commits the retained upload and makes
it eligible for START; a rejected response invalidates it. No instruction is
valid after successful finalisation, and START remains invalid before the
accepted Complete Test Response.

Only one response-requiring operation may be outstanding. If Transport/session
loss makes finalisation uncertain, the host enters the existing recovery path
and does not blindly resend the request. There is no reverse finalisation
message for results: fixed results and Type 34 results complete when tick
`expected_tick_count - 1` is complete, with Type 34 requiring its
`COMPLETE_TICK` chunk.

## Test Result fixed body

Test Result is a fully supported fixed codec family. It requires subtype `NONE` and a Test ID. The
payload is exactly 39 bytes and the complete message is exactly 62 bytes.

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | `tick_number`, `uint32_t` little-endian |
| 4 | 10 | Digital Input channels 0..9, one `uint8_t` each |
| 14 | 8 | Analogue Input channels 0..1, one `uint32_t` little-endian microvolt value each |
| 22 | 6 | PWM Input channel 0: `uint32_t` period ns + `uint16_t` duty permyriad, little-endian |
| 28 | 6 | PWM Input channel 1: same record |
| 34 | 1 | result condition |
| 35 | 4 | `problem_detail`, `uint32_t` little-endian |

The codec accepts only Digital values 0 and 1. PWM duty is valid from 0 through 10000, and a zero
period requires zero duty. `tick_number` must be less than `context->config.max_expected_tick_count`.
The only structurally valid conditions are `OK`, `PARTIAL`, and `EXECUTION_PROBLEM`; unknown and
reserved values are rejected. `PARTIAL` is representable. Analogue values and `problem_detail` have
no additional codec range rule. Active-test tick
comparison, enabled-channel semantics, result ordering, and hardware feasibility are integration-owned.

## Variable Test Result body

Variable Test Result is a fully supported variable-length codec family (type 34). It requires subtype `NONE` and a Test ID. Each message contains one result chunk with captured peripheral state and incoming communication buffers for one zero-based tick. Cross-message chunk order and assembly are endpoint integration rules.

### Payload header (12 bytes)

| Payload offset | Width | Field | Encoding rule |
| ---: | ---: | --- | --- |
| 0 | 4 | `tick_number` | little-endian `uint32_t`, must be `< max_expected_tick_count` |
| 4 | 1 | `record_count` | unsigned byte, valid range `0..255` |
| 5 | 1 | `condition` | `OK` (0), `PARTIAL` (1), or `EXECUTION_PROBLEM` (2) |
| 6 | 1 | `flags` | `0x00` (`COMPLETE_TICK`) or `0x01` (`HAS_MORE_CHUNKS`) |
| 7 | 1 | `reserved` | unsigned byte, must be zero |
| 8 | 4 | `problem_detail` | little-endian `uint32_t`, must be 0 when `condition == OK` |

When `record_count == 0`, the payload is exactly 12 bytes.

### Captured records (4-byte aligned TLV framing)

When `record_count > 0`, the header is followed by `record_count` records using the identical 4-byte-aligned TLV layout:

| Record offset | Width | Field | Encoding rule |
| ---: | ---: | --- | --- |
| 0 | 1 | `peripheral_type` | enum identifier (`DIGITAL_INPUT`, `ANALOG_INPUT`, `PWM_INPUT`, `UART`, `SPI`, `CAN`) |
| 1 | 1 | `channel` | logical channel index within peripheral family |
| 2 | 2 | `payload_length` | little-endian `uint16_t`, valid range `1..65535`, further limited by complete-message size and alignment |
| 4 | N | captured data | exactly `payload_length` bytes |
| 4 + N | 0..3 | zero padding | `(4 - (N % 4)) % 4` zero bytes to align to a 4-byte boundary |

Padding bytes must strictly be zero on the wire. No duplicate `(peripheral_type, channel)` pairs may appear in the same message.

#### Supported captured record payloads:
- **`DIGITAL_INPUT`**: Channel must be 0 (bank 0). Payload length must be exactly 2 bytes (16-bit mask, bits 10..15 reserved zero).
- **`ANALOG_INPUT`**: Channel must be `< 2`. Payload length must be exactly 4 bytes (`uint32_t` little-endian microvolts).
- **`PWM_INPUT`**: Channel must be `< 2`. Payload length must be exactly 6 bytes (`uint32_t` period ns + `uint16_t` duty permyriad). Duty $\le 10000$, zero period requires zero duty.
- **`UART`**: Channel must be `< 2`. Payload length must be `1..65535` bytes raw captured data, subject to message capacity.
- **`SPI`**: Channel must be `< 2`. Payload length must be `1..65535` bytes raw captured data, subject to message capacity.
- **`CAN`**: Channel must be `< 2`. Payload length must be a non-zero multiple of 12 bytes ($12 \times K$). Each frame is 2B `can_id` ($\le 0x7FF$) + 1B `dlc` ($\le 8$) + 8B data + 1B reserved zero.

## Arbitrary endpoint messages

Both families permit an optional envelope Test ID and require subtype `NONE`.
They work in either direction. Identifiers and values are ordinary unsigned
integers with no protocol whitelist.

| Type | Payload offset | Width | Field |
| --- | ---: | ---: | --- |
| `ARBITRARY_CONTROL` (64) | 0 | 4 | `control_id`, little-endian `uint32_t` |
| | 4 | 4 | `value`, little-endian `uint32_t` |
| `ARBITRARY_DATA` (65) | 0 | 4 | `data_id`, little-endian `uint32_t` |
| | 4 | 2 | `data_length` N, little-endian `uint16_t` |
| | 6 | N | opaque payload, exactly N bytes |

Control has an eight-byte payload and a 31-byte complete message; it needs no
decode storage. Data has a `6 + N` byte payload and a `29 + N` byte complete
message; decoding needs N caller-owned bytes, copied from the input. N may be
zero. The exact internal length is checked before local policy. N must not
exceed `max_variable_data_size`, and `29 + N` must fit the context's complete
message limit and the protocol's 65535-byte absolute ceiling. Thus the
absolute maximum N is 65506, the default 4096-byte profile permits 4067, and
an inspected 2302-byte firmware Application buffer permits 2273. The direct
USB prefix is separate from this Application size.

No payload encoding, alignment, reply, or custom-ID version field is imposed.
An unknown ID is a valid encoded container and is handled by endpoint code.

## Application Response

Application Response was introduced in v0.2.0 and remains supported as type 48, subtype `NONE`. Its body
is exactly 13 bytes and requires no decode storage.

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | scope enum |
| 1 | 1 | outcome enum |
| 2 | 1 | reason enum |
| 3 | 4 | tick number, `uint32_t` little-endian |
| 7 | 1 | execution control command enum |
| 8 | 1 | global control command enum |
| 9 | 4 | detail, `uint32_t` little-endian |

The five defined scopes, four defined outcomes, reasons `NONE` through
`INTERNAL_FAILURE`, execution commands `INVALID`/`START`/`ABORT`, and global
commands `INVALID`/`RESET_APPLICATION`/`GET_STATUS` are structurally valid. Global Control
scope forbids a Test ID; every other scope requires one. The codec does not
apply a scope/outcome/reason/command/tick compatibility matrix.

## Application Error

Application Error was introduced in v0.2.0 and remains supported as type 49, subtype `NONE`. Its body is
`12 + N` bytes.

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | category enum |
| 1 | 1 | recoverable, exactly 0 or 1 |
| 2 | 1 | tick present, exactly 0 or 1 |
| 3 | 4 | tick number, `uint32_t` little-endian |
| 7 | 4 | detail, `uint32_t` little-endian |
| 11 | 1 | diagnostic length N |
| 12 | N | diagnostic bytes |

Global Errors omit Test ID and tick; test-wide Errors include a Test ID and omit
the tick; tick-specific Errors include both. An absent tick requires a zero tick
field, and a present tick must be below `max_expected_tick_count`. The bounded
scanner proves the complete declared diagnostic span and rejects trailing bytes
before applying `max_variable_data_size`. Categories are not tied to Test-ID
presence, recoverability, endpoint state, or recovery action.

## Public API publication rules

The wire contract is paired with deterministic public output rules:

- `HIL_APPLICATION_Encode_Message()` sets `output_size` to zero before work and publishes a nonzero
  size only after the complete message succeeds. Buffer contents are unspecified after failure.
- `HIL_APPLICATION_Decode_Message()` sets used decoded storage to zero and the output message type to
  `INVALID` before parsing. Both remain in that unpublished state on every failure.
- `HIL_APPLICATION_Decode_Storage_Size()` and `HIL_APPLICATION_Validate_Encoded_Message()` clear their
  required-storage output before any failure. Storage sizing also validates the exact declared payload
  width for supported fixed-size families, so malformed fixed bodies are rejected consistently with
  normal decoding.
- No Application API allocates memory or retains caller pointers after returning.

## Current support boundary

Retired variable-message identifiers remain reserved. Type 21 and Type 34 bodies
are supported by the codec; endpoint workflow semantics remain outside the codec.
See the support
table in
[Application Layer codec and transaction design](application_layer.md#current-message-family-implementation-status)
before treating a payload family as fully operational.

## Rig Status and GET_STATUS


`GET_STATUS` retains the existing five-byte Global Control body: command byte 2 followed by four zero flag bytes. Complete size is 28 bytes. A successful query produces exactly one `RIG_STATUS` with origin QUERY_RESPONSE; a failed query produces an existing Global Control Response echoing `GET_STATUS`. There is no additional success Response.

`RIG_STATUS` has a fixed 12-byte payload and a **35-byte complete size**. It needs zero additional decode storage.

| Offset | Width | Field | Definition |
|---:|---:|---|---|
| 0 | 2 | `schema_version` | 1. |
| 2 | 1 | `origin` | QUERY_RESPONSE=1, NOTIFICATION=2; 0 invalid. |
| 3 | 1 | `state` | Public state enum below. |
| 4 | 4 | `flags` | Defined bits below; all other bits zero. |
| 8 | 1 | `failure_source` | Current fault/cancellation source or NONE. |
| 9 | 1 | `failure_stage` | Current fault/cancellation stage or NONE. |
| 10 | 2 | `failure_reason` | Stable reason or NONE. |

| State | Value | Meaning and firmware mapping |
|---|---:|---|
| INITIALISING | 1 | Host/RSM prerequisites have not been established. |
| IDLE | 2 | RSM idle; host ownership/output checks still determine readiness. |
| UPLOADING | 3 | Test package receive. |
| CONFIGURING | 4 | Configuration and execution preparation before ARMED. |
| ARMED | 5 | Uploaded test accepted for START. |
| RUNNING | 6 | Execution active. |
| FINALISING | 7 | Driver shutdown and result finalisation after execution. |
| RESULTS_READY | 8 | Results available, transfer not yet started. |
| TRANSFERRING | 9 | Results and terminal report are being sent. |
| RECOVERING | 10 | Reset/discard cleanup is progressing. |
| FAULT | 11 | Latched fault, including fault cleanup until reset. |

State 0 and undefined values are invalid. Map states explicitly rather than casting `RunState_T`. Fault takes precedence; an accepted reset may be reported as RECOVERING while carrying the fault it is clearing.

Flags are READY_FOR_NEW_TEST=`0x01`, TRANSITION_PENDING=`0x02`, RESET_PERMITTED=`0x04`, EXECUTION_ACTIVE=`0x08`. These flags describe the captured instant, not reservations against subsequent state changes.

READY_FOR_NEW_TEST requires IDLE, no active Test ID, no execution/timer activity, no fault, RSM cleanup complete, flash idle, no owned configuration, and no pending terminal report/control completion/old-result output that prevents admitting a fresh upload. READY implies RESET_PERMITTED and excludes TRANSITION_PENDING and EXECUTION_ACTIVE. Enforce these message-local combinations in the codec; verify hardware readiness in firmware.

Use the envelope Test ID only while a transaction remains active. A ready status has no ID. Unsolicited readiness notifications use origin NOTIFICATION, occur after compatible discovery and when readiness changes from false to true, and may be coalesced to the latest truthful status under backpressure. Query replies and terminal reports must not be coalesced away. A query remains authoritative if a notification was missed.


## Stable failure representation


Use shared failure enums for report and status:

| Enum | Values |
|---|---|
| `FailureSource` | NONE=0, RUN_STATE_MANAGER=1, EXECUTION_MANAGER=2, FLASH_MANAGER=3, HOST_INTERFACE=4, DRIVER_LIFECYCLE=5, EXECUTION_TIMER=6 |
| `FailureStage` | NONE=0, PREPARATION=1, EXECUTION=2, SHUTDOWN=3, FINALISATION=4, TRANSFER=5, CLEANUP=6 |

Define `FailureReason` as explicit values encoded in u16. The table is a semantic mapping, never an enum cast. Prefixes below identify existing native symbols where relevant.

| Wire value | Reason | Native origin / interpretation |
|---:|---|---|
| `0x0000` | NONE | No failure. |
| `0x0001` | HOST_ABORT | Validated protocol ABORT; source HOST_INTERFACE. |
| `0x0002` | APPLICATION_RESET | Reset cancels a still-open result transaction; source HOST_INTERFACE. |
| `0x0010` | INVALID_LIFECYCLE_STATE | RSM INVALID_TRANSITION. |
| `0x0011` | HARDWARE_NOT_READY | RSM LOGIC_EXPANDER_NOT_READY. |
| `0x0012` | CONFIGURATION_UNAVAILABLE | Same RSM suffix. |
| `0x0020` | DRIVER_CONFIGURATION_FAILED | RSM DRIVER_CONFIGURATION. |
| `0x0021` | DRIVER_CONFIGURATION_TIMEOUT | Same RSM suffix. |
| `0x0022` | DRIVER_START_FAILED | RSM DRIVER_START. |
| `0x0023` | DRIVER_START_TIMEOUT | Same RSM suffix. |
| `0x0024` | DRIVER_STOP_FAILED | RSM DRIVER_STOP. |
| `0x0025` | DRIVER_STOP_TIMEOUT | Same RSM suffix. |
| `0x0030` | ACQUISITION_EPOCH_FAILED | RSM ACQUISITION_EPOCH. |
| `0x0031` | EXECUTION_TIMER_FAILED | RSM EXECUTION_TIMER. |
| `0x0040` | EXECUTION_NOT_PREPARED | Execution Manager NOT_PREPARED. |
| `0x0041` | INSTRUCTION_UNDERRUN | Same Execution Manager suffix. |
| `0x0042` | INSTRUCTION_CORRUPT | Same Execution Manager suffix. |
| `0x0043` | INSTRUCTION_LATE | Same Execution Manager suffix. |
| `0x0044` | OPERATION_REJECTED | Same Execution Manager suffix. |
| `0x0045` | INSTRUCTION_CONSUME_FAILED | Execution Manager INSTRUCTION_CONSUME. |
| `0x0046` | MEASUREMENT_REJECTED | Same Execution Manager suffix. |
| `0x0047` | INSTRUCTION_UNCONSUMED | Same Execution Manager suffix. |
| `0x0050` | FLASH_PREPARATION_FAILED | RSM FLASH_EXECUTION_PREPARATION. |
| `0x0051` | FLASH_PREPARATION_TIMEOUT | RSM FLASH_EXECUTION_PREPARATION_TIMEOUT. |
| `0x0052` | FLASH_FINALISATION_FAILED | RSM FLASH_RESULT_FINALISATION. |
| `0x0053` | FLASH_FINALISATION_TIMEOUT | RSM FLASH_RESULT_FINALISATION_TIMEOUT. |
| `0x0054` | RESULT_TRANSFER_FAILED | RSM FLASH_RESULT_TRANSFER or result producer failure. |
| `0x0055` | RESULT_DISPOSITION_FAILED | RSM FLASH_RESULT_DISPOSITION. |
| `0x0056` | FLASH_MANAGER_FAILED | RSM FLASH_MANAGER. |
| `0x0060` | HOST_RESPONSE_BLOCKED | RSM HOST_INTERFACE_RESPONSE_BLOCKED. |
| `0x0061` | INSTRUCTION_UPLOAD_TIMEOUT | RSM HOST_INTERFACE_INSTRUCTION_UPLOAD_TIMEOUT. |
| `0x0062` | USB_INITIALISATION_FAILED | RSM HOST_INTERFACE_USB_INIT. |
| `0x0063` | CODEC_INITIALISATION_FAILED | RSM HOST_INTERFACE_CODEC_INIT. |
| `0x0064` | TRANSPORT_INITIALISATION_FAILED | RSM HOST_INTERFACE_TRANSPORT_INIT. |
| `0x0065` | HOST_INTERFACE_FAILED | RSM HOST_INTERFACE_ERROR. |
| `0x0070` | INTERNAL_FAILURE | RSM INTERNAL, unmapped internal failure, or external fault without known cancellation provenance. |

All other values are reserved in schema 1. NONE requires source/stage NONE. A nonzero reason requires nonzero source/stage. Capture stage at first failure; querying the RSM later when it is already in FAULT cannot recover that stage reliably.

The generic RSM EXTERNAL_REQUEST value does not prove that a Python host sent ABORT. Preserve validated command provenance in the host control state. Use INTERNAL_FAILURE for an unclassified external fault rather than inventing a host action.

## Run Report


### Purpose and size

One report closes the result stream of one admitted execution. It describes execution, result delivery by the rig, and the measurements associated with that execution. It is not a per-tick result or a receipt proving that Python stored the data.

The payload consists of a 32-byte header, 144 bytes of fixed statistic fields, and a one-byte extension length followed by 0–255 bytes. Statistic fields are always physically present; validity flags determine whether they mean anything.

- Payload length: **177 + E bytes**.
- Complete Application message: **200 + E bytes**, maximum **455 bytes**.
- Additional caller-owned decode storage: **E bytes**, solely for the extension.
- New messages therefore fit the existing 2302-byte firmware Application capacity without increasing it.

### Header layout

Offsets in this and subsequent report tables are relative to the start of the payload.

| Offset | Width | Field | Meaning |
|---:|---:|---|---|
| 0 | 2 | `schema_version` | Exactly 1. |
| 2 | 1 | `run_outcome` | Overall execution/result outcome through report construction. |
| 3 | 1 | `execution_outcome` | Whether execution started and how execution ended. |
| 4 | 1 | `result_status` | COMPLETE, PARTIAL or UNAVAILABLE as defined below. |
| 5 | 1 | `failure_source` | Stable protocol subsystem identifier. |
| 6 | 1 | `failure_stage` | Lifecycle stage of the first failure or cancellation. |
| 7 | 1 | Reserved | Zero. |
| 8 | 2 | `failure_reason` | Stable reason from the stable failure representation. |
| 10 | 2 | Reserved | Zero. |
| 12 | 4 | `valid_sections` | Validity bits from the validity rules below. |
| 16 | 4 | `expected_tick_count` | Number of requested measurement intervals, N. |
| 20 | 4 | `tick_period_us` | Nominal configured period for this run; not a calibrated measurement. |
| 24 | 4 | `last_completed_boundary` | Last fully successful execution boundary, when valid. |
| 28 | 4 | `result_ticks_emitted` | Number of complete logical result ticks accepted by the rig's ordered output path. |

Enums use these explicit values:

| Enum | Values |
|---|---|
| `RunOutcome` | INVALID=0, SUCCESS=1, FAILED=2, ABORTED=3 |
| `ExecutionOutcome` | INVALID=0, NOT_STARTED=1, COMPLETE=2, FAILED=3, ABORTED=4 |
| `RunResultStatus` | INVALID=0, COMPLETE=1, PARTIAL=2, UNAVAILABLE=3 |

There is no wire PENDING outcome. A rejected START creates no report. A START admitted by the RSM that subsequently fails during preparation does create a report with `execution_outcome=NOT_STARTED`.

Validate `1 <= expected_tick_count <= HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT`, `result_ticks_emitted <= expected_tick_count`, and a nominal period from the existing supported protocol period set. A valid last boundary must be at most N and must equal N when execution is COMPLETE. NOT_STARTED forbids measurement/last-boundary validity and requires zero emitted ticks. The codec checks widths and these explicit relationships; it does not attempt to prove physical consistency between independently supplied statistics, such as recomputing total cycles from a minimum and maximum.

`SUCCESS` requires execution COMPLETE, result status COMPLETE, `result_ticks_emitted=N`, and no failure. A non-success report carries a nonzero source, stage and reason. An intentional host abort/reset uses the cancellation reason, unless an earlier actual fault already determines the run outcome.

`result_status` describes the usable stream preceding this report:

- **COMPLETE:** N complete logical result ticks were emitted and their stream is trustworthy. Individual ticks may still have the existing per-tick PARTIAL/capture-overflow condition.
- **PARTIAL:** a trustworthy, contiguous prefix of 1 through N−1 complete result ticks was emitted. Discard an unfinished trailing variable-result tick. Firmware is not required to salvage such a prefix in this implementation.
- **UNAVAILABLE:** no trustworthy usable stream can be promised. Previously received results may be retained for diagnostics but must not be treated as a valid experiment dataset. `result_ticks_emitted` can be nonzero because it records transmission progress, not trustworthiness.

Increment `result_ticks_emitted` once per fixed result or final variable-result chunk accepted by the output path. Do not increment for an intermediate chunk, staging attempt or retry. Never count records as ticks. A later physical-link failure can still prevent host receipt.

### Statistic layout

| Offset | Width | Field | Existing native source |
|---:|---:|---|---|
| 32 | 4 | `isr_timing.sample_count` | Same field in `RunMetadataSnapshot_T`. |
| 36 | 8 | `isr_timing.total_cycles` | Same field. |
| 44 | 4 | `isr_timing.minimum_cycles` | Same field. |
| 48 | 4 | `isr_timing.maximum_cycles` | Same field. |
| 52 | 4 | `isr_timing.maximum_boundary` | Corrected maximum-sample-to-boundary mapping. |
| 56 | 4 | `instruction_buffer.sample_count` | Same field. |
| 60 | 4 | `instruction_buffer.minimum_unread_bytes` | Same field. |
| 64 | 4 | `instruction_buffer.minimum_boundary` | Same field. |
| 68 | 4 | `result_buffer.committed_record_count` | Same field. |
| 72 | 4 | `result_buffer.committed_bytes` | Same field; native record headers plus payload bytes. |
| 76 | 4 | `result_buffer.peak_pending_bytes` | Same field; committed bytes awaiting NAND drain. |
| 80 | 4 | `result_buffer.peak_pending_boundary` | Native execution timestamp associated with the peak. |
| 84 | 4 | `result_buffer.reserve_failure_count` | Same field. |
| 88 | 4 | `result_buffer.commit_failure_count` | Same field. |
| 92 | 4 | `flash.result_pages_drained` | `flash_throughput.result_pages_drained`. |
| 96 | 8 | `flash.result_bytes_drained` | Same native field name. |
| 104 | 8 | `flash.result_drain_total_cycles` | Same native field name. |
| 112 | 4 | `flash.result_drain_maximum_cycles` | Same native field name. |
| 116 | 4 | `flash.instruction_pages_refilled` | Same native field name. |
| 120 | 8 | `flash.instruction_bytes_refilled` | Same native field name. |
| 128 | 8 | `flash.instruction_refill_total_cycles` | Same native field name. |
| 136 | 4 | `flash.instruction_refill_maximum_cycles` | Same native field name. |
| 140 | 4 | `flash.instruction_publish_sample_count` | Same native field name. |
| 144 | 8 | `flash.instruction_publish_total_cycles` | Same native field name. |
| 152 | 4 | `flash.instruction_publish_maximum_cycles` | Same native field name. |
| 156 | 4 | `flash.service_gap_sample_count` | Same native field name. |
| 160 | 8 | `flash.service_gap_total_cycles` | Same native field name. |
| 168 | 4 | `flash.service_gap_maximum_cycles` | Same native field name. |
| 172 | 4 | `flash.refill_drain_contention_count` | Same native field name. |
| 176 | 1 | `extension_length` | E, 0–255. |
| 177 | E | `extension_data` | Opaque bytes; empty in the initial firmware integration. |

Keep full 64-bit totals through CFFI and Python integers. This layout does not include mean values, floating-point durations, host timestamps, calibrated clock estimates, peripheral-specific test scores or duplicate per-tick samples. Hosts can derive those quantities from the supplied data and experiment context.

### Validity, boundaries and sampling windows

Use bits 0–5 respectively for TERMINAL, LAST_COMPLETED_BOUNDARY, ISR_TIMING, INSTRUCTION_BUFFER, RESULT_BUFFER and FLASH_THROUGHPUT. Mask `0x3f` is the complete v1 mask. TERMINAL is mandatory; unknown bits are rejected. Invalid sections and an invalid last boundary must contain all-zero scalar fields, enforced by typed and encoded validation.

Zero is a valid sample value. For a valid ISR section with zero samples, total/minimum/maximum/boundary must be zero. For a valid instruction-buffer section with zero samples, minimum bytes and boundary must be zero. For flash subgroups with zero completed samples/pages, their associated totals and maxima are zero. Do not calculate a mean with a zero divisor.

Boundaries and result ticks are deliberately different:

- N requested intervals execute boundaries **0 through N**.
- Boundary 0 primes outputs and has no preceding measurement interval.
- Results use ticks **0 through N−1**, with result tick t associated with measurement boundary t+1.
- All report fields named `boundary` use the native boundary convention, including `last_completed_boundary=N` on a successful N-interval run when this field is valid.
- The timer numbers samples starting at 1. Its guarded IRQ records one timing sample per executed boundary, so the current mapping is `maximum_boundary = max_sample_number − 1`, with a nonzero sample number required. Add a regression for the first and last boundary. Guard-rejected IRQs must not increment the sample count.

ISR timing includes the boundary-zero sample and the measured IRQ work before the final context-switch request. It is neither interrupt latency nor a complete jitter distribution. Flash statistics are the execution-phase diagnostic snapshot captured after stopping the execution timer, before later finalisation drains; they are not lifetime NAND totals. Publication timings are components of flash service and must not be summed as independent work without accounting for overlap. Instruction-buffer minima are sampled while unread instructions remain. Keep these definitions in public documentation.

Take statistics only from the admitted execution generation. For NOT_STARTED, clear all measurement validity bits and fields, even if old hardware diagnostic counters are still nonzero. Document and verify collector bounds. If a native counter can overflow within a supported run, detect the overflow and mark its whole section invalid; do not silently wrap or truncate a value and publish it as a valid measurement. Collector overflow detection and hardware sampling are endpoint integration responsibilities; the protocol library does not collect these values.
