"""Tests for Transport configuration resolution and native lifetime ownership."""

from __future__ import annotations

import copy
import gc
import pickle
import queue
import threading
import weakref

import pytest

from hil_rig_protocol import (
    Role,
    Transport,
    TransportBindingError,
    TransportClosedError,
    TransportConfig,
    TransportConfigurationError,
    TransportCreationError,
    TransportInternalError,
    TransportOwnershipError,
    TransportStatus,
)
from hil_rig_protocol import _binding
from hil_rig_protocol import transport as transport_module

UINT64_MAX = (1 << 64) - 1


def test_host_generated_seed_is_secure_range_and_visible(monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[int] = []

    def fake_randbelow(upper_bound: int) -> int:
        calls.append(upper_bound)
        return 0x12345677

    monkeypatch.setattr(transport_module.secrets, "randbelow", fake_randbelow)
    transport = Transport(Role.HOST, TransportConfig())
    try:
        assert calls == [UINT64_MAX - 1]
        assert transport.config.session_seed == 0x12345678
        assert 1 <= transport.config.session_seed <= UINT64_MAX - 1
    finally:
        transport.close()


@pytest.mark.parametrize("seed", [1, 0x123456789ABCDEF0, UINT64_MAX - 1])
def test_host_explicit_seed_is_preserved(seed: int) -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=seed))
    try:
        assert transport.config.session_seed == seed
    finally:
        transport.close()


def test_transport_retains_effective_immutable_configuration() -> None:
    requested = TransportConfig(
        max_application_message_size=128,
        max_encoded_frame_size=256,
        session_seed=7,
        initial_reliable_sequence=0x1234,
        retransmit_timeout_ms=25,
        max_retries=3,
    )
    transport = Transport(Role.HOST, requested)
    try:
        assert transport.config == requested
        assert transport.config is not requested
    finally:
        transport.close()


@pytest.mark.parametrize("seed", [0, UINT64_MAX])
def test_host_rejects_reserved_seed_before_native_create(
    seed: int, monkeypatch: pytest.MonkeyPatch
) -> None:
    def unexpected_create(*args: object) -> tuple[int, int, object]:
        raise AssertionError("native create must not be called")

    monkeypatch.setattr(transport_module, "_call_native_create", unexpected_create)
    with pytest.raises(TransportConfigurationError) as error:
        Transport(Role.HOST, TransportConfig(session_seed=seed))
    assert error.value.status is None


@pytest.mark.parametrize("seed", [None, 0])
def test_rig_none_and_zero_resolve_to_zero(seed: int | None) -> None:
    transport = Transport(Role.RIG, TransportConfig(session_seed=seed))
    try:
        assert transport.config.session_seed == 0
    finally:
        transport.close()


@pytest.mark.parametrize("seed", [1, 2, UINT64_MAX])
def test_rig_rejects_nonzero_seed_before_native_create(
    seed: int, monkeypatch: pytest.MonkeyPatch
) -> None:
    def unexpected_create(*args: object) -> tuple[int, int, object]:
        raise AssertionError("native create must not be called")

    monkeypatch.setattr(transport_module, "_call_native_create", unexpected_create)
    with pytest.raises(TransportConfigurationError) as error:
        Transport(Role.RIG, TransportConfig(session_seed=seed))
    assert error.value.status is None


def test_unsupported_native_configuration_preserves_status() -> None:
    with pytest.raises(TransportConfigurationError) as error:
        Transport(
            Role.HOST,
            TransportConfig(session_seed=1, connection_timeout_ms=1),
        )
    assert error.value.status is TransportStatus.UNSUPPORTED_CONFIGURATION


def test_transport_requires_public_role_and_config_types() -> None:
    with pytest.raises(TypeError):
        Transport(0, TransportConfig())  # type: ignore[arg-type]
    with pytest.raises(TypeError):
        Transport(Role.HOST, object())  # type: ignore[arg-type]


def test_native_create_is_called_exactly_once(monkeypatch: pytest.MonkeyPatch) -> None:
    original = transport_module._call_native_create
    calls = 0

    def counting_create(role: Role, native_config: object) -> tuple[int, int, object]:
        nonlocal calls
        calls += 1
        return original(role, native_config)

    monkeypatch.setattr(transport_module, "_call_native_create", counting_create)
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    transport.close()
    assert calls == 1


def test_creation_result_mapping() -> None:
    lib = _binding.lib
    ok = lib.HIL_PY_ADAPTER_STATUS_OK
    invalid = lib.HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT
    allocation = lib.HIL_PY_ADAPTER_STATUS_ALLOCATION_FAILED
    native_error = lib.HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR

    transport_module._interpret_creation_result(ok, int(TransportStatus.OK), True)

    with pytest.raises(MemoryError):
        transport_module._interpret_creation_result(allocation, int(TransportStatus.OK), False)

    for status in (TransportStatus.INVALID_ARGUMENT, TransportStatus.UNSUPPORTED_CONFIGURATION):
        with pytest.raises(TransportConfigurationError) as error:
            transport_module._interpret_creation_result(native_error, int(status), False)
        assert error.value.status is status

    with pytest.raises(TransportInternalError) as error:
        transport_module._interpret_creation_result(
            native_error, int(TransportStatus.INTERNAL_ERROR), False
        )
    assert error.value.status is TransportStatus.INTERNAL_ERROR

    with pytest.raises(TransportCreationError) as error:
        transport_module._interpret_creation_result(
            native_error, int(TransportStatus.BUFFER_TOO_SMALL), False
        )
    assert error.value.status is TransportStatus.BUFFER_TOO_SMALL

    inconsistent = [
        (invalid, int(TransportStatus.INVALID_ARGUMENT), False),
        (ok, int(TransportStatus.OK), False),
        (native_error, int(TransportStatus.INVALID_ARGUMENT), True),
        (native_error, int(TransportStatus.OK), False),
        (allocation, int(TransportStatus.INVALID_ARGUMENT), False),
        (999, int(TransportStatus.OK), False),
        (native_error, 999, False),
    ]
    for adapter_status, core_status, handle_published in inconsistent:
        with pytest.raises(TransportBindingError):
            transport_module._interpret_creation_result(
                adapter_status, core_status, handle_published
            )


def test_inconsistent_published_handle_is_destroyed(monkeypatch: pytest.MonkeyPatch) -> None:
    fake_handle = _binding.ffi.cast("HIL_Python_Transport_T *", 1)
    destroyed: list[object] = []

    def fake_create(role: Role, native_config: object) -> tuple[int, int, object]:
        return (
            _binding.lib.HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR,
            int(TransportStatus.INVALID_ARGUMENT),
            fake_handle,
        )

    monkeypatch.setattr(transport_module, "_call_native_create", fake_create)
    monkeypatch.setattr(transport_module, "_destroy_native", destroyed.append)

    with pytest.raises(TransportBindingError):
        transport_module._create_native_handle(
            Role.HOST,
            TransportConfig(session_seed=1),
        )
    assert destroyed == [fake_handle]


def _count_destruction(monkeypatch: pytest.MonkeyPatch) -> list[object]:
    original_destroy = transport_module._destroy_native
    destroyed: list[object] = []

    def counting_destroy(handle: object) -> None:
        destroyed.append(handle)
        original_destroy(handle)

    monkeypatch.setattr(transport_module, "_destroy_native", counting_destroy)
    return destroyed


def test_host_and_rig_construct_and_close() -> None:
    host = Transport(Role.HOST, TransportConfig(session_seed=1))
    rig = Transport(Role.RIG, TransportConfig())
    assert host.role is Role.HOST
    assert rig.role is Role.RIG
    assert not host.closed
    assert not rig.closed
    host.close()
    rig.close()
    assert host.closed
    assert rig.closed


def test_repeated_close_destroys_exactly_once(monkeypatch: pytest.MonkeyPatch) -> None:
    destroyed = _count_destruction(monkeypatch)
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    transport.close()
    transport.close()
    assert transport.closed
    assert len(destroyed) == 1


def test_context_exit_destroys_exactly_once(monkeypatch: pytest.MonkeyPatch) -> None:
    destroyed = _count_destruction(monkeypatch)
    with Transport(Role.HOST, TransportConfig(session_seed=1)) as transport:
        assert not transport.closed
    assert transport.closed
    assert len(destroyed) == 1


def test_exceptional_context_exit_does_not_suppress_and_destroys_once(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    destroyed = _count_destruction(monkeypatch)
    transport: Transport | None = None
    with pytest.raises(RuntimeError, match="sentinel"):
        with Transport(Role.HOST, TransportConfig(session_seed=1)) as transport:
            raise RuntimeError("sentinel")
    assert transport is not None
    assert transport.closed
    assert len(destroyed) == 1


def test_gc_fallback_destroys_exactly_once(monkeypatch: pytest.MonkeyPatch) -> None:
    destroyed = _count_destruction(monkeypatch)
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    reference = weakref.ref(transport)
    del transport
    gc.collect()
    assert reference() is None
    assert len(destroyed) == 1


def test_explicit_close_then_gc_does_not_destroy_twice(monkeypatch: pytest.MonkeyPatch) -> None:
    destroyed = _count_destruction(monkeypatch)
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    reference = weakref.ref(transport)
    transport.close()
    assert len(destroyed) == 1
    del transport
    gc.collect()
    assert reference() is None
    assert len(destroyed) == 1


def test_use_after_close_raises_transport_closed_error() -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    transport.close()
    with pytest.raises(TransportClosedError):
        transport._require_open_owner()


def test_wrong_thread_rejects_live_use_and_release() -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    outcomes: queue.Queue[BaseException | None] = queue.Queue()

    def worker() -> None:
        for operation in (transport._require_open_owner, transport.close):
            try:
                operation()
            except BaseException as error:
                outcomes.put(error)
            else:
                outcomes.put(None)

    thread = threading.Thread(target=worker)
    thread.start()
    thread.join()

    first = outcomes.get_nowait()
    second = outcomes.get_nowait()
    assert isinstance(first, TransportOwnershipError)
    assert isinstance(second, TransportOwnershipError)
    assert not transport.closed
    transport.close()


def test_recycled_thread_identifier_does_not_grant_ownership(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    created: queue.Queue[tuple[Transport, threading.Thread, int]] = queue.Queue()

    def owner_worker() -> None:
        created.put(
            (
                Transport(Role.HOST, TransportConfig(session_seed=1)),
                threading.current_thread(),
                threading.get_ident(),
            )
        )

    owner = threading.Thread(target=owner_worker)
    owner.start()
    owner.join()
    transport, owner_thread, owner_ident = created.get_nowait()
    assert owner_thread is owner

    replacement = threading.Thread()
    replacement._ident = owner_ident
    monkeypatch.setattr(threading, "get_ident", lambda: owner_ident)
    monkeypatch.setattr(threading, "current_thread", lambda: replacement)

    with pytest.raises(TransportOwnershipError):
        transport._require_open_owner()
    with pytest.raises(TransportOwnershipError):
        transport.close()
    assert not transport.closed


def test_copy_deepcopy_and_pickle_are_rejected() -> None:
    transport = Transport(Role.HOST, TransportConfig(session_seed=1))
    try:
        with pytest.raises(TypeError):
            copy.copy(transport)
        with pytest.raises(TypeError):
            copy.deepcopy(transport)
        with pytest.raises(TypeError):
            pickle.dumps(transport)
    finally:
        transport.close()
