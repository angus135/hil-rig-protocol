"""Operational contract tests for the public Python Transport facade."""

from __future__ import annotations

import queue
import threading

import pytest

from hil_rig_protocol import (
    EventType,
    Failure,
    LinkState,
    OperatingMode,
    ReceiveResult,
    Role,
    SessionState,
    Transport,
    TransportBindingError,
    TransportClosedError,
    TransportConfig,
    TransportEvent,
    TransportInternalError,
    TransportOwnershipError,
    TransportSnapshot,
    TransportStatus,
)
from hil_rig_protocol import _binding
import hil_rig_protocol.transport as transport_module

UINT32_MAX = (1 << 32) - 1


@pytest.fixture
def transport() -> Transport:
    value = Transport(Role.HOST, TransportConfig(session_seed=1))
    try:
        yield value
    finally:
        if not value.closed:
            value.close()


def _calls_for_invalid_precedence(transport: Transport) -> list[callable]:
    return [
        transport.reset,
        lambda: transport.notify_link_state(0, object()),  # type: ignore[arg-type]
        lambda: transport.submit_application_data(object()),  # type: ignore[arg-type]
        lambda: transport.receive_bytes(object()),  # type: ignore[arg-type]
        lambda: transport.process(object(), 0),  # type: ignore[arg-type]
        transport.peek_output,
        lambda: transport.commit_output(object()),  # type: ignore[arg-type]
        transport.read_application_data,
        transport.read_event,
        transport.get_status,
    ]


def test_closed_guard_precedes_argument_validation_for_all_operations() -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    transport.close()
    for operation in _calls_for_invalid_precedence(transport):
        with pytest.raises(TransportClosedError):
            operation()


def test_wrong_thread_guard_precedes_argument_validation_for_all_operations() -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    outcomes: queue.Queue[BaseException | None] = queue.Queue()

    def worker() -> None:
        for operation in _calls_for_invalid_precedence(transport):
            try:
                operation()
            except BaseException as error:
                outcomes.put(error)
            else:
                outcomes.put(None)

    thread = threading.Thread(target=worker)
    thread.start()
    thread.join()
    try:
        for _ in range(10):
            assert isinstance(outcomes.get_nowait(), TransportOwnershipError)
    finally:
        transport.close()


@pytest.mark.parametrize(
    ("method_name", "native_name", "extra_args"),
    [
        ("notify_link_state", "_native_notify_link_state", (LinkState.CONNECTED,)),
        ("process", "_native_process", (OperatingMode.NORMAL,)),
        ("commit_output", "_native_commit_output", ()),
    ],
)
@pytest.mark.parametrize("now_ms", [0, UINT32_MAX])
def test_time_boundaries_are_forwarded(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    method_name: str,
    native_name: str,
    extra_args: tuple[object, ...],
    now_ms: int,
) -> None:
    seen: list[int] = []

    if method_name == "notify_link_state":
        def fake(handle: object, state: int, value: int) -> int:
            seen.append(value)
            return int(TransportStatus.OK)
        args = (extra_args[0], now_ms)
    elif method_name == "process":
        def fake(handle: object, value: int, mode: int) -> int:
            seen.append(value)
            return int(TransportStatus.OK)
        args = (now_ms, extra_args[0])
    else:
        def fake(handle: object, value: int) -> int:
            seen.append(value)
            return int(TransportStatus.NOT_READY)
        args = (now_ms,)

    monkeypatch.setattr(transport_module, native_name, fake)
    getattr(transport, method_name)(*args)
    assert seen == [now_ms]


@pytest.mark.parametrize("invalid", [-1, UINT32_MAX + 1])
@pytest.mark.parametrize("method", ["notify", "process", "commit"])
def test_time_out_of_range_is_rejected(transport: Transport, method: str, invalid: int) -> None:
    if method == "notify":
        operation = lambda: transport.notify_link_state(LinkState.CONNECTED, invalid)
    elif method == "process":
        operation = lambda: transport.process(invalid, OperatingMode.NORMAL)
    else:
        operation = lambda: transport.commit_output(invalid)
    with pytest.raises(ValueError):
        operation()


@pytest.mark.parametrize("invalid", [True, False, 1.0, "1", object()])
@pytest.mark.parametrize("method", ["notify", "process", "commit"])
def test_time_wrong_types_are_rejected(transport: Transport, method: str, invalid: object) -> None:
    if method == "notify":
        operation = lambda: transport.notify_link_state(LinkState.CONNECTED, invalid)  # type: ignore[arg-type]
    elif method == "process":
        operation = lambda: transport.process(invalid, OperatingMode.NORMAL)  # type: ignore[arg-type]
    else:
        operation = lambda: transport.commit_output(invalid)  # type: ignore[arg-type]
    with pytest.raises(TypeError):
        operation()


@pytest.mark.parametrize("invalid", [0, 1, True, Role.HOST, OperatingMode.NORMAL])
def test_notify_link_state_requires_exact_enum(transport: Transport, invalid: object) -> None:
    with pytest.raises(TypeError):
        transport.notify_link_state(invalid, 0)  # type: ignore[arg-type]


@pytest.mark.parametrize("invalid", [0, 1, True, Role.HOST, LinkState.CONNECTED])
def test_process_requires_exact_operating_mode(transport: Transport, invalid: object) -> None:
    with pytest.raises(TypeError):
        transport.process(0, invalid)  # type: ignore[arg-type]


@pytest.mark.parametrize(
    ("method_name", "native_name", "allowed"),
    [
        ("reset", "_native_reset", [TransportStatus.OK]),
        (
            "notify_link_state",
            "_native_notify_link_state",
            [TransportStatus.OK, TransportStatus.CAPACITY_EXHAUSTED],
        ),
        (
            "submit_application_data",
            "_native_submit_application_data",
            [
                TransportStatus.OK,
                TransportStatus.NOT_READY,
                TransportStatus.MESSAGE_TOO_LARGE,
                TransportStatus.CAPACITY_EXHAUSTED,
            ],
        ),
        (
            "process",
            "_native_process",
            [
                TransportStatus.OK,
                TransportStatus.NOT_READY,
                TransportStatus.CAPACITY_EXHAUSTED,
                TransportStatus.DELIVERY_FAILED,
            ],
        ),
        (
            "commit_output",
            "_native_commit_output",
            [TransportStatus.OK, TransportStatus.NOT_READY],
        ),
    ],
)
def test_status_returning_methods_preserve_all_documented_statuses(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    method_name: str,
    native_name: str,
    allowed: list[TransportStatus],
) -> None:
    for expected in allowed:
        if method_name == "reset":
            fake = lambda handle, value=expected: int(value)
            args: tuple[object, ...] = ()
        elif method_name == "notify_link_state":
            fake = lambda handle, state, now, value=expected: int(value)
            args = (LinkState.CONNECTED, 0)
        elif method_name == "submit_application_data":
            fake = lambda handle, data, size, value=expected: int(value)
            args = (b"x",)
        elif method_name == "process":
            fake = lambda handle, now, mode, value=expected: int(value)
            args = (0, OperatingMode.NORMAL)
        else:
            fake = lambda handle, now, value=expected: int(value)
            args = (0,)
        monkeypatch.setattr(transport_module, native_name, fake)
        assert getattr(transport, method_name)(*args) is expected


@pytest.mark.parametrize(
    ("method_name", "native_name", "args"),
    [
        ("reset", "_native_reset", ()),
        ("notify_link_state", "_native_notify_link_state", (LinkState.CONNECTED, 0)),
        ("submit_application_data", "_native_submit_application_data", (b"x",)),
        ("process", "_native_process", (0, OperatingMode.NORMAL)),
        ("commit_output", "_native_commit_output", (0,)),
    ],
)
@pytest.mark.parametrize(
    ("returned", "expected_exception"),
    [
        (999, TransportBindingError),
        (TransportStatus.INVALID_ARGUMENT, TransportBindingError),
        (TransportStatus.INTERNAL_ERROR, TransportInternalError),
        (TransportStatus.TIMEOUT, TransportBindingError),
    ],
)
def test_status_returning_methods_reject_bad_native_statuses(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    method_name: str,
    native_name: str,
    args: tuple[object, ...],
    returned: int | TransportStatus,
    expected_exception: type[BaseException],
) -> None:
    if method_name == "reset":
        fake = lambda handle: int(returned)
    elif method_name == "notify_link_state":
        fake = lambda handle, state, now: int(returned)
    elif method_name == "submit_application_data":
        fake = lambda handle, data, size: int(returned)
    elif method_name == "process":
        fake = lambda handle, now, mode: int(returned)
    else:
        fake = lambda handle, now: int(returned)
    monkeypatch.setattr(transport_module, native_name, fake)
    with pytest.raises(expected_exception):
        getattr(transport, method_name)(*args)


@pytest.mark.parametrize(
    ("status", "consumed"),
    [
        (TransportStatus.OK, 4),
        (TransportStatus.NOT_READY, 0),
        (TransportStatus.CAPACITY_EXHAUSTED, 0),
        (TransportStatus.CAPACITY_EXHAUSTED, 2),
        (TransportStatus.CAPACITY_EXHAUSTED, 4),
    ],
)
def test_receive_preserves_valid_consumption_combinations(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    status: TransportStatus,
    consumed: int,
) -> None:
    def fake(handle: object, data: object, size: int, out: object) -> int:
        out[0] = consumed  # type: ignore[index]
        return int(status)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake)
    assert transport.receive_bytes(b"abcd") == ReceiveResult(status, consumed)


@pytest.mark.parametrize(
    ("status", "consumed"),
    [
        (TransportStatus.OK, 3),
        (TransportStatus.NOT_READY, 1),
        (TransportStatus.CAPACITY_EXHAUSTED, 5),
        (TransportStatus.INVALID_ARGUMENT, 0),
        (TransportStatus.TIMEOUT, 0),
        (999, 0),
    ],
)
def test_receive_rejects_invalid_status_consumption_results(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    status: int | TransportStatus,
    consumed: int,
) -> None:
    def fake(handle: object, data: object, size: int, out: object) -> int:
        out[0] = consumed  # type: ignore[index]
        return int(status)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake)
    with pytest.raises(TransportBindingError):
        transport.receive_bytes(b"abcd")


def test_receive_internal_error_preserves_valid_consumed_prefix(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    def fake(handle: object, data: object, size: int, out: object) -> int:
        out[0] = 2  # type: ignore[index]
        return int(TransportStatus.INTERNAL_ERROR)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake)
    with pytest.raises(TransportInternalError) as error:
        transport.receive_bytes(b"abcd")
    assert error.value.status is TransportStatus.INTERNAL_ERROR
    assert error.value.bytes_consumed == 2


def test_receive_internal_error_with_invalid_consumption_is_binding_error(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    def fake(handle: object, data: object, size: int, out: object) -> int:
        out[0] = 5  # type: ignore[index]
        return int(TransportStatus.INTERNAL_ERROR)

    monkeypatch.setattr(transport_module, "_native_receive_bytes", fake)
    with pytest.raises(TransportBindingError):
        transport.receive_bytes(b"abcd")


def _fake_peek_sequence(responses: list[tuple[int | TransportStatus, int, bytes]]) -> callable:
    calls = 0

    def fake(handle: object, output: object, capacity: int, size_ptr: object) -> int:
        nonlocal calls
        status, reported_size, payload = responses[calls]
        calls += 1
        size_ptr[0] = reported_size  # type: ignore[index]
        if output != _binding.ffi.NULL and payload:
            _binding.ffi.memmove(output, payload, min(len(payload), capacity))
        return int(status)

    return fake


def test_peek_output_none_requires_zero_size(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence([(TransportStatus.NOT_READY, 0, b"")]),
    )
    assert transport.peek_output() is None


def test_peek_output_two_call_copy_is_immutable(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.OK, 3, b"abc"),
            ]
        ),
    )
    value = transport.peek_output()
    assert value == b"abc"
    assert type(value) is bytes


def test_repeated_peek_calls_native_again_and_returns_stable_bytes(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.OK, 3, b"abc"),
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.OK, 3, b"abc"),
            ]
        ),
    )
    assert transport.peek_output() == b"abc"
    assert transport.peek_output() == b"abc"


@pytest.mark.parametrize(
    "responses",
    [
        [(TransportStatus.NOT_READY, 1, b"")],
        [(TransportStatus.BUFFER_TOO_SMALL, 0, b"")],
        [(TransportStatus.OK, 0, b"")],
        [(TransportStatus.TIMEOUT, 0, b"")],
        [(999, 0, b"")],
    ],
)
def test_peek_output_rejects_invalid_query_results(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    responses: list[tuple[int | TransportStatus, int, bytes]],
) -> None:
    monkeypatch.setattr(transport_module, "_native_peek_output", _fake_peek_sequence(responses))
    with pytest.raises(TransportBindingError):
        transport.peek_output()


def test_peek_output_rejects_internal_error(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence([(TransportStatus.INTERNAL_ERROR, 0, b"")]),
    )
    with pytest.raises(TransportInternalError):
        transport.peek_output()


def test_peek_output_rejects_size_above_configured_limit(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence(
            [(TransportStatus.BUFFER_TOO_SMALL, transport.config.max_encoded_frame_size + 1, b"")]
        ),
    )
    with pytest.raises(TransportBindingError):
        transport.peek_output()


@pytest.mark.parametrize(
    "second_status",
    [TransportStatus.NOT_READY, TransportStatus.BUFFER_TOO_SMALL, TransportStatus.TIMEOUT, 999],
)
def test_peek_output_rejects_inconsistent_second_status(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    second_status: int | TransportStatus,
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (second_status, 3, b"abc"),
            ]
        ),
    )
    with pytest.raises(TransportBindingError):
        transport.peek_output()


def test_peek_output_second_internal_error_remains_internal(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.INTERNAL_ERROR, 0, b""),
            ]
        ),
    )
    with pytest.raises(TransportInternalError):
        transport.peek_output()


def test_peek_output_rejects_changed_second_size(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_peek_output",
        _fake_peek_sequence(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.OK, 2, b"ab"),
            ]
        ),
    )
    with pytest.raises(TransportBindingError):
        transport.peek_output()


def _fake_application_read(
    responses: list[tuple[int | TransportStatus, int, bytes]],
    *,
    require_nonnull_zero_capacity: bool = False,
) -> callable:
    calls = 0

    def fake(handle: object, output: object, capacity: int, size_ptr: object) -> int:
        nonlocal calls
        status, reported_size, payload = responses[calls]
        calls += 1
        size_ptr[0] = reported_size  # type: ignore[index]
        if calls > 1 and require_nonnull_zero_capacity:
            assert output != _binding.ffi.NULL
            assert capacity == 0
        if output != _binding.ffi.NULL and payload:
            _binding.ffi.memmove(output, payload, min(len(payload), capacity))
        return int(status)

    return fake


def test_read_application_data_none(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read([(TransportStatus.NOT_READY, 0, b"")]),
    )
    assert transport.read_application_data() is None


def test_read_application_data_complete_immutable_copy(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.OK, 3, b"abc"),
            ]
        ),
    )
    value = transport.read_application_data()
    assert value == b"abc"
    assert type(value) is bytes


def test_read_application_data_empty_pending_uses_nonnull_scratch(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 0, b""),
                (TransportStatus.OK, 0, b""),
            ],
            require_nonnull_zero_capacity=True,
        ),
    )
    assert transport.read_application_data() == b""


def test_read_application_data_initial_ok_zero_returns_empty_without_second_call(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    calls = 0

    def fake(handle: object, output: object, capacity: int, size_ptr: object) -> int:
        nonlocal calls
        calls += 1
        size_ptr[0] = 0  # type: ignore[index]
        return int(TransportStatus.OK)

    monkeypatch.setattr(transport_module, "_native_read_application_data", fake)
    assert transport.read_application_data() == b""
    assert calls == 1


@pytest.mark.parametrize(
    "responses",
    [
        [(TransportStatus.NOT_READY, 1, b"")],
        [(TransportStatus.OK, 1, b"")],
        [(TransportStatus.TIMEOUT, 0, b"")],
        [(999, 0, b"")],
    ],
)
def test_read_application_data_rejects_invalid_query_results(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    responses: list[tuple[int | TransportStatus, int, bytes]],
) -> None:
    monkeypatch.setattr(
        transport_module, "_native_read_application_data", _fake_application_read(responses)
    )
    with pytest.raises(TransportBindingError):
        transport.read_application_data()


def test_read_application_data_rejects_oversized_query(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read(
            [
                (
                    TransportStatus.BUFFER_TOO_SMALL,
                    transport.config.max_application_message_size + 1,
                    b"",
                )
            ]
        ),
    )
    with pytest.raises(TransportBindingError):
        transport.read_application_data()


@pytest.mark.parametrize(
    "second_status",
    [TransportStatus.NOT_READY, TransportStatus.BUFFER_TOO_SMALL, TransportStatus.TIMEOUT, 999],
)
def test_read_application_data_rejects_inconsistent_second_status(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    second_status: int | TransportStatus,
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (second_status, 3, b"abc"),
            ]
        ),
    )
    with pytest.raises(TransportBindingError):
        transport.read_application_data()


def test_read_application_data_second_internal_error_remains_internal(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.INTERNAL_ERROR, 0, b""),
            ]
        ),
    )
    with pytest.raises(TransportInternalError):
        transport.read_application_data()


def test_read_application_data_rejects_changed_second_size(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_application_data",
        _fake_application_read(
            [
                (TransportStatus.BUFFER_TOO_SMALL, 3, b""),
                (TransportStatus.OK, 2, b"ab"),
            ]
        ),
    )
    with pytest.raises(TransportBindingError):
        transport.read_application_data()


def _event_fake(
    native_status: int | TransportStatus,
    *,
    event_type: int = int(EventType.LINK_STATE_CHANGED),
    event_status: int = int(TransportStatus.OK),
    failure: int = int(Failure.NONE),
    required_capacity: int = 0,
) -> callable:
    def fake(handle: object, event: object) -> int:
        event.type = event_type
        event.status = event_status
        event.failure = failure
        event.required_capacity = required_capacity
        return int(native_status)

    return fake


def test_read_event_none(monkeypatch: pytest.MonkeyPatch, transport: Transport) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_event",
        _event_fake(TransportStatus.NOT_READY),
    )
    assert transport.read_event() is None


def test_read_event_converts_all_fields_and_internal_event_status_is_data(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_event",
        _event_fake(
            TransportStatus.OK,
            event_type=int(EventType.PROTOCOL_ERROR),
            event_status=int(TransportStatus.INTERNAL_ERROR),
            failure=int(Failure.PROTOCOL),
            required_capacity=123,
        ),
    )
    assert transport.read_event() == TransportEvent(
        EventType.PROTOCOL_ERROR,
        TransportStatus.INTERNAL_ERROR,
        Failure.PROTOCOL,
        123,
    )


def test_read_event_rejects_none_sentinel(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_read_event",
        _event_fake(TransportStatus.OK, event_type=int(EventType.NONE)),
    )
    with pytest.raises(TransportBindingError):
        transport.read_event()


@pytest.mark.parametrize("field", ["type", "status", "failure"])
def test_read_event_rejects_unknown_enum_values(
    monkeypatch: pytest.MonkeyPatch, transport: Transport, field: str
) -> None:
    kwargs = {
        "event_type": int(EventType.LINK_STATE_CHANGED),
        "event_status": int(TransportStatus.OK),
        "failure": int(Failure.NONE),
    }
    kwargs[{"type": "event_type", "status": "event_status", "failure": "failure"}[field]] = 999
    monkeypatch.setattr(
        transport_module,
        "_native_read_event",
        _event_fake(TransportStatus.OK, **kwargs),
    )
    with pytest.raises(TransportBindingError):
        transport.read_event()


@pytest.mark.parametrize(
    ("status", "exception"),
    [
        (TransportStatus.INVALID_ARGUMENT, TransportBindingError),
        (TransportStatus.INTERNAL_ERROR, TransportInternalError),
        (TransportStatus.BUFFER_TOO_SMALL, TransportBindingError),
        (999, TransportBindingError),
    ],
)
def test_read_event_rejects_invalid_operation_status(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    status: int | TransportStatus,
    exception: type[BaseException],
) -> None:
    monkeypatch.setattr(transport_module, "_native_read_event", _event_fake(status))
    with pytest.raises(exception):
        transport.read_event()


def _snapshot_fake(
    native_status: int | TransportStatus = TransportStatus.OK,
    **overrides: int,
) -> callable:
    values = {
        "role": int(Role.HOST),
        "link_state": int(LinkState.CONNECTED),
        "session_state": int(SessionState.ESTABLISHED),
        "operating_mode": int(OperatingMode.NORMAL),
        "operating_mode_valid": 1,
        "output_pending": 1,
        "application_message_pending": 0,
        "event_pending": 1,
        "reliable_delivery_pending": 0,
        "last_failure": int(Failure.NONE),
    }
    values.update(overrides)

    def fake(handle: object, snapshot: object) -> int:
        for name, value in values.items():
            setattr(snapshot, name, value)
        return int(native_status)

    return fake


def test_get_status_converts_snapshot(monkeypatch: pytest.MonkeyPatch, transport: Transport) -> None:
    monkeypatch.setattr(transport_module, "_native_get_status", _snapshot_fake())
    assert transport.get_status() == TransportSnapshot(
        role=Role.HOST,
        link_state=LinkState.CONNECTED,
        session_state=SessionState.ESTABLISHED,
        operating_mode=OperatingMode.NORMAL,
        output_pending=True,
        application_message_pending=False,
        event_pending=True,
        reliable_delivery_pending=False,
        last_failure=Failure.NONE,
    )


def test_get_status_operating_mode_none_does_not_convert_mode(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_get_status",
        _snapshot_fake(operating_mode_valid=0, operating_mode=999),
    )
    assert transport.get_status().operating_mode is None


@pytest.mark.parametrize(
    "field",
    [
        "operating_mode_valid",
        "output_pending",
        "application_message_pending",
        "event_pending",
        "reliable_delivery_pending",
    ],
)
def test_get_status_rejects_non_boolean_native_flags(
    monkeypatch: pytest.MonkeyPatch, transport: Transport, field: str
) -> None:
    monkeypatch.setattr(transport_module, "_native_get_status", _snapshot_fake(**{field: 2}))
    with pytest.raises(TransportBindingError):
        transport.get_status()


@pytest.mark.parametrize("field", ["role", "link_state", "session_state", "operating_mode", "last_failure"])
def test_get_status_rejects_unknown_enum_values(
    monkeypatch: pytest.MonkeyPatch, transport: Transport, field: str
) -> None:
    monkeypatch.setattr(transport_module, "_native_get_status", _snapshot_fake(**{field: 999}))
    with pytest.raises(TransportBindingError):
        transport.get_status()


@pytest.mark.parametrize(
    ("status", "exception"),
    [
        (TransportStatus.INVALID_ARGUMENT, TransportBindingError),
        (TransportStatus.INTERNAL_ERROR, TransportInternalError),
        (TransportStatus.NOT_READY, TransportBindingError),
        (999, TransportBindingError),
    ],
)
def test_get_status_rejects_invalid_operation_status(
    monkeypatch: pytest.MonkeyPatch,
    transport: Transport,
    status: int | TransportStatus,
    exception: type[BaseException],
) -> None:
    monkeypatch.setattr(transport_module, "_native_get_status", _snapshot_fake(status))
    with pytest.raises(exception):
        transport.get_status()


def test_non_receive_internal_errors_have_no_consumption(
    monkeypatch: pytest.MonkeyPatch, transport: Transport
) -> None:
    monkeypatch.setattr(
        transport_module,
        "_native_reset",
        lambda handle: int(TransportStatus.INTERNAL_ERROR),
    )
    with pytest.raises(TransportInternalError) as error:
        transport.reset()
    assert error.value.bytes_consumed is None


def test_real_native_initial_and_caller_driven_host_flow() -> None:
    with Transport(Role.HOST, TransportConfig(max_application_message_size=8, session_seed=1)) as transport:
        initial = transport.get_status()
        assert initial.role is Role.HOST
        assert initial.link_state is LinkState.DISCONNECTED
        assert initial.session_state is SessionState.DISCONNECTED
        assert initial.operating_mode is None

        assert transport.receive_bytes(b"") == ReceiveResult(TransportStatus.NOT_READY, 0)
        assert transport.commit_output(0) is TransportStatus.NOT_READY
        assert transport.submit_application_data(b"x") is TransportStatus.NOT_READY
        assert transport.submit_application_data(b"x" * 9) is TransportStatus.NOT_READY

        assert transport.notify_link_state(LinkState.CONNECTED, 0) is TransportStatus.OK
        assert transport.read_event() == TransportEvent(
            EventType.LINK_STATE_CHANGED,
            TransportStatus.OK,
            Failure.NONE,
            0,
        )
        assert transport.read_event() is None

        assert transport.process(1, OperatingMode.NORMAL) is TransportStatus.OK
        first = transport.peek_output()
        assert first is not None
        assert len(first) > 0
        assert transport.peek_output() == first
        assert transport.commit_output(2) is TransportStatus.OK
        assert transport.commit_output(2) is TransportStatus.NOT_READY

        assert transport.reset() is TransportStatus.OK
        after_reset = transport.get_status()
        assert after_reset.role is Role.HOST
        assert after_reset.link_state is LinkState.CONNECTED
        assert after_reset.session_state is SessionState.RECOVERING
        assert after_reset.last_failure is Failure.LOCAL_RESET
