"""Python-specific real-native buffer protocol integration coverage."""

from __future__ import annotations

from array import array

import pytest
from hil_rig_protocol import EventType, TransportStatus

from .parity_helpers import H2R, R2H, deliver_item, establish, make_pair, take_output


def _complete_submitted_host_delivery(pair, expected: bytes) -> None:
    application = take_output(pair, H2R, now_ms=100)
    assert deliver_item(pair, application) is TransportStatus.OK
    assert pair.rig.read_application_data() == expected

    acknowledgement = take_output(pair, R2H, now_ms=101)
    assert deliver_item(pair, acknowledgement) is TransportStatus.OK
    event = pair.host.read_event()
    assert event is not None
    assert event.type is EventType.DELIVERY_CONFIRMED


def _buffer_for_kind(kind: str, payload: bytes):
    if kind == "bytes":
        return payload, None
    if kind == "bytearray":
        return bytearray(payload), None
    if kind == "readonly-memoryview":
        view = memoryview(payload)
        return view, view
    if kind == "writable-memoryview":
        view = memoryview(bytearray(payload))
        return view, view
    raise AssertionError(f"unexpected buffer kind {kind}")


@pytest.mark.parametrize(
    "buffer_kind",
    ["bytes", "bytearray", "readonly-memoryview", "writable-memoryview"],
)
def test_application_submission_buffer_types_end_to_end(buffer_kind: str) -> None:
    payload = b"python-buffer-protocol-\x00\x7f\xff"
    data, view = _buffer_for_kind(buffer_kind, payload)
    try:
        with make_pair() as pair:
            establish(pair)
            assert pair.host.submit_application_data(data) is TransportStatus.OK
            _complete_submitted_host_delivery(pair, payload)
    finally:
        if view is not None:
            view.release()


def test_multibyte_memoryview_submission_uses_nbytes_end_to_end() -> None:
    storage = array("I", [0x01020304, 0xA0B0C0D0, 0x11223344])
    view = memoryview(storage)
    byte_view = view.cast("B")
    expected = byte_view.tobytes()
    try:
        assert len(view) == 3
        assert view.nbytes == len(expected)
        assert view.nbytes != len(view)

        with make_pair() as pair:
            establish(pair)
            assert pair.host.submit_application_data(view) is TransportStatus.OK
            _complete_submitted_host_delivery(pair, expected)
    finally:
        byte_view.release()
        view.release()


@pytest.mark.parametrize("buffer_kind", ["bytearray", "writable-memoryview"])
def test_mutable_submission_export_is_released_and_native_owns_data(
    buffer_kind: str,
) -> None:
    original = b"mutable-python-input"
    backing = bytearray(original)
    view = memoryview(backing) if buffer_kind == "writable-memoryview" else None
    data = view if view is not None else backing

    with make_pair() as pair:
        establish(pair)
        assert pair.host.submit_application_data(data) is TransportStatus.OK

        if view is not None:
            view.release()

        # Resizing would raise BufferError if CFFI retained an export after the call.
        backing.extend(b"-resized")
        backing[:] = b"x" * len(backing)

        _complete_submitted_host_delivery(pair, original)


@pytest.mark.parametrize(
    "buffer_kind",
    ["bytes", "bytearray", "readonly-memoryview", "writable-memoryview"],
)
def test_receive_buffer_types_and_export_release_end_to_end(buffer_kind: str) -> None:
    payload = b"receive-buffer-protocol-\x00\xfe"

    with make_pair() as pair:
        establish(pair)
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        encoded = take_output(pair, H2R, now_ms=200).data

        if buffer_kind == "bytes":
            backing = None
            view = None
            offered = encoded
        elif buffer_kind == "bytearray":
            backing = bytearray(encoded)
            view = None
            offered = backing
        elif buffer_kind == "readonly-memoryview":
            backing = None
            view = memoryview(encoded)
            offered = view
        elif buffer_kind == "writable-memoryview":
            backing = bytearray(encoded)
            view = memoryview(backing)
            offered = view
        else:
            raise AssertionError(f"unexpected buffer kind {buffer_kind}")

        receive = pair.rig.receive_bytes(offered)
        assert receive.status is TransportStatus.OK
        assert receive.bytes_consumed == len(encoded)

        if view is not None:
            view.release()
        if backing is not None:
            # Proves the CFFI export ended with receive_bytes().
            backing.extend(b"-now-resizable")
            backing[:] = b"z" * len(backing)

        assert pair.rig.read_application_data() == payload
        acknowledgement = take_output(pair, R2H, now_ms=201)
        assert deliver_item(pair, acknowledgement) is TransportStatus.OK
        event = pair.host.read_event()
        assert event is not None
        assert event.type is EventType.DELIVERY_CONFIRMED


def test_nonzero_offset_contiguous_memoryview_submission_end_to_end() -> None:
    payload = b"offset-application-payload-\x00\xfe"
    prefix = b"guard-before:"
    suffix = b":guard-after"
    backing = bytearray(prefix + payload + suffix)
    start = len(prefix)
    view = memoryview(backing)[start : start + len(payload)]

    with make_pair() as pair:
        establish(pair)
        assert view.c_contiguous
        assert view.nbytes == len(payload)
        assert pair.host.submit_application_data(view) is TransportStatus.OK

        view.release()
        backing.extend(b"-resized")
        backing[:] = b"x" * len(backing)

        _complete_submitted_host_delivery(pair, payload)


def test_nonzero_offset_contiguous_memoryview_receive_end_to_end() -> None:
    payload = b"offset-receive-payload-\x00\xfd"

    with make_pair() as pair:
        establish(pair)
        assert pair.host.submit_application_data(payload) is TransportStatus.OK
        encoded = take_output(pair, H2R, now_ms=300).data

        prefix = b"guard-before:"
        suffix = b":guard-after"
        backing = bytearray(prefix + encoded + suffix)
        start = len(prefix)
        view = memoryview(backing)[start : start + len(encoded)]

        assert view.c_contiguous
        assert view.nbytes == len(encoded)
        receive = pair.rig.receive_bytes(view)
        assert receive.status is TransportStatus.OK
        assert receive.bytes_consumed == len(encoded)

        view.release()
        backing.extend(b"-resized")
        backing[:] = b"z" * len(backing)

        assert pair.rig.read_application_data() == payload
        acknowledgement = take_output(pair, R2H, now_ms=301)
        assert deliver_item(pair, acknowledgement) is TransportStatus.OK
        event = pair.host.read_event()
        assert event is not None
        assert event.type is EventType.DELIVERY_CONFIRMED


def test_mutable_receive_partial_consumption_releases_export_and_preserves_suffix() -> None:
    first_payload = b"first-unread-message"
    second_payload = b"second-retained-message"
    harmless_suffix = b"\x00\x00"

    with make_pair() as pair:
        establish(pair)

        # Deliver and acknowledge the first message, but deliberately leave the
        # complete Application message unread at the RIG.
        assert pair.host.submit_application_data(first_payload) is TransportStatus.OK
        first_frame = take_output(pair, H2R, now_ms=400)
        assert deliver_item(pair, first_frame) is TransportStatus.OK
        first_ack = take_output(pair, R2H, now_ms=401)
        assert deliver_item(pair, first_ack) is TransportStatus.OK
        first_confirmation = pair.host.read_event()
        assert first_confirmation is not None
        assert first_confirmation.type is EventType.DELIVERY_CONFIRMED
        assert pair.rig.get_status().application_message_pending

        assert pair.host.submit_application_data(second_payload) is TransportStatus.OK
        second_frame = take_output(pair, H2R, now_ms=402)
        backing = bytearray(second_frame.data + harmless_suffix)
        view = memoryview(backing)

        blocked = pair.rig.receive_bytes(view)
        assert blocked.status is TransportStatus.CAPACITY_EXHAUSTED
        assert blocked.bytes_consumed == len(second_frame.data)
        assert 0 < blocked.bytes_consumed < view.nbytes
        caller_owned_suffix = bytes(view[blocked.bytes_consumed :])
        assert caller_owned_suffix == harmless_suffix

        view.release()
        # Resizing immediately after receive proves that CFFI released the
        # mutable export even though native Transport retained completed work.
        backing.extend(b"-resized-after-partial-consume")
        backing[:] = b"m" * len(backing)

        assert pair.rig.read_application_data() == first_payload

        resumed = pair.rig.receive_bytes(b"")
        assert resumed.status is TransportStatus.OK
        assert resumed.bytes_consumed == 0
        assert pair.rig.read_application_data() == second_payload

        suffix_result = pair.rig.receive_bytes(caller_owned_suffix)
        assert suffix_result.status is TransportStatus.OK
        assert suffix_result.bytes_consumed == len(caller_owned_suffix)

        second_ack = take_output(pair, R2H, now_ms=403)
        assert deliver_item(pair, second_ack) is TransportStatus.OK
        confirmations = []
        while True:
            event = pair.host.read_event()
            if event is None:
                break
            confirmations.append(event)
        assert [event.type for event in confirmations].count(EventType.DELIVERY_CONFIRMED) == 1
