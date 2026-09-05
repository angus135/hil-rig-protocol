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
in the common envelope. The decoder currently requires an exact encoded major/minor match with the
compiled repository version.

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

## Fixed-width wire primitives

| Primitive | Width | Encoding |
| --- | ---: | --- |
| protocol/message enum identifier | 1 byte | explicit unsigned byte |
| `uint8_t` | 1 byte | unsigned byte |
| `uint16_t` | 2 bytes | little-endian |
| `uint32_t` | 4 bytes | little-endian |
| `uint64_t` | 8 bytes | little-endian |
| byte span | `1 + N` bytes | one-byte length followed by exactly N bytes |
| channel ID | 3 bytes | peripheral `uint8_t`, then channel `uint16_t` little-endian |

A byte span therefore has a wire maximum of 255 data bytes:

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

This is the smallest currently supported complete Application message.

- message type: `SYSTEM_INFO_REQUEST == 0x01`
- subtype: `BASIC == 0x01`
- Test ID: absent
- payload size: exactly 2 bytes
- complete message size: exactly 25 bytes

Payload:

```text
payload offset
  0       1       2
  +-------+-------+
  | hash? | query |
  | 1 B   | 1 B   |
  +-------+-------+
```

| Payload offset | Width | Field | Initial valid values |
| ---: | ---: | --- | --- |
| 0 | 1 byte | `request_firmware_git_hash` | `0x00` or `0x01` |
| 1 | 1 byte | `query` | `BASIC == 0x01` |

For repository protocol version 0.1, a BASIC request asking for the Git hash has this literal complete
wire vector:

```text
00 01 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
01 01 02 00
01 01
```

Broken down:

```text
00       protocol major = 0
01       protocol minor = 1
00       Test ID absent
00..00   16 zero Test-ID bytes
01       SYSTEM_INFO_REQUEST
01       BASIC subtype
02 00    payload length = 2
01       request firmware Git hash
01       BASIC query
```

The literal version bytes above are a golden example for protocol 0.1, not a rule that future protocol
versions remain 0.1.

## System Information Response

The existing response payload encoder/decoder uses the following wire layout. Public validation also
requires the typed repository protocol version fields to match the compiled repository version.
Message-specific encoded-size and decode-storage sizing remain deliberately unfinished, so this family
is not yet fully supported by every public façade operation.

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
| 90 | 20 | 2 CAN records, 10 bytes each |
| 110 | 28 | 2 SPI records, 14 bytes each |
| 138 | 30 | 2 UART records, 15 bytes each |
| 168 | 28 | 2 I2C records, 14 bytes each |
| 196 | 1 | extension length, `uint8_t` |
| 197 | N | exactly N extension bytes |

The fixed payload, through and including the extension-length byte, is exactly
**197 bytes**. An empty-extension complete message is therefore `23 + 197 =
220` bytes. Extension length N produces `220 + N` complete bytes. The maximum
255-byte extension produces a 452-byte payload and a **475-byte complete
message**, which fits the 512-byte default `max_encoded_message_size`. The
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

CAN, 10 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 1 | termination enabled, Boolean |
| 6 | 4 | capture limit in bytes, `uint32_t` LE |

This is standard CAN only. CAN-FD options and implementation-specific filter
banks, filter IDs, filter masks, prescalers, and driver values are not protocol
fields.

SPI, 14 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 1 | master/slave role |
| 6 | 1 | data width |
| 7 | 1 | bit order |
| 8 | 1 | clock polarity |
| 9 | 1 | clock phase |
| 10 | 4 | capture limit in bytes, `uint32_t` LE |

UART, 15 bytes:

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
| 11 | 4 | capture limit in bytes, `uint32_t` LE |

I2C, 14 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 1 | master/slave role |
| 6 | 2 | own 7-bit address stored in `uint16_t`, LE |
| 8 | 1 | voltage level |
| 9 | 1 | pull-up selection |
| 10 | 4 | capture limit in bytes, `uint32_t` LE |

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
when period is zero, nonzero rates for enabled communications, capture limits no
greater than `max_variable_data_size`, CAN
termination Boolean, UART RX/TX availability and RX/capture consistency, and
I2C role/address rules. I2C masters use own address zero; I2C slaves use a
nonzero 7-bit address `1..127`.

The codec deliberately does not validate physical-channel availability, exact
supported rates, power-rail state, MCU timer/DMA/filter settings, complete-test
storage capacity, cross-driver conflicts, or workflow state. Those decisions
belong to firmware integration. Variable communication instruction/result
messages remain deferred even though communication Test Configuration is now
implemented.

## Test Instruction fixed body

Test Instruction is a fully supported fixed codec family. It requires subtype `NONE` and a Test ID.
The payload is exactly 50 bytes and the complete message is exactly 73 bytes. Variable instruction
declarations/data remain deliberately deferred and are not represented or encoded by this fixed body.

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
forbids one.

## Test Result fixed body

Test Result is a fully supported fixed codec family. It requires subtype `NONE` and a Test ID. The
payload is exactly 39 bytes and the complete message is exactly 62 bytes. Variable result
declarations/data remain deliberately deferred and are not represented or encoded by this fixed body.

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
reserved values are rejected. `PARTIAL` is representable even though variable result-data support is
deferred. Analogue values and `problem_detail` have no additional codec range rule. Active-test tick
comparison, enabled-channel semantics, result ordering, and hardware feasibility are integration-owned.

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

The presence of a documented identifier or public C structure does not mean all façade operations are
complete. In particular, variable instruction/result bodies, Response/Error semantics, and
other unfinished message-specific encoded-size/validation paths remain deliberately deferred. Test
Configuration plus the fixed Test Instruction and Test Result families are supported. See the support
table in
[Application Layer codec and transaction design](application_layer.md#current-message-family-implementation-status)
before treating a payload family as fully operational.
