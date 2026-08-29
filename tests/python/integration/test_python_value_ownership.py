"""Integration tests for Python-owned copies returned from native Transport."""

from __future__ import annotations

from hil_rig_protocol import TransportStatus

from .parity_helpers import H2R, R2H, deliver_item, establish, make_pair, take_output


def _receive_host_message(pair, payload: bytes, now_ms: int) -> bytes:
    assert pair.host.submit_application_data(payload) is TransportStatus.OK
    application = take_output(pair, H2R, now_ms=now_ms)
    assert deliver_item(pair, application) is TransportStatus.OK
    received = pair.rig.read_application_data()
    assert received == payload
    assert received is not None

    acknowledgement = take_output(pair, R2H, now_ms=now_ms + 1)
    assert deliver_item(pair, acknowledgement) is TransportStatus.OK
    assert pair.host.read_event() is not None
    return received


def test_application_result_is_detached_from_native_lifetime() -> None:
    first_payload = b"first-python-owned-application"
    second_payload = b"second-native-buffer-reuse"

    pair = make_pair()
    try:
        establish(pair)
        first = _receive_host_message(pair, first_payload, 300)
        assert isinstance(first, bytes)

        second = _receive_host_message(pair, second_payload, 310)
        assert second == second_payload
        assert first == first_payload

        assert pair.rig.reset() is TransportStatus.OK
        assert first == first_payload
    finally:
        pair.close()

    # The Python bytes remain valid after their native owner has been destroyed.
    assert first == first_payload


def test_peeked_output_is_detached_from_native_lifetime() -> None:
    pair = make_pair()
    saved_output: bytes | None = None
    original = b""
    try:
        assert pair.connect() == (TransportStatus.OK, TransportStatus.OK)
        assert pair.host.read_event() is not None
        assert pair.rig.read_event() is not None
        assert pair.process_host() is TransportStatus.OK

        saved_output = pair.host.peek_output()
        assert saved_output is not None
        original = bytes(saved_output)

        # transfer_one_output() re-peeks, commits, and delivers this same native item.
        transfer = pair.transfer_one_output(H2R)
        assert transfer.accept.status is TransportStatus.OK
        assert saved_output == original

        pair.establish_clean_session()
        assert saved_output == original

        assert pair.host.submit_application_data(b"later-native-output") is TransportStatus.OK
        later = pair.host.peek_output()
        assert later is not None
        assert later != saved_output
        assert saved_output == original

        assert pair.host.reset() is TransportStatus.OK
        assert saved_output == original
    finally:
        pair.close()

    assert saved_output == original
