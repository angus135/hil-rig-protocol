"""Public Transport construction, lifetime, and caller-driven protocol facade."""

from __future__ import annotations

import secrets
import threading
from collections.abc import Buffer, Iterator
from contextlib import contextmanager
from enum import IntEnum
from typing import Any, Literal, NoReturn, SupportsIndex, TypeVar

from . import _binding
from .errors import (
    TransportBindingError,
    TransportClosedError,
    TransportConfigurationError,
    TransportCreationError,
    TransportInternalError,
    TransportOwnershipError,
)
from .transport_types import (
    EventType,
    Failure,
    LinkState,
    OperatingMode,
    ReceiveResult,
    Role,
    SessionState,
    TransportConfig,
    TransportEvent,
    TransportSnapshot,
    TransportStatus,
)

_UINT64_MAX = (1 << 64) - 1
_UINT32_MAX = (1 << 32) - 1

_EnumT = TypeVar("_EnumT", bound=IntEnum)


def _resolve_effective_config(role: Role, config: TransportConfig) -> TransportConfig:
    seed = config.session_seed

    if role is Role.HOST:
        if seed is None:
            seed = secrets.randbelow(_UINT64_MAX - 1) + 1
        elif seed == 0 or seed == _UINT64_MAX:
            raise TransportConfigurationError(
                "HOST session_seed must be between 1 and UINT64_MAX - 1"
            )
    else:
        if seed is None:
            seed = 0
        elif seed != 0:
            raise TransportConfigurationError("RIG session_seed must be zero")

    return TransportConfig(
        max_application_message_size=config.max_application_message_size,
        max_encoded_frame_size=config.max_encoded_frame_size,
        session_seed=seed,
        initial_reliable_sequence=config.initial_reliable_sequence,
        connection_timeout_ms=config.connection_timeout_ms,
        retransmit_timeout_ms=config.retransmit_timeout_ms,
        max_retries=config.max_retries,
    )


def _build_native_config(config: TransportConfig) -> Any:
    native_config = _binding.ffi.new("HIL_Transport_Config_T *")
    native_config.max_application_message_size = config.max_application_message_size
    native_config.max_encoded_frame_size = config.max_encoded_frame_size
    if config.session_seed is None:
        raise TransportBindingError("effective Transport configuration has no session seed")
    native_config.session_seed = config.session_seed
    native_config.initial_reliable_sequence = config.initial_reliable_sequence
    native_config.connection_timeout_ms = config.connection_timeout_ms
    native_config.retransmit_timeout_ms = config.retransmit_timeout_ms
    native_config.max_retries = config.max_retries
    return native_config


def _call_native_create(role: Role, native_config: Any) -> tuple[int, int, Any]:
    out_transport = _binding.ffi.new("HIL_Python_Transport_T **")
    out_status = _binding.ffi.new("HIL_Transport_Status_T *")
    adapter_status = _binding.lib.HIL_PY_TRANSPORT_Create(
        int(role), native_config, out_transport, out_status
    )
    return int(adapter_status), int(out_status[0]), out_transport[0]


def _known_transport_status(value: int) -> TransportStatus | None:
    try:
        return TransportStatus(value)
    except ValueError:
        return None


def _interpret_creation_result(
    adapter_status: int, core_status_value: int, handle_published: bool
) -> None:
    """Validate and map one adapter creation result.

    This helper is deliberately side-effect free so result/invariant mapping can
    be unit tested without adding controls to the native ABI. Cleanup of an
    unexpectedly published handle is owned by ``_create_native_handle``.
    """

    core_status = _known_transport_status(core_status_value)
    adapter_ok = _binding.lib.HIL_PY_ADAPTER_STATUS_OK
    adapter_invalid = _binding.lib.HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT
    adapter_allocation = _binding.lib.HIL_PY_ADAPTER_STATUS_ALLOCATION_FAILED
    adapter_transport = _binding.lib.HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR
    known_adapter_results = {
        adapter_ok,
        adapter_invalid,
        adapter_allocation,
        adapter_transport,
    }

    if adapter_status not in known_adapter_results:
        raise TransportBindingError(
            f"native adapter returned unknown creation result {adapter_status}",
            status=core_status,
        )

    if core_status is None:
        raise TransportBindingError(
            f"native adapter returned unknown Transport status {core_status_value}"
        )

    success = adapter_status == adapter_ok and core_status is TransportStatus.OK
    if handle_published != success:
        raise TransportBindingError(
            "native adapter returned an inconsistent handle/result combination",
            status=core_status,
        )

    if success:
        return

    if adapter_status == adapter_invalid:
        raise TransportBindingError(
            "native adapter rejected binding-owned creation arguments",
            status=core_status,
        )

    if adapter_status == adapter_allocation:
        if core_status is not TransportStatus.OK:
            raise TransportBindingError(
                "native adapter allocation failure carried a non-OK core status",
                status=core_status,
            )
        raise MemoryError("native Transport adapter allocation failed")

    if adapter_status != adapter_transport or core_status is TransportStatus.OK:
        raise TransportBindingError(
            "native adapter returned an inconsistent creation result",
            status=core_status,
        )

    if core_status in {
        TransportStatus.INVALID_ARGUMENT,
        TransportStatus.UNSUPPORTED_CONFIGURATION,
    }:
        raise TransportConfigurationError(
            f"native Transport rejected the configuration: {core_status.name}",
            status=core_status,
        )
    if core_status is TransportStatus.INTERNAL_ERROR:
        raise TransportInternalError(
            "native Transport reported an internal error during creation",
            status=core_status,
        )

    raise TransportCreationError(
        f"native Transport creation failed: {core_status.name}",
        status=core_status,
    )


_destroy_native = _binding.lib.HIL_PY_TRANSPORT_Destroy
_native_reset = _binding.lib.HIL_PY_TRANSPORT_Reset
_native_notify_link_state = _binding.lib.HIL_PY_TRANSPORT_Notify_Link_State
_native_submit_application_data = _binding.lib.HIL_PY_TRANSPORT_Submit_Application_Data
_native_receive_bytes = _binding.lib.HIL_PY_TRANSPORT_Receive_Bytes
_native_process = _binding.lib.HIL_PY_TRANSPORT_Process
_native_peek_output = _binding.lib.HIL_PY_TRANSPORT_Peek_Output
_native_commit_output = _binding.lib.HIL_PY_TRANSPORT_Commit_Output
_native_read_application_data = _binding.lib.HIL_PY_TRANSPORT_Read_Application_Data
_native_read_event = _binding.lib.HIL_PY_TRANSPORT_Read_Event
_native_get_status = _binding.lib.HIL_PY_TRANSPORT_Get_Status


def _create_native_handle(role: Role, config: TransportConfig) -> Any:
    native_config = _build_native_config(config)
    adapter_status, core_status, handle = _call_native_create(role, native_config)
    handle_published = handle != _binding.ffi.NULL
    try:
        _interpret_creation_result(adapter_status, core_status, handle_published)
    except BaseException:
        if handle_published:
            _destroy_native(handle)
        raise
    return handle


def _register_native_lifetime(handle: Any) -> Any:
    try:
        return _binding.ffi.gc(handle, _destroy_native)
    except BaseException:
        _destroy_native(handle)
        raise


def _release_native_lifetime(handle: Any) -> None:
    _binding.ffi.release(handle)


def _validate_now_ms(now_ms: object) -> None:
    if type(now_ms) is not int:
        raise TypeError("now_ms must be an int")
    if now_ms < 0 or now_ms > _UINT32_MAX:
        raise ValueError(f"now_ms must be in the range 0..{_UINT32_MAX}")


def _operation_status(
    value: int,
    operation: str,
    allowed: frozenset[TransportStatus],
    *,
    bytes_consumed: int | None = None,
) -> TransportStatus:
    """Convert a native operation result while enforcing its documented status set."""

    status = _known_transport_status(int(value))
    if status is None:
        raise TransportBindingError(f"{operation} returned unknown Transport status {int(value)}")
    if status is TransportStatus.INTERNAL_ERROR:
        raise TransportInternalError(
            f"native Transport reported an internal error during {operation}",
            status=status,
            bytes_consumed=bytes_consumed,
        )
    if status is TransportStatus.INVALID_ARGUMENT:
        raise TransportBindingError(
            f"{operation} returned INVALID_ARGUMENT after Python-side validation",
            status=status,
        )
    if status not in allowed:
        raise TransportBindingError(
            f"{operation} returned undocumented status {status.name}",
            status=status,
        )
    return status


def _enum_value(enum_type: type[_EnumT], value: int, field_name: str) -> _EnumT:
    try:
        return enum_type(int(value))
    except ValueError as error:
        raise TransportBindingError(
            f"native {field_name} contains unknown value {int(value)}"
        ) from error


def _native_bool(value: int, field_name: str) -> bool:
    native_value = int(value)
    if native_value not in (0, 1):
        raise TransportBindingError(
            f"native {field_name} must be exactly zero or one, got {native_value}"
        )
    return bool(native_value)


@contextmanager
def _borrow_buffer(data: Buffer) -> Iterator[tuple[Any, int]]:
    """Borrow one C-contiguous Python buffer as raw const bytes for one native call."""

    try:
        view = memoryview(data)
    except TypeError as error:
        raise TypeError("data must support the buffer protocol") from error

    cdata = None
    try:
        if not view.c_contiguous:
            raise BufferError("data must be C-contiguous")

        size = view.nbytes
        if size == 0:
            yield _binding.ffi.NULL, 0
            return

        cdata = _binding.ffi.from_buffer("const uint8_t[]", view, require_writable=False)
        yield cdata, size
    finally:
        if cdata is not None:
            _binding.ffi.release(cdata)
        view.release()


class Transport:
    """Own and service one native Transport context.

    Every operation is caller-driven and must run on the creating thread. The
    caller supplies monotonic millisecond time, external I/O, retry policy for
    unconsumed receive suffixes, event draining, and complete-write tracking.
    Output remains pinned until the caller explicitly commits it after complete
    external acceptance. Application payloads are exchanged as complete opaque
    byte strings; Transport delivery does not imply Application acceptance.
    """

    __slots__ = ("_role", "_config", "_owner_thread", "__native_handle", "__weakref__")

    def __init__(self, role: Role, config: TransportConfig) -> None:
        if type(role) is not Role:
            raise TypeError("role must be a Role")
        if type(config) is not TransportConfig:
            raise TypeError("config must be a TransportConfig")

        owner_thread = threading.current_thread()
        effective_config = _resolve_effective_config(role, config)
        raw_handle = _create_native_handle(role, effective_config)
        managed_handle = _register_native_lifetime(raw_handle)

        self._role = role
        self._config = effective_config
        self._owner_thread = owner_thread
        self.__native_handle = managed_handle

    @property
    def role(self) -> Role:
        """Role fixed for this Transport lifetime."""

        return self._role

    @property
    def config(self) -> TransportConfig:
        """Effective immutable configuration used to create the native context."""

        return self._config

    @property
    def closed(self) -> bool:
        """Whether deterministic native cleanup has already occurred."""

        return self.__native_handle is None

    def close(self) -> None:
        """Release the native context exactly once.

        Repeated calls after closure are no-ops. A live Transport can only be
        released by the thread that created it.
        """

        if self.__native_handle is None:
            return
        self._require_owner()
        handle = self.__native_handle
        _release_native_lifetime(handle)
        self.__native_handle = None

    def _require_owner(self) -> None:
        if threading.current_thread() is not self._owner_thread:
            raise TransportOwnershipError(
                "Transport native lifetime may only be used by its creating thread"
            )

    def _require_open_owner(self) -> Any:
        """Return the private native handle after reusable lifetime/owner checks."""

        if self.__native_handle is None:
            raise TransportClosedError("Transport is closed")
        self._require_owner()
        return self.__native_handle

    def reset(self) -> TransportStatus:
        """Abandon session-scoped work without closing this Transport."""

        handle = self._require_open_owner()
        return _operation_status(
            _native_reset(handle),
            "reset",
            frozenset({TransportStatus.OK}),
        )

    def notify_link_state(self, link_state: LinkState, now_ms: int) -> TransportStatus:
        """Report caller-owned physical-link availability at ``now_ms``."""

        handle = self._require_open_owner()
        if type(link_state) is not LinkState:
            raise TypeError("link_state must be a LinkState")
        _validate_now_ms(now_ms)
        return _operation_status(
            _native_notify_link_state(handle, int(link_state), now_ms),
            "notify_link_state",
            frozenset({TransportStatus.OK, TransportStatus.CAPACITY_EXHAUSTED}),
        )

    def submit_application_data(self, data: Buffer) -> TransportStatus:
        """Submit one complete opaque Application message.

        Mutable input is borrowed only during this call and must not be mutated
        concurrently. Session readiness remains authoritative before native size
        validation, so oversized pre-establishment input still returns
        :attr:`TransportStatus.NOT_READY`.
        """

        handle = self._require_open_owner()
        with _borrow_buffer(data) as (native_data, data_size):
            if data_size == 0:
                raise ValueError("empty Application messages are not supported by the MVP")
            return _operation_status(
                _native_submit_application_data(handle, native_data, data_size),
                "submit_application_data",
                frozenset(
                    {
                        TransportStatus.OK,
                        TransportStatus.NOT_READY,
                        TransportStatus.MESSAGE_TOO_LARGE,
                        TransportStatus.CAPACITY_EXHAUSTED,
                    }
                ),
            )

    def receive_bytes(self, data: Buffer) -> ReceiveResult:
        """Offer one arbitrary byte-stream chunk and report its accepted prefix.

        A zero-byte input is forwarded to native Transport because it can resume
        a retained completed frame. On partial capacity exhaustion, the caller
        retries only the unconsumed suffix; this method never retries internally.
        Mutable input is borrowed only for the duration of the native call.
        """

        handle = self._require_open_owner()
        with _borrow_buffer(data) as (native_data, data_size):
            consumed_ptr = _binding.ffi.new("size_t *")
            native_status = _native_receive_bytes(handle, native_data, data_size, consumed_ptr)
            bytes_consumed = int(consumed_ptr[0])

        if bytes_consumed > data_size:
            raise TransportBindingError(
                "receive_bytes reported consumption beyond the offered buffer"
            )

        status = _operation_status(
            native_status,
            "receive_bytes",
            frozenset(
                {
                    TransportStatus.OK,
                    TransportStatus.NOT_READY,
                    TransportStatus.CAPACITY_EXHAUSTED,
                }
            ),
            bytes_consumed=bytes_consumed,
        )
        if status is TransportStatus.OK and bytes_consumed != data_size:
            raise TransportBindingError(
                "receive_bytes returned OK without consuming the complete offered buffer",
                status=status,
            )
        if status is TransportStatus.NOT_READY and bytes_consumed != 0:
            raise TransportBindingError(
                "receive_bytes returned NOT_READY with nonzero consumption",
                status=status,
            )
        return ReceiveResult(status=status, bytes_consumed=bytes_consumed)

    def process(self, now_ms: int, operating_mode: OperatingMode) -> TransportStatus:
        """Advance caller-driven Transport work once."""

        handle = self._require_open_owner()
        _validate_now_ms(now_ms)
        if type(operating_mode) is not OperatingMode:
            raise TypeError("operating_mode must be an OperatingMode")
        return _operation_status(
            _native_process(handle, now_ms, int(operating_mode)),
            "process",
            frozenset(
                {
                    TransportStatus.OK,
                    TransportStatus.NOT_READY,
                    TransportStatus.CAPACITY_EXHAUSTED,
                    TransportStatus.DELIVERY_FAILED,
                }
            ),
        )

    def peek_output(self) -> bytes | None:
        """Copy the complete currently selected encoded output without committing it."""

        handle = self._require_open_owner()
        required_ptr = _binding.ffi.new("size_t *")
        query_status = _operation_status(
            _native_peek_output(handle, _binding.ffi.NULL, 0, required_ptr),
            "peek_output query",
            frozenset({TransportStatus.NOT_READY, TransportStatus.BUFFER_TOO_SMALL}),
        )
        required_size = int(required_ptr[0])

        if query_status is TransportStatus.NOT_READY:
            if required_size != 0:
                raise TransportBindingError(
                    "peek_output returned NOT_READY with a nonzero required size",
                    status=query_status,
                )
            return None

        if required_size == 0:
            raise TransportBindingError(
                "peek_output reported BUFFER_TOO_SMALL with a zero required size",
                status=query_status,
            )
        if required_size > self._config.max_encoded_frame_size:
            raise TransportBindingError(
                "peek_output required size exceeds the configured encoded-frame limit",
                status=query_status,
            )

        output = _binding.ffi.new("uint8_t[]", required_size)
        output_size_ptr = _binding.ffi.new("size_t *")
        status = _operation_status(
            _native_peek_output(handle, output, required_size, output_size_ptr),
            "peek_output copy",
            frozenset({TransportStatus.OK}),
        )
        output_size = int(output_size_ptr[0])
        if output_size != required_size:
            raise TransportBindingError(
                "peek_output size changed between query and copy",
                status=status,
            )
        return bytes(_binding.ffi.buffer(output, output_size))

    def commit_output(self, now_ms: int) -> TransportStatus:
        """Commit the last complete peek only after external I/O accepted every byte."""

        handle = self._require_open_owner()
        _validate_now_ms(now_ms)
        return _operation_status(
            _native_commit_output(handle, now_ms),
            "commit_output",
            frozenset({TransportStatus.OK, TransportStatus.NOT_READY}),
        )

    def read_application_data(self) -> bytes | None:
        """Read and consume one complete opaque received Application message."""

        handle = self._require_open_owner()
        required_ptr = _binding.ffi.new("size_t *")
        query_status = _operation_status(
            _native_read_application_data(handle, _binding.ffi.NULL, 0, required_ptr),
            "read_application_data query",
            frozenset(
                {
                    TransportStatus.OK,
                    TransportStatus.NOT_READY,
                    TransportStatus.BUFFER_TOO_SMALL,
                }
            ),
        )
        required_size = int(required_ptr[0])

        if query_status is TransportStatus.NOT_READY:
            if required_size != 0:
                raise TransportBindingError(
                    "read_application_data returned NOT_READY with a nonzero size",
                    status=query_status,
                )
            return None

        if query_status is TransportStatus.OK:
            if required_size != 0:
                raise TransportBindingError(
                    "read_application_data query returned OK with a nonzero size",
                    status=query_status,
                )
            return b""

        if required_size > self._config.max_application_message_size:
            raise TransportBindingError(
                "read_application_data required size exceeds the configured message limit",
                status=query_status,
            )

        if required_size == 0:
            output = _binding.ffi.new("uint8_t[1]")
        else:
            output = _binding.ffi.new("uint8_t[]", required_size)
        output_size_ptr = _binding.ffi.new("size_t *")
        status = _operation_status(
            _native_read_application_data(handle, output, required_size, output_size_ptr),
            "read_application_data copy",
            frozenset({TransportStatus.OK}),
        )
        output_size = int(output_size_ptr[0])
        if output_size != required_size:
            raise TransportBindingError(
                "read_application_data size changed between query and consume",
                status=status,
            )
        if output_size == 0:
            return b""
        return bytes(_binding.ffi.buffer(output, output_size))

    def read_event(self) -> TransportEvent | None:
        """Read and consume one oldest high-level Transport event."""

        handle = self._require_open_owner()
        native_event = _binding.ffi.new("HIL_Transport_Event_T *")
        status = _operation_status(
            _native_read_event(handle, native_event),
            "read_event",
            frozenset({TransportStatus.OK, TransportStatus.NOT_READY}),
        )
        if status is TransportStatus.NOT_READY:
            return None

        event_type = _enum_value(EventType, native_event.type, "event type")
        if event_type is EventType.NONE:
            raise TransportBindingError("native read_event returned the NONE sentinel")
        event_status = _enum_value(TransportStatus, native_event.status, "event status")
        failure = _enum_value(Failure, native_event.failure, "event failure")
        return TransportEvent(
            type=event_type,
            status=event_status,
            failure=failure,
            required_capacity=int(native_event.required_capacity),
        )

    def get_status(self) -> TransportSnapshot:
        """Return one consistent high-level native Transport status snapshot."""

        handle = self._require_open_owner()
        native_status = _binding.ffi.new("HIL_Transport_Status_Snapshot_T *")
        _operation_status(
            _native_get_status(handle, native_status),
            "get_status",
            frozenset({TransportStatus.OK}),
        )

        operating_mode_valid = _native_bool(
            native_status.operating_mode_valid, "operating_mode_valid"
        )
        operating_mode = (
            _enum_value(
                OperatingMode,
                native_status.operating_mode,
                "snapshot operating_mode",
            )
            if operating_mode_valid
            else None
        )
        return TransportSnapshot(
            role=_enum_value(Role, native_status.role, "snapshot role"),
            link_state=_enum_value(LinkState, native_status.link_state, "snapshot link_state"),
            session_state=_enum_value(
                SessionState,
                native_status.session_state,
                "snapshot session_state",
            ),
            operating_mode=operating_mode,
            output_pending=_native_bool(native_status.output_pending, "output_pending"),
            application_message_pending=_native_bool(
                native_status.application_message_pending,
                "application_message_pending",
            ),
            event_pending=_native_bool(native_status.event_pending, "event_pending"),
            reliable_delivery_pending=_native_bool(
                native_status.reliable_delivery_pending,
                "reliable_delivery_pending",
            ),
            last_failure=_enum_value(Failure, native_status.last_failure, "snapshot last_failure"),
        )

    def __enter__(self) -> Transport:
        self._require_open_owner()
        return self

    def __exit__(self, exc_type: object, exc_value: object, traceback: object) -> Literal[False]:
        self.close()
        return False

    def __copy__(self) -> None:
        raise TypeError("Transport instances cannot be copied")

    def __deepcopy__(self, memo: object) -> None:
        raise TypeError("Transport instances cannot be deep-copied")

    def __reduce__(self) -> NoReturn:
        raise TypeError("Transport instances cannot be pickled")

    def __reduce_ex__(self, protocol: SupportsIndex) -> NoReturn:
        raise TypeError("Transport instances cannot be pickled")


__all__ = ["Transport"]
