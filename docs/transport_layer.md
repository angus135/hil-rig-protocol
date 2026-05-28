# Transport Layer

## 1. Purpose of the Transport Layer

The transport layer defines how binary protocol data is moved between the host software and the HIL-RIG MCU. It sits below the application layer and above the physical byte stream used to carry the data.

In practice, the physical byte stream may be USB CDC, UART, or another serial-style interface. The transport layer should not depend on the specific hardware peripheral. Its job is to take application-layer payload bytes, wrap them into a well-defined transport frame, encode that frame into a byte stream, and then allow the receiver to recover the original payload reliably.

At a high level, the transport layer answers these questions:

- Where does one packet start and end?
- How does the receiver detect corrupted data?
- How are partial packets handled?
- How does the receiver recover after malformed input or noise?
- How are sequence numbers, acknowledgements, keep-alives, and state-change requests represented?
- How does the host know whether the HIL-RIG is idle, configuring, armed, running, reporting, or faulted?
- How does the firmware and host software share exactly the same binary protocol definition?

The transport layer does not define the meaning of the application payload. It should not know whether the payload contains an instruction upload, a result packet, a status packet, or a capability query. That interpretation belongs to the application layer and the instruction/result encoding layers.

The transport layer should therefore be treated as a reusable framing, validation, and session-management layer for opaque application payloads.

## 2. Repository Scope

The `hil-rig-protocol` repository is intended to define the common binary protocol shared by the host-side API and the MCU firmware. The transport layer is one part of that repository.

The transport layer should include:

- COBS wire framing
- frame boundary detection
- raw transport frame encoding and decoding
- transport header definitions
- payload length handling
- CRC/checksum support
- incremental byte-stream parsing
- sequence and acknowledgement fields
- keep-alive frame support
- reset and fault signalling
- basic session state tracking
- malformed-frame rejection
- resynchronisation after corrupted input

The transport layer should not include:

- STM32 USB CDC driver code
- UART, DMA, GPIO, or FreeRTOS code
- application-layer packet IDs
- instruction layouts
- result layouts
- test execution scheduling
- flash loading behaviour
- Python user workflow logic
- plotting, logging, or CLI behaviour

The transport layer defines protocol-facing behaviour only. Hardware code should push received bytes into the transport layer and transmit encoded bytes produced by the transport layer, but it should not be embedded inside the transport implementation.

## 3. Conceptual Layering

The intended layering is:

```text
Physical byte stream
    USB CDC, UART, or similar

COBS wire framing
    Encodes complete transport frames and uses 0x00 as the frame delimiter

Transport frame
    Contains the transport header, opaque application payload, and CRC

Transport session
    Tracks role, state, sequence numbers, acknowledgements, keep-alives, and timeouts

Application packet
    Interprets the opaque payload as a protocol command, response, instruction upload, result download, etc.

Instruction/result encoding
    Interprets application payload contents in detail
```

The transport layer owns the middle three parts:

```text
COBS wire framing
Transport frame
Transport session
```

The application layer begins only after a complete, valid transport frame has been decoded and accepted.

## 4. Data Flow Overview

### 4.1 Transmit Path

When the host or HIL-RIG wants to send data, the data should move through the stack as follows:

```text
Application object
    ↓
Application-layer payload bytes
    ↓
Transport session builds frame metadata
    ↓
Transport frame encoder creates raw transport frame
    ↓
CRC is appended or inserted
    ↓
COBS encoder creates wire-safe byte stream
    ↓
0x00 delimiter is appended
    ↓
USB CDC/UART/other byte stream transmits bytes
```

For example, when the host sends a configuration packet:

```text
Instruction/configuration data
    ↓
Application packet encode
    ↓
Transport data frame
    ↓
CRC-protected raw frame
    ↓
COBS-encoded wire frame
    ↓
USB CDC transmit
```

The transport layer does not need to know that the payload is a configuration packet. It only sees payload bytes.

### 4.2 Receive Path

When bytes are received, the data flow is reversed:

```text
USB CDC/UART/other byte stream receives bytes
    ↓
Transport parser accumulates bytes until 0x00 delimiter
    ↓
COBS decoder reconstructs raw transport frame
    ↓
Transport frame decoder checks header, length, and CRC
    ↓
Transport session validates sequence/state/ACK behaviour
    ↓
Opaque application payload is delivered upward
    ↓
Application layer decodes packet meaning
```

The receiver may not receive one frame at a time. A single USB CDC receive callback may contain:

- half a frame
- one complete frame
- multiple complete frames
- random noise before a valid frame
- a corrupted frame followed by a valid frame

For this reason, byte-stream parsing must be separated from complete-frame decoding.

## 5. Why COBS Is Used

COBS, or Consistent Overhead Byte Stuffing, is used to make frame boundaries simple and reliable.

The intended wire format is:

```text
COBS(raw_transport_frame) 0x00
COBS(raw_transport_frame) 0x00
COBS(raw_transport_frame) 0x00
```

COBS ensures that the encoded frame body contains no `0x00` bytes. This allows `0x00` to be used as an unambiguous frame delimiter.

This avoids the need for a custom start marker, end marker, or escape-byte scheme. It also makes receiver resynchronisation straightforward: if a corrupted frame is rejected, the parser can discard data up to the next delimiter and then attempt to decode the next frame.

COBS is a good fit for this project because:

- it is byte-oriented
- it is deterministic
- it has low and bounded overhead
- it does not require heap allocation
- it is simple enough to implement or vendor as a small C module
- it works well with USB CDC and UART-style streams
- it keeps framing independent from application payload contents

The transport layer should own COBS encoding and decoding. The USB CDC or UART driver should only handle raw byte movement.

## 6. Raw Transport Frame

The raw transport frame is the decoded frame content before COBS encoding.

Conceptually, it should contain:

```text
transport header
application payload
CRC
```

The exact byte layout is still to be finalised, but the frame should contain enough information to support:

- protocol/wire version
- frame type
- flags
- sequence number
- acknowledgement number
- payload length
- current state
- requested state
- advertised window size
- reserved fields for future use
- CRC or checksum

The raw transport frame should be explicitly byte-encoded. The implementation should not rely on compiler struct packing as the wire format.

That means the encoder should write each field manually in a fixed endian format, for example:

```text
write_u8()
write_u16_be()
write_u32_be()
```

and the decoder should read fields in the corresponding way.

This avoids problems caused by:

- compiler padding
- alignment differences
- endian differences
- implementation-defined enum sizes
- different host and embedded compiler behaviour

## 7. Transport Header Responsibilities

The transport header is the fixed metadata at the front of the raw frame. It describes how the frame should be handled, but it does not define the application payload meaning.

The header should eventually represent the following concepts.

### 7.1 Role

The source role identifies whether the frame was sent by the host or by the HIL-RIG.

```c
typedef enum
{
    HIL_TRANSPORT_ROLE_HOST = 0,
    HIL_TRANSPORT_ROLE_RIG  = 1
} HIL_Transport_Role_T;
```

This is useful for validation. A host-side session should generally expect incoming frames from the rig, and a rig-side session should generally expect incoming frames from the host.

### 7.2 Transport State

The transport state describes the broad protocol state of the sender.

```c
typedef enum
{
    HIL_TRANSPORT_STATE_IDLE        = 0,
    HIL_TRANSPORT_STATE_CONFIGURING = 1,
    HIL_TRANSPORT_STATE_ARMED       = 2,
    HIL_TRANSPORT_STATE_RUNNING     = 3,
    HIL_TRANSPORT_STATE_REPORTING   = 4,
    HIL_TRANSPORT_STATE_FAULT       = 5,
    HIL_TRANSPORT_STATE_RESERVED_6  = 6,
    HIL_TRANSPORT_STATE_RESERVED_7  = 7
} HIL_Transport_State_T;
```

These states are transport-visible states. They are not the complete firmware execution state.

For example, `RUNNING` means that the protocol-visible HIL-RIG session is running a test. It does not describe the internal execution-manager ISR state, DMA state, flash buffering state, or scheduler details.

### 7.3 Frame Type

The frame type describes the broad purpose of the frame.

```c
typedef enum
{
    HIL_TRANSPORT_FRAME_TYPE_DATA       = 0,
    HIL_TRANSPORT_FRAME_TYPE_ACK        = 1,
    HIL_TRANSPORT_FRAME_TYPE_NACK       = 2,
    HIL_TRANSPORT_FRAME_TYPE_KEEP_ALIVE = 3,
    HIL_TRANSPORT_FRAME_TYPE_RESET      = 4,
    HIL_TRANSPORT_FRAME_TYPE_CONTROL    = 5
} HIL_Transport_Frame_Type_T;
```

Typical meanings:

- `DATA`: carries opaque application payload bytes
- `ACK`: acknowledges a received frame or state change
- `NACK`: rejects a frame or requests retransmission
- `KEEP_ALIVE`: confirms that the peer is still connected
- `RESET`: requests or confirms protocol reset
- `CONTROL`: carries transport-level control information

### 7.4 Flags

Flags refine the meaning of the frame.

Examples:

```c
#define HIL_TRANSPORT_FLAG_ACK              (1u << 0)
#define HIL_TRANSPORT_FLAG_NACK             (1u << 1)
#define HIL_TRANSPORT_FLAG_RST              (1u << 2)
#define HIL_TRANSPORT_FLAG_FIN              (1u << 3)
#define HIL_TRANSPORT_FLAG_KEEP_ALIVE       (1u << 4)
#define HIL_TRANSPORT_FLAG_STATE_CHANGE     (1u << 5)
#define HIL_TRANSPORT_FLAG_STATE_CHANGE_ACK (1u << 6)
#define HIL_TRANSPORT_FLAG_MORE_FRAGMENTS   (1u << 7)
```

These should be used carefully. The frame type should describe the main category of the frame, while flags should describe secondary properties.

For example:

- a data frame may also acknowledge previous data
- a state-change frame may request transition into `CONFIGURING`
- a reporting data frame may use `FIN` to mark the final result frame
- a reset frame may use the reset flag for explicit reset semantics

### 7.5 Sequence and Acknowledgement Numbers

Sequence numbers allow the receiver to detect duplicate, stale, or missing frames.

Acknowledgement numbers allow the receiver to tell the sender what has been received.

The first implementation does not need to implement a full sliding-window protocol. A staged approach is preferred:

1. include sequence and acknowledgement fields in the header
2. use them for diagnostics and basic duplicate detection
3. use them for reliable state-change acknowledgement
4. later extend them for application-data retransmission if needed
5. only add true windowed transfer if large result or instruction transfers require it

The recommended receive behaviour is:

```text
incoming sequence == expected sequence:
    accept frame

incoming sequence < expected sequence:
    treat as duplicate or stale

incoming sequence > expected sequence:
    treat as gap or missing frame
```

The exact retry and NACK behaviour should be defined before full implementation.

### 7.6 Payload Length

The header should include the payload length in bytes.

The transport layer should treat payloads as byte-oriented. Bit-level instruction packing belongs above the transport layer.

This means the transport frame should look conceptually like:

```c
typedef struct
{
    HIL_Transport_Header_T header;
    const uint8_t *payload;
    size_t payload_len;
} HIL_Transport_Frame_T;
```

The transport layer should not expose payloads as integer bitstreams. If the application layer wants to pack non-byte-aligned fields, it should pack them into a byte buffer first.

### 7.7 CRC

The CRC detects corrupted raw transport frames.

The CRC should be calculated over the decoded raw transport frame, not over the COBS-encoded wire data. This keeps CRC behaviour independent from the framing method.

The final CRC parameters must be explicitly defined before golden vectors are created:

- polynomial
- initial value
- final XOR
- reflection behaviour
- byte order in the frame
- whether the CRC field is excluded or treated as zero during calculation

CRC32 is preferred unless there is a strong reason to use a 16-bit checksum.

## 8. Parser Responsibilities

The parser converts an arbitrary byte stream into complete encoded frames.

It should not understand application payload contents. Its job is to identify complete COBS-delimited frames and provide them to the decode path.

The parser should handle:

- byte-at-a-time input
- multi-byte input buffers
- partial frame delivery
- multiple frames in one received chunk
- empty delimiters
- oversized frames
- malformed COBS blocks
- noise before a valid delimiter
- resynchronisation after corrupted data

The parser should use a caller-provided scratch buffer. It should not allocate memory internally.

Conceptually:

```text
for each received byte:
    if byte != 0x00:
        append byte to scratch buffer
    else:
        if scratch buffer is empty:
            ignore empty delimiter
        else:
            mark encoded frame as ready
```

The complete encoded frame can then be passed into the frame decoder.

The parser should be able to report whether it needs more data, has a frame ready, rejected a frame, or had to resynchronise.

## 9. Frame Encoder and Decoder Responsibilities

The frame encoder and decoder are responsible for complete frames, not streaming input.

### 9.1 Encoder

The encoder should take:

```text
HIL_Transport_Frame_T
```

and produce:

```text
COBS(raw transport header + payload + CRC) + 0x00
```

The encoder should:

1. validate input arguments
2. validate header fields
3. validate payload length
4. calculate the encoded size
5. write the raw transport frame into an internal or caller-provided temporary buffer
6. calculate and insert the CRC
7. COBS encode the raw frame
8. append the `0x00` delimiter
9. return the number of bytes written

The encoder should return `HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL` if the output buffer is not large enough.

### 9.2 Decoder

The decoder should take one complete encoded frame and produce:

```text
HIL_Transport_Frame_T
```

plus payload bytes copied into a caller-provided buffer.

The decoder should:

1. validate input arguments
2. COBS decode the encoded frame
3. validate minimum raw frame length
4. decode the transport header
5. validate the payload length
6. validate reserved fields where required
7. verify the CRC
8. copy the payload into the caller-provided payload buffer
9. return the decoded frame metadata and payload length

The decoder should reject:

- malformed COBS data
- unsupported wire protocol versions
- truncated headers
- invalid frame lengths
- invalid states
- invalid frame types
- bad CRC values
- payload lengths larger than the caller-provided buffer

## 10. Session Layer Responsibilities

The session layer is separate from raw frame encoding and decoding.

The encoder and decoder answer:

```text
Can this frame be converted to or from bytes?
```

The session layer answers:

```text
Does this frame make sense in the current protocol conversation?
```

The session layer should track:

- local role
- local transport state
- peer state, if required
- next sequence number
- expected peer sequence number
- last acknowledgement number
- connection status
- fault status
- keep-alive timing
- pending state-change request
- state-change retry count
- statistics

A session object should not transmit bytes itself. Instead, it should build frames or return events that the caller can act on.

This keeps the transport session usable from:

- firmware code
- host-side C or Python bindings
- unit tests
- integration tests
- simulated links

## 11. Transport State Flow

The intended protocol-visible state flow is:

```text
IDLE
    ↓
CONFIGURING
    ↓
ARMED
    ↓
RUNNING
    ↓
REPORTING
    ↓
IDLE
```

Faults may occur from most states:

```text
IDLE ───────────────→ FAULT
CONFIGURING ───────→ FAULT
ARMED ─────────────→ FAULT
RUNNING ───────────→ FAULT
REPORTING ─────────→ FAULT
```

Reset should be the normal way to return from `FAULT` to `IDLE`.

### 11.1 IDLE

`IDLE` means no active test is being configured, executed, or reported.

Typical actions:

- host may request entry into `CONFIGURING`
- rig may acknowledge state
- either side may send keep-alives if connected
- reset may keep or return the session to `IDLE`

### 11.2 CONFIGURING

`CONFIGURING` means the host is sending test configuration or instruction data.

Typical actions:

- host sends application data frames containing configuration/application payloads
- rig accepts or rejects payloads
- host signals end of configuration
- rig moves to `ARMED` or `RUNNING`, depending on execution mode

### 11.3 ARMED

`ARMED` means the rig has received enough configuration to run, but is waiting for an explicit execute request.

Typical actions:

- host sends execute state-change request
- rig acknowledges and enters `RUNNING`
- timeout or invalid command may move to `FAULT`
- reset may return to `IDLE`

### 11.4 RUNNING

`RUNNING` means the configured test is executing.

Typical actions:

- keep-alives may continue
- application data transfer should generally be limited
- host should not send new configuration data
- rig eventually transitions to `REPORTING`
- fault conditions move to `FAULT`

The transport layer should not implement the test execution itself. It only represents the state at the protocol boundary.

### 11.5 REPORTING

`REPORTING` means the rig is sending result data back to the host.

Typical actions:

- rig sends result payloads as application data frames
- host acknowledges result transfer
- final frame or explicit reporting-complete acknowledgement returns the session to `IDLE`
- transfer failure may move to `FAULT`

### 11.6 FAULT

`FAULT` means the protocol session has encountered an unrecoverable or explicitly reported error.

Typical causes:

- keep-alive timeout
- invalid state transition
- repeated acknowledgement timeout
- malformed critical frame
- application-level fault reported through the transport-visible state

Typical actions:

- peer is notified of `FAULT`
- normal data transfer stops
- reset is required to return to `IDLE`

## 12. Keep-Alive Behaviour

Keep-alives are used to detect whether the peer is still responsive.

A keep-alive frame should be small and should not carry application payload data.

The session layer should track:

- last transmitted keep-alive time
- last received keep-alive time
- last received packet time
- keep-alive interval
- keep-alive timeout

Typical behaviour:

```text
if connected and no keep-alive has been sent within the interval:
    build keep-alive frame

if no frame or keep-alive has been received within the timeout:
    enter FAULT or report timeout event
```

The exact timeout values should be configurable.

The session should not call timers directly. Instead, the caller should pass the current timestamp into functions such as:

```c
HIL_TRANSPORT_Session_Tick(&session, now_ms, &event);
```

This keeps the transport library independent of any operating system or hardware timer.

## 13. State-Change Acknowledgement

State changes should be explicit and acknowledged.

For example, when the host wants to configure the rig:

```text
host:
    request state change to CONFIGURING

rig:
    validates transition from IDLE to CONFIGURING
    updates state
    sends state-change acknowledgement

host:
    receives acknowledgement
    clears pending state-change state
```

The session layer should track pending state changes so that timeout and retry behaviour can be implemented.

Conceptually:

```text
pending_state_change_valid
pending_state_change
pending_state_change_since_ms
pending_state_change_retries
```

If the acknowledgement is not received in time, the session may retransmit the state-change request. If the retry limit is exceeded, the session may enter `FAULT`.

The first implementation can support state-change acknowledgements before implementing full data-frame retransmission.

## 14. ACK/NACK Behaviour

ACK and NACK support should be introduced progressively.

The initial useful behaviour is:

- ACK state-change requests
- ACK important control frames
- reject malformed frames before they reach the session
- optionally NACK sequence gaps or unsupported transitions

Full reliable data transfer does not need to be implemented immediately.

A staged approach is preferred:

### Stage 1: Frame Validation Only

- encode/decode frames
- verify CRC
- reject malformed input
- expose sequence fields but do not enforce full reliability

### Stage 2: Reliable State Changes

- state-change requests require acknowledgement
- missing acknowledgement triggers retry
- repeated failure enters `FAULT`

### Stage 3: Basic Data Sequence Checking

- expected sequence numbers are enforced
- duplicate frames are detected
- sequence gaps are reported

### Stage 4: Data Retransmission

- data frames can be retransmitted after timeout
- NACK can request retransmission
- sender tracks unacknowledged frames

### Stage 5: Fragmentation and Windowing

- large application payloads may be split across frames
- `MORE_FRAGMENTS` and `FIN` indicate payload boundaries
- advertised window size becomes meaningful

The early implementation should not attempt to become a full TCP replacement.

## 15. Public API Relationship

The transport layer API can be understood as four groups:

```text
types/status
frame encode/decode
incremental parser
CRC helpers
session helpers
```

### 15.1 Types and Status

Defined by headers such as:

```text
transport_status.h
transport_types.h
```

These define common enums, structs, flags, and constants used across the rest of the transport layer.

Examples:

```c
HIL_Transport_Status_T
HIL_Transport_Role_T
HIL_Transport_State_T
HIL_Transport_Frame_Type_T
HIL_Transport_Header_T
HIL_Transport_Frame_T
HIL_Transport_Event_T
```

These types are shared across the encoder, parser, and session APIs.

### 15.2 Frame API

Defined by:

```text
transport_frame.h
```

The frame API converts between structured transport frames and encoded wire bytes.

Main functions:

```c
size_t HIL_TRANSPORT_Encoded_Size(
    const HIL_Transport_Frame_T *frame
);

HIL_Transport_Status_T HIL_TRANSPORT_Encode_Frame(
    const HIL_Transport_Frame_T *frame,
    uint8_t *out_buffer,
    size_t out_buffer_len,
    size_t *bytes_written
);

HIL_Transport_Status_T HIL_TRANSPORT_Decode_Frame(
    const uint8_t *encoded_frame,
    size_t encoded_frame_len,
    HIL_Transport_Frame_T *out_frame,
    uint8_t *payload_buffer,
    size_t payload_buffer_len,
    size_t *payload_bytes_written
);
```

These functions operate on complete frames.

Use this API when:

- the caller already has one complete frame to encode
- the parser has already identified one complete encoded frame
- a test wants to perform a direct frame round trip
- golden vectors are being checked

Do not use this API directly for arbitrary USB CDC receive chunks. Use the parser for that.

### 15.3 Parser API

Defined by:

```text
transport_parser.h
```

The parser API converts arbitrary received bytes into complete encoded frames.

Main functions:

```c
HIL_Transport_Status_T HIL_TRANSPORT_Parser_Init(
    HIL_Transport_Parser_T *parser,
    uint8_t *scratch_buffer,
    size_t scratch_buffer_len
);

HIL_Transport_Parse_Result_T HIL_TRANSPORT_Parser_Push_Byte(
    HIL_Transport_Parser_T *parser,
    uint8_t byte
);

HIL_Transport_Parse_Result_T HIL_TRANSPORT_Parser_Push_Bytes(
    HIL_Transport_Parser_T *parser,
    const uint8_t *data,
    size_t data_len,
    size_t *bytes_consumed
);

HIL_Transport_Status_T HIL_TRANSPORT_Parser_Read_Frame(
    HIL_Transport_Parser_T *parser,
    HIL_Transport_Frame_T *out_frame,
    uint8_t *payload_buffer,
    size_t payload_buffer_len,
    size_t *payload_bytes_written
);

void HIL_TRANSPORT_Parser_Reset(
    HIL_Transport_Parser_T *parser
);
```

Use this API when:

- bytes are arriving from USB CDC
- bytes are arriving from UART
- frames may be split across callbacks
- multiple frames may arrive at once
- corrupted input may require resynchronisation

The parser should eventually call the frame decoder once a delimiter has been found.

### 15.4 CRC API

Defined by:

```text
transport_crc.h
```

The CRC API isolates the checksum calculation from the rest of the frame logic.

Main functions:

```c
uint32_t HIL_TRANSPORT_CRC32_Init(void);

uint32_t HIL_TRANSPORT_CRC32_Update(
    uint32_t crc,
    const uint8_t *data,
    size_t len
);

uint32_t HIL_TRANSPORT_CRC32_Finish(
    uint32_t crc
);

uint32_t HIL_TRANSPORT_CRC32_Compute(
    const uint8_t *data,
    size_t len
);
```

The frame encoder and decoder should use these functions internally.

The CRC API is also useful for tests and golden vector generation.

### 15.5 Session API

Defined by:

```text
transport_session.h
```

The session API tracks protocol conversation state.

Main functions:

```c
HIL_Transport_Status_T HIL_TRANSPORT_Session_Init(
    HIL_Transport_Session_T *session,
    HIL_Transport_Role_T role,
    const HIL_Transport_Session_Config_T *config
);

HIL_Transport_Status_T HIL_TRANSPORT_Session_Build_Data_Frame(
    HIL_Transport_Session_T *session,
    const uint8_t *payload,
    size_t payload_len,
    HIL_Transport_Frame_T *out_frame
);

HIL_Transport_Status_T HIL_TRANSPORT_Session_Build_Keep_Alive(
    HIL_Transport_Session_T *session,
    uint32_t now_ms,
    HIL_Transport_Frame_T *out_frame
);

HIL_Transport_Status_T HIL_TRANSPORT_Session_Request_State_Change(
    HIL_Transport_Session_T *session,
    HIL_Transport_State_T requested_state,
    uint32_t now_ms,
    HIL_Transport_Frame_T *out_frame
);

HIL_Transport_Status_T HIL_TRANSPORT_Session_Receive_Frame(
    HIL_Transport_Session_T *session,
    const HIL_Transport_Frame_T *frame,
    uint32_t now_ms,
    HIL_Transport_Event_T *out_event
);

HIL_Transport_Status_T HIL_TRANSPORT_Session_Tick(
    HIL_Transport_Session_T *session,
    uint32_t now_ms,
    HIL_Transport_Event_T *out_event
);
```

Use this API when:

- the host or rig needs to build a correctly numbered frame
- a received frame must be checked against current session state
- keep-alive timers need to be serviced
- state-change acknowledgement needs to be tracked
- a protocol-visible fault needs to be generated

The session layer should produce events rather than directly performing I/O.

## 16. Example Host-Side Flow

A host-side configuration flow may look like:

```text
initialise parser
initialise host transport session

host requests CONFIGURING
    session builds state-change frame
    frame encoder converts it to COBS-delimited bytes
    USB CDC transmits bytes

host receives bytes
    parser accumulates bytes until delimiter
    frame decoder validates frame
    session receives frame
    state-change ACK clears pending request

host sends configuration payload
    application layer encodes configuration packet
    session builds data frame
    frame encoder produces wire bytes
    USB CDC transmits bytes

host finishes configuration
    session builds FIN/control frame
    rig responds by entering ARMED or RUNNING

host sends execute request if rig is ARMED
    session builds state-change request to RUNNING
```

The host should not manually construct raw transport headers in application code. It should use the session and frame APIs.

## 17. Example Rig-Side Flow

A rig-side receive flow may look like:

```text
USB CDC receive callback places bytes into a stream/ring buffer

transport task or polling function reads bytes
    parser receives bytes
    parser detects a complete COBS frame
    frame decoder validates CRC and header
    session validates source, sequence, state, and flags

if payload is ready:
    application layer receives payload bytes
    application layer decodes packet
    firmware acts on decoded application command
```

The rig-side transport code should not run heavy decode logic inside a high-frequency ISR. USB receive callbacks or interrupts should only move bytes into a buffer. The parser and decoder should run in task/thread/main-loop context.

## 18. Buffer Ownership

The transport layer should avoid heap allocation.

APIs should use caller-provided buffers:

- caller provides output buffer for encoded frames
- caller provides scratch buffer for parser state
- caller provides payload buffer for decoded payloads
- caller provides session object storage

This is important for embedded use because it keeps memory ownership explicit and deterministic.

Typical ownership:

```text
HIL_Transport_Parser_T
    owned by firmware/host transport driver

parser scratch buffer
    owned by caller
    passed to parser init

encoded output buffer
    owned by caller
    passed to frame encoder

decoded payload buffer
    owned by caller
    passed to frame decoder or parser read function

HIL_Transport_Session_T
    owned by host or rig protocol manager
```

No core encode/decode path should call `malloc()`.

## 19. Error Handling

Transport functions should return explicit status codes.

Examples:

```c
typedef enum
{
    HIL_TRANSPORT_STATUS_OK = 0,
    HIL_TRANSPORT_STATUS_INVALID_ARGUMENT,
    HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL,
    HIL_TRANSPORT_STATUS_MALFORMED_FRAME,
    HIL_TRANSPORT_STATUS_BAD_CRC,
    HIL_TRANSPORT_STATUS_UNSUPPORTED_VERSION,
    HIL_TRANSPORT_STATUS_INVALID_STATE,
    HIL_TRANSPORT_STATUS_UNEXPECTED_SEQUENCE,
    HIL_TRANSPORT_STATUS_DUPLICATE_FRAME,
    HIL_TRANSPORT_STATUS_TIMEOUT,
    HIL_TRANSPORT_STATUS_NOT_READY,
    HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED,
    HIL_TRANSPORT_STATUS_INTERNAL_ERROR
} HIL_Transport_Status_T;
```

Errors should be handled at the correct layer:

- parser errors identify invalid byte-stream/framing conditions
- decoder errors identify invalid complete frames
- session errors identify invalid protocol conversation behaviour
- application errors identify invalid packet contents

For example:

```text
bad COBS block:
    parser/frame decode error

bad CRC:
    frame decode error

unexpected sequence number:
    session error

unknown application packet ID:
    application-layer error

invalid instruction channel:
    instruction-layer error
```

## 20. Testing Expectations

The transport layer should be tested before application-layer behaviour is added.

Recommended unit tests:

- CRC known-answer tests
- COBS encode/decode round trips
- frame encoded size calculation
- frame encode/decode round trips
- invalid argument handling
- buffer-too-small handling
- bad CRC rejection
- unsupported version rejection
- malformed header rejection
- parser partial-frame handling
- parser multiple-frame handling
- parser empty delimiter handling
- parser oversized-frame handling
- parser resynchronisation after noise
- session initialisation
- keep-alive event generation
- state-change request and acknowledgement
- duplicate sequence handling
- sequence gap handling

Recommended integration tests:

- application payload -> transport encode -> parser -> transport decode -> application payload
- bad CRC rejection
- truncated frame rejection
- random byte corruption
- dropped bytes
- inserted noise before valid frame
- partial frame delivery
- multiple frames in one receive buffer
- golden vector compatibility

Golden vectors should eventually define exact encoded bytes and decoded meanings for known-good and known-bad packets. These should be used by both C tests and future Python bindings.

## 21. Implementation Staging

The transport layer should be implemented progressively.

### Stage 1: Public Types and API Stubs

Define:

- status codes
- roles
- states
- frame types
- flags
- header type
- frame type
- parser type
- session type
- function declarations
- buildable stub definitions

No real protocol logic is required at this stage.

### Stage 2: CRC

Implement CRC32 and lock down:

- polynomial
- initial value
- final XOR
- reflection behaviour
- byte order
- golden vectors

### Stage 3: COBS

Implement or vendor a small COBS codec.

Requirements:

- no heap allocation
- caller-provided buffers
- deterministic runtime
- clear error return values
- unit tests for edge cases

### Stage 4: Frame Encode/Decode

Implement raw header encoding, payload copying, CRC insertion, COBS wrapping, and delimiter handling.

### Stage 5: Parser

Implement incremental parsing from arbitrary byte streams.

### Stage 6: Session State

Implement role/state tracking, sequence numbers, keep-alives, state-change acknowledgement, and timeout handling.

### Stage 7: Integration With Application Layer

Connect decoded transport payloads to application packet decoding.

Transport should still treat payload bytes as opaque.

### Stage 8: Golden Vectors

Create known-good and known-bad vectors that lock down the wire format.

## 22. Design Principles

The transport layer should follow these principles:

- define bytes on the wire explicitly
- keep hardware out of the protocol library
- keep application payloads opaque
- avoid heap allocation
- use caller-provided buffers
- avoid packed structs as the wire format
- use explicit endian encode/decode helpers
- separate streaming parser logic from complete-frame decode logic
- separate frame encode/decode from session state
- keep COBS internal to transport framing
- make CRC behaviour exact and testable
- introduce reliability features progressively
- preserve compatibility through golden vectors
- keep the implementation easy to consume from STM32 firmware and Python bindings

## 23. Summary

The transport layer is responsible for turning arbitrary application payload bytes into reliable, delimited, CRC-protected transport frames that can be sent over a byte stream such as USB CDC. It should own COBS framing, frame encode/decode, CRC validation, parsing, and transport-visible session behaviour.

It should not know what the application payload means. It should not contain firmware driver code. It should not contain high-level Python workflow logic.

The intended architecture is:

```text
USB CDC/UART bytes
    ↓
COBS parser
    ↓
transport frame decoder
    ↓
transport session validator
    ↓
application payload bytes
    ↓
application layer
```

and for transmission:

```text
application payload bytes
    ↓
transport session frame builder
    ↓
transport frame encoder
    ↓
COBS wire framing
    ↓
USB CDC/UART bytes
```

This separation keeps the protocol deterministic, testable, firmware-friendly, and suitable as the shared binary compatibility layer between the host API and the MCU firmware.
