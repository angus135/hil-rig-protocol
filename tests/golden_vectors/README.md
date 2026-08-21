# Golden-vector fixtures

This directory is reserved for language-neutral fixtures that can be consumed
by C, firmware, and future Python implementations. No completed fixture set is
present yet; this README does not make the golden-vector work complete.

Future fixtures should cover valid Transport frames for every MVP frame type,
COBS and CRC boundaries, malformed/integrity-invalid inputs, size limits and
sequence wrap, clean establishment (`INITIATE`, `RESPONSE`, `CONFIRM`, final
ACK), Application delivery, and RESET/recovery cases. Once the Application wire
format exists, fixtures must also cover each Application message family,
version rejection, validation failures, and boundary sizes.
