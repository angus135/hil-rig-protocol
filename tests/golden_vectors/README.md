# Golden-vector fixtures

The executable source of truth is
`tests/c/integration/wire/test_transport_golden_vectors.cpp`. It drives only the
public Transport facade and compares complete delimiter-terminated output with
literal bytes. The canonical handshake uses session
`0x0807060504030201`, HOST initial sequence `0xFFFE`, and RIG initial sequence
`0x5678`. This makes the CONFIRM sequence `0xFFFF` and the first HOST
Application sequence wrap naturally to zero.

All multibyte semantic fields below are encoded little-endian before CRC and
COBS processing. CRC is CRC-32/ISO-HDLC over the decoded header and payload.

| Version | Frame | Session | Sequence | ACK | Payload | CRC | Complete encoded bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0x01` | INITIATE | `0x0807060504030201` | `0xFFFE` | `0x0000` | empty | `0x3A544BCB` | `0D 01 01 01 02 03 04 05 06 07 08 FE FF 01 05 CB 4B 54 3A 00` |
| `0x01` | RESPONSE | `0x0807060504030201` | `0x5678` | `0xFFFE` | empty | `0x3E388BB3` | `13 01 02 01 02 03 04 05 06 07 08 78 56 FE FF B3 8B 38 3E 00` |
| `0x01` | CONFIRM | `0x0807060504030201` | `0xFFFF` | `0x5678` | empty | `0x875A9EDA` | `13 01 03 01 02 03 04 05 06 07 08 FF FF 78 56 DA 9E 5A 87 00` |
| `0x01` | ACK | `0x0807060504030201` | `0x0000` | `0xFFFF` | empty | `0x9CEA66DB` | `0B 01 05 01 02 03 04 05 06 07 08 01 07 FF FF DB 66 EA 9C 00` |
| `0x01` | APPLICATION | `0x0807060504030201` | `0x0000` | `0x0000` | `11 00 22` | `0x7F681C57` | `0B 01 04 01 02 03 04 05 06 07 08 01 01 01 02 11 06 22 57 1C 68 7F 00` |
| `0x01` | RESET | `0x0807060504030201` | `0x0000` | `0x0000` | empty | `0x9F0618EA` | `0B 01 06 01 02 03 04 05 06 07 08 01 01 01 05 EA 18 06 9F 00` |

The integration suite also feeds the handshake and Application literals back
through public `Receive_Bytes()`, verifies the resulting ACK and RESET behavior,
and checks that changing one integrity-covered encoded byte without updating
the CRC cannot deliver the Application payload.
