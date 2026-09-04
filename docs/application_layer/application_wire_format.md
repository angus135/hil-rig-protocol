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
`BUFFER_TOO_SMALL` is reserved for insufficient caller-provided encode capacity or decoded-data
storage.

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

The current fixed Test Configuration body is encoded and decoded, while detailed Digital, Analogue,
PWM, communication-family, extension, and test-wide flag semantics remain deliberately deferred.
The message requires a Test ID.

The tick duration unit is **microseconds**. PWM periods remain **nanoseconds**. These units are separate
protocol fields and must not be interchanged.

Current payload layout:

```text
payload offsets
   0        4        8       12        52       92       100      124      144      164    165
   +--------+--------+--------+---------+--------+--------+--------+--------+--------+------+---------+
   |tick us |ticks   | flags  |digital  |digital |analog  |analog  | PWM    | PWM    | ext  |ext data |
   | u32LE  | u32LE  | u32LE  | in x10  |out x10 | in x2  |out x6  | in x2  |out x2  | len  |  N B    |
   +--------+--------+--------+---------+--------+--------+--------+--------+--------+------+---------+
                              40 B      40 B      8 B      24 B     20 B     20 B      1 B
```

| Payload offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | tick duration in microseconds, `uint32_t` LE |
| 4 | 4 | `expected_tick_count`, `uint32_t` LE |
| 8 | 4 | reserved Test Configuration `flags`, `uint32_t` LE |
| 12 | 40 | 10 Digital Input configuration records |
| 52 | 40 | 10 Digital Output configuration records |
| 92 | 8 | 2 Analogue Input configuration records |
| 100 | 24 | 6 Analogue Output configuration records |
| 124 | 20 | 2 PWM Input configuration records |
| 144 | 20 | 2 PWM Output configuration records |
| 164 | 1 | extension-data length |
| 165 | N | extension-data bytes |

The fixed portion including the extension length byte is 165 bytes.

Supported tick-duration values are:

| Tick rate | Wire value |
| --- | ---: |
| 100 Hz | `10000 us` |
| 1 kHz | `1000 us` |
| 10 kHz | `100 us` |
| 100 kHz | `10 us` |

For example, a 1 ms tick is encoded as the literal four bytes `E8 03 00 00`, representing decimal
`1000` microseconds.

### Configuration subrecords

Digital and Analogue configuration records are both four bytes:

```text
+------------+-------------+---------------+
| peripheral | channel     | voltage level |
| u8         | u16 LE      | u8 enum       |
+------------+-------------+---------------+
       <--- 3-byte channel ID --->
```

PWM configuration records are ten bytes:

```text
+------------+-------------+-----------------+----------------+---------------+
| peripheral | channel     | period          | initial duty   | voltage level |
| u8         | u16 LE      | u32 LE, ns      | u16 LE         | u8 enum       |
+------------+-------------+-----------------+----------------+---------------+
       <--- channel ID --->
```

Communication-family configuration wire fields remain `NOT_IMPLEMENTED` and are not part of the
current fixed Test Configuration payload.

## Test Instruction fixed body

The current fixed body is 50 bytes. Variable instruction declarations/data remain deliberately
deferred and are not encoded by this fixed body.

```text
+----------------+----------------------+----------------------+----------------------+
| tick number    | digital outputs x10  | analogue outputs x6  | PWM outputs x2       |
| u32 LE         | u8 each              | u32 LE each, uV      | u32LE ns + u16LE duty|
+----------------+----------------------+----------------------+----------------------+
      4 B                 10 B                   24 B                   12 B
```

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

The current fixed result body is 39 bytes. Variable result declarations/data remain deliberately
deferred and are not encoded by this fixed body.

```text
+-----------+--------------------+--------------------+------------------+-----------+---------------+
| tick      | digital inputs x10 | analogue inputs x2 | PWM inputs x2    | condition | problem detail|
| u32 LE    | u8 each            | u32 LE each, uV    | u32LE ns+u16 duty| u8 enum   | u32 LE        |
+-----------+--------------------+--------------------+------------------+-----------+---------------+
    4 B              10 B                 8 B                12 B            1 B          4 B
```

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
complete. In particular, variable instruction/result bodies, Response/Error semantics, most
message-specific encoded-size functions, communication configuration, and detailed fixed-I/O
semantics remain deliberately deferred. See the support table in
[Application Layer codec and transaction design](application_layer.md#current-message-family-implementation-status)
before treating a payload family as fully operational.
