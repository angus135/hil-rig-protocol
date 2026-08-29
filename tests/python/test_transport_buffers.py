"""Buffer-protocol behavior for the public Transport facade."""

from __future__ import annotations

from array import array

import pytest

from hil_rig_protocol import ReceiveResult, Role, Transport, TransportConfig, TransportStatus
from hil_rig_protocol import _binding
import hil_rig_protocol.transport as transport_module


@pytest.fixture
def transport() -> Transport:
    value = Transport(Role.HOST, TransportConfig(session_seed=1))
    try:
        yield value
    finally:
        value.close()


@pytest.mark.parametrize(
    "data",
    [
        b"abc",
        bytearray(b"abc"),
        memoryview(b"abc"),
        memoryview(bytearray(b"abc")),
    ],
)
def test_submit_accepts_common_contiguous_buffers(
    monkeypatch: pytest.MonkeyPatch, transport: Transport, data: object
) -> None:
    seen: list[bytes] = []

    def fake_submit(handle: object, pointer: object, size: int) -> int:
        assert handle is not None
        seen.append(bytes(_binding.ffi.buffer(pointer, size)))
        return int(TransportStatus.NOT_READY)

    monkeypatch.setattr(transport_module, "_native_submit_application_data", fake_submit)
    assert transport.submit_application_data(data) is TransportStatus.NOT_READY  # type: ignore[arg-type]
    assert seen == [b"abc"]
    if isinstance(data, memoryview):
        data.release()


@pytest.mark.parametrize(
    "data",
    [
        b"abc",
        bytearray(b"abc"),
        memoryview(b"abc"),
        memoryview(bytearray(b"abc")),
    ],
)
def test_receive_accepts_common_contiguous_buffers(
    monkeypatch: pytest.MonkeyPatch, transport: Transport, data: object
) -> None:
    seen: list[bytes] = []

    def fake_receive(
        handle: object, pointer: object, size: int, consumed: object
    ) -> int:
        assert handle is not None
        seen.append(bytes(_binding.ffi.buffer(pointer, size)))
        consumed[0] = size  # type: ignore[index]
        return int(TransportStatus.OK)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake_receive)
    assert transport.receive_bytes(data) == ReceiveResult(TransportStatus.OK, 3)  # type: ignore[arg-type]
    assert seen == [b"abc"]
    if isinstance(data, memoryview):
        data.release()


def test_multibyte_view_uses_nbytes_not_len(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    values = array("I", [0x01020304, 0x05060708])
    view = memoryview(values)
    assert len(view) == 2
    assert view.nbytes > len(view)
    seen_sizes: list[int] = []

    def fake_receive(
        handle: object, pointer: object, size: int, consumed: object
    ) -> int:
        seen_sizes.append(size)
        consumed[0] = size  # type: ignore[index]
        return int(TransportStatus.OK)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake_receive)
    result = transport.receive_bytes(view)
    assert result.bytes_consumed == view.nbytes
    assert seen_sizes == [view.nbytes]
    view.release()


@pytest.mark.parametrize("method_name", ["submit_application_data", "receive_bytes"])
def test_non_contiguous_buffers_are_rejected(
    transport: Transport, method_name: str
) -> None:
    view = memoryview(bytearray(b"abcdef"))[::2]
    try:
        with pytest.raises(BufferError, match="C-contiguous"):
            getattr(transport, method_name)(view)
    finally:
        view.release()


@pytest.mark.parametrize("method_name", ["submit_application_data", "receive_bytes"])
def test_non_buffer_inputs_are_rejected(transport: Transport, method_name: str) -> None:
    with pytest.raises(TypeError, match="buffer protocol"):
        getattr(transport, method_name)(object())


def test_submit_releases_bytearray_export_immediately(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    data = bytearray(b"abc")
    monkeypatch.setattr(
        transport_module,
        "_native_submit_application_data",
        lambda handle, pointer, size: int(TransportStatus.NOT_READY),
    )
    transport.submit_application_data(data)
    data.extend(b"d")
    assert data == bytearray(b"abcd")


def test_receive_releases_bytearray_export_immediately(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    data = bytearray(b"abc")

    def fake_receive(
        handle: object, pointer: object, size: int, consumed: object
    ) -> int:
        consumed[0] = size  # type: ignore[index]
        return int(TransportStatus.OK)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake_receive)
    transport.receive_bytes(data)
    data.extend(b"d")
    assert data == bytearray(b"abcd")


def test_buffer_exports_are_released_when_native_status_mapping_raises(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    data = bytearray(b"abc")
    monkeypatch.setattr(
        transport_module,
        "_native_submit_application_data",
        lambda handle, pointer, size: 999,
    )
    with pytest.raises(Exception):
        transport.submit_application_data(data)
    data.extend(b"d")
    assert data == bytearray(b"abcd")


def test_empty_application_submission_is_rejected_before_native_call(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    called = False

    def fake_submit(handle: object, pointer: object, size: int) -> int:
        nonlocal called
        called = True
        return int(TransportStatus.OK)

    monkeypatch.setattr(transport_module, "_native_submit_application_data", fake_submit)
    with pytest.raises(ValueError, match="empty Application"):
        transport.submit_application_data(b"")
    assert not called


def test_zero_byte_receive_calls_native_with_null_pointer(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    calls = 0

    def fake_receive(
        handle: object, pointer: object, size: int, consumed: object
    ) -> int:
        nonlocal calls
        calls += 1
        assert pointer == _binding.ffi.NULL
        assert size == 0
        consumed[0] = 0  # type: ignore[index]
        return int(TransportStatus.OK)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake_receive)
    assert transport.receive_bytes(b"") == ReceiveResult(TransportStatus.OK, 0)
    assert calls == 1
