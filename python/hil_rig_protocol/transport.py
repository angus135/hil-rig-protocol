"""Public Transport construction, configuration resolution, and lifetime ownership."""

from __future__ import annotations

import secrets
import threading
from typing import Any

from . import _binding
from .errors import (
    TransportBindingError,
    TransportClosedError,
    TransportConfigurationError,
    TransportCreationError,
    TransportInternalError,
    TransportOwnershipError,
)
from .transport_types import Role, TransportConfig, TransportStatus

_UINT64_MAX = (1 << 64) - 1


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


class Transport:
    """Own one native Transport context and its binding-private workspace.

    The creating thread is the single owner for the native lifetime. ``close``
    provides deterministic cleanup, context-manager exit closes automatically,
    and CFFI garbage-collection cleanup remains a fallback when a caller fails
    to close explicitly. Operational Transport methods are intentionally not
    part of this PR.
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

    def __enter__(self) -> Transport:
        self._require_open_owner()
        return self

    def __exit__(self, exc_type: object, exc_value: object, traceback: object) -> bool:
        self.close()
        return False

    def __copy__(self) -> None:
        raise TypeError("Transport instances cannot be copied")

    def __deepcopy__(self, memo: object) -> None:
        raise TypeError("Transport instances cannot be deep-copied")

    def __reduce__(self) -> None:
        raise TypeError("Transport instances cannot be pickled")

    def __reduce_ex__(self, protocol: int) -> None:
        raise TypeError("Transport instances cannot be pickled")


__all__ = ["Transport"]
