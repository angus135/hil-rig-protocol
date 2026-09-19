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
the default 512-byte complete limit may reject that larger message. Encoded sizing and encoding require
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
| 90 | 26 | 2 CAN records, 13 bytes each |
| 116 | 28 | 2 SPI records, 14 bytes each |
| 144 | 30 | 2 UART records, 15 bytes each |
| 174 | 28 | 2 I2C records, 14 bytes each |
| 202 | 1 | extension length, `uint8_t` |
| 203 | N | exactly N extension bytes |

The fixed payload, through and including the extension-length byte, is exactly
**203 bytes**. An empty-extension complete message is therefore `23 + 203 =
226` bytes. Extension length N produces `226 + N` complete bytes. The maximum
255-byte extension produces a 458-byte payload and a **481-byte complete
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

CAN, 13 bytes:

| Record offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | enabled |
| 1 | 4 | bit rate, `uint32_t` LE |
| 5 | 4 | capture limit in bytes, `uint32_t` LE |
| 9 | 2 | standard receive filter ID, `uint16_t` LE |
| 11 | 2 | standard receive filter mask, `uint16_t` LE |

Only 11-bit standard CAN identifiers are supported. A received frame matches when
`(received_standard_id & filter_mask) == (filter_id & filter_mask)`. A zero mask
accepts every standard identifier, including when `filter_id` is nonzero. Both
filter fields are host-selected protocol fields in the range `0x000..0x7FF`.
Filter-bank allocation remains firmware-internal. CAN-FD options, extended
identifiers, timing-register values, peripheral instances, and filter-bank
selection are not protocol fields. CAN termination is not software-configurable
through the Application protocol; physical termination must be fixed or handled
outside Application configuration.

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
greater than `max_variable_data_size`, 11-bit CAN filter ID/mask bounds, UART
RX/TX availability and RX/capture consistency, and I2C role/address rules. I2C masters use own address zero; I2C slaves use a
nonzero 7-bit address `1..127`.

The codec deliberately does not validate physical-channel availability, exact
supported rates, power-rail state, MCU timer/DMA/filter-bank settings,
complete-test storage capacity, cross-driver conflicts, or workflow state.
Analogue-input sampling frequency, analogue-output DAC/reference selection, and
hardware-specific rate/timing choices remain firmware policies. Unsupported
hardware configurations must be rejected by integration rather than silently
substituted. Those decisions belong to firmware integration. Type 21 and Type
34 communication operation/capture records are bounded per-message wire data;
their cross-message assembly remains endpoint integration policy.

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
| 2 | 2 | `payload_length` | little-endian `uint16_t`, valid range `1..255` |
| 4 | N | payload data | exactly `payload_length` bytes |
| 4 + N | 0..3 | zero padding | `(4 - (N % 4)) % 4` zero bytes to align to a 4-byte boundary |

Padding bytes must strictly be zero on the wire. No duplicate `(peripheral_type, channel)` pairs may appear in the same message.

#### Supported peripheral operation payloads:
- **`DIGITAL_OUTPUT`**: Channel must be 0 (bank 0). Payload length must be exactly 2 bytes (16-bit mask, bits 10..15 reserved zero).
- **`ANALOG_OUTPUT`**: Channel must be `< 6`. Payload length must be exactly 4 bytes (`uint32_t` little-endian microvolts).
- **`PWM_OUTPUT`**: Channel must be `< 2`. Payload length must be exactly 6 bytes (`uint32_t` period ns + `uint16_t` duty permyriad). Duty $\le 10000$, and zero period requires zero duty.
- **`UART`**: Channel must be `< 2`. Payload length must be `1..255` bytes raw communication data.
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
`GLOBAL_CONTROL` is type 20 with subtype NONE: RESET_APPLICATION is 1. Every control complete message
is 28 bytes. Decoding never performs a control. START requires an accepted complete test; ABORT abandons
the identified operation; RESET_APPLICATION cleans Application state without resetting Transport. The
actual lifecycle checks and decisions about Application Responses remain endpoint integration work.

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
| 2 | 2 | `payload_length` | little-endian `uint16_t`, valid range `1..255` |
| 4 | N | captured data | exactly `payload_length` bytes |
| 4 + N | 0..3 | zero padding | `(4 - (N % 4)) % 4` zero bytes to align to a 4-byte boundary |

Padding bytes must strictly be zero on the wire. No duplicate `(peripheral_type, channel)` pairs may appear in the same message.

#### Supported captured record payloads:
- **`DIGITAL_INPUT`**: Channel must be 0 (bank 0). Payload length must be exactly 2 bytes (16-bit mask, bits 10..15 reserved zero).
- **`ANALOG_INPUT`**: Channel must be `< 2`. Payload length must be exactly 4 bytes (`uint32_t` little-endian microvolts).
- **`PWM_INPUT`**: Channel must be `< 2`. Payload length must be exactly 6 bytes (`uint32_t` period ns + `uint16_t` duty permyriad). Duty $\le 10000$, zero period requires zero duty.
- **`UART`**: Channel must be `< 2`. Payload length must be `1..255` bytes raw captured data.
- **`SPI`**: Channel must be `< 2`. Payload length must be `1..255` bytes raw captured data.
- **`CAN`**: Channel must be `< 2`. Payload length must be a non-zero multiple of 12 bytes ($12 \times K$). Each frame is 2B `can_id` ($\le 0x7FF$) + 1B `dlc` ($\le 8$) + 8B data + 1B reserved zero.

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
commands `INVALID`/`RESET_APPLICATION` are structurally valid. Global Control
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
