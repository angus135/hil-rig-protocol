"""Stateless, direction-neutral Application codec backed exclusively by native C."""

from __future__ import annotations

import threading
from collections.abc import Buffer, Iterator
from contextlib import contextmanager
from typing import Any, NoReturn, SupportsIndex

from . import _binding
from ._application_conversion import (
    _build_native_config,
    _read_test_configuration,
    _read_test_instruction,
    _read_test_result,
    _write_test_configuration,
    _write_test_instruction,
    _write_test_result,
)
from .application_types import (
    ApplicationConfig,
    ApplicationMessage,
    ApplicationStatus,
    TestConfiguration,
    TestId,
    TestInstruction,
    TestResult,
)
from .errors import (
    ApplicationBindingError,
    ApplicationConfigurationError,
    ApplicationDecodeError,
    ApplicationEncodeError,
    ApplicationError,
    ApplicationInternalError,
    ApplicationOwnershipError,
)

_native_init = _binding.lib.HIL_APPLICATION_Init
_native_encoded_size = _binding.lib.HIL_APPLICATION_Encoded_Size
_native_encode = _binding.lib.HIL_APPLICATION_Encode_Message
_native_storage_size = _binding.lib.HIL_APPLICATION_Decode_Storage_Size
_native_decode = _binding.lib.HIL_APPLICATION_Decode_Message


@contextmanager
def _binding_boundary() -> Iterator[None]:
    """Prevent private CFFI and conversion exceptions from escaping the facade."""
    try:
        yield
    except (ApplicationError, MemoryError):
        raise
    except Exception:
        raise ApplicationBindingError("native Application binding or conversion failed") from None


def _check_status(
    value: int,
    error_type: type[ApplicationError],
    operation: str,
    *,
    sized_buffer: bool = False,
) -> None:
    try:
        status = ApplicationStatus(value)
    except ValueError:
        raise ApplicationBindingError(
            f"{operation} returned unknown Application status {value}"
        ) from None
    if status is ApplicationStatus.OK:
        return
    if status is ApplicationStatus.INTERNAL_ERROR:
        error_type = ApplicationInternalError
    elif status in (ApplicationStatus.INVALID_ARGUMENT, ApplicationStatus.UNINITIALIZED) or (
        status is ApplicationStatus.BUFFER_TOO_SMALL and sized_buffer
    ):
        error_type = ApplicationBindingError
    raise error_type(f"{operation} failed: {status.name}", status=status)


def _snapshot(data: Buffer) -> bytes:
    # Match Transport's Buffer/C-contiguous convention, but take ownership before
    # querying C so mutable caller storage cannot change between native calls.
    try:
        view = memoryview(data)
    except TypeError:
        raise TypeError("data must support the buffer protocol") from None
    with view:
        if not view.c_contiguous:
            raise BufferError("data must be C-contiguous")
        return view.tobytes()


def _build_message(message: ApplicationMessage) -> tuple[Any, list[Any]]:
    native = _binding.ffi.new("HIL_Application_Message_T *")
    native.has_test_id = 1
    native.test_id.bytes[0:16] = message.test_id.bytes
    native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
    if type(message) is TestConfiguration:
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION
        owners = _write_test_configuration(message, native.body.test_configuration)
    elif type(message) is TestInstruction:
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION
        owners = _write_test_instruction(message, native.body.test_instruction)
    else:
        assert type(message) is TestResult
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
        owners = _write_test_result(message, native.body.test_result)
    return native, owners


def _read_message(native: Any, storage: Any, capacity: int) -> ApplicationMessage:
    lib = _binding.lib
    supported = (
        lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION,
        lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION,
        lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT,
    )
    if native.type not in supported:
        if native.type in (
            lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST,
            lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE,
            lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA,
            lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA,
            lib.HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL,
            lib.HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL,
            lib.HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
            lib.HIL_APPLICATION_MESSAGE_TYPE_ERROR,
        ):
            # C may support a family that this public Python subset defers. No
            # native failure occurred, so do not manufacture a failure status.
            raise ApplicationDecodeError("Application message family is not supported by Python")
        raise ApplicationBindingError("native decoder returned an impossible message type")
    if native.has_test_id != 1 or native.subtype != lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE:
        raise ApplicationBindingError("native decoder returned an inconsistent message envelope")
    test_id = TestId(bytes(_binding.ffi.buffer(native.test_id.bytes, 16)))
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
        span = native.body.test_configuration.extension_data
        # Check ownership before dereferencing any native pointer. Configuration
        # extensions are the only decode-storage users in the supported subset.
        if span.size != capacity or span.data != storage:
            raise ApplicationBindingError("native extension span disagrees with decode storage")
        return _read_test_configuration(test_id, native.body.test_configuration)
    if capacity != 0:
        raise ApplicationBindingError("native fixed message unexpectedly used decode storage")
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
        return _read_test_instruction(test_id, native.body.test_instruction)
    return _read_test_result(test_id, native.body.test_result)


class ApplicationCodec:
    """Convert immutable messages and complete Application bytes using native C.

    Each instance belongs to its creating thread. It keeps codec policy only;
    it has no active test, endpoint role, sequence, tick or transaction state.
    """

    __slots__ = ("_config", "_owner_thread", "__context")

    def __init__(self, config: ApplicationConfig) -> None:
        if type(config) is not ApplicationConfig:
            raise TypeError("config must be an ApplicationConfig")
        with _binding_boundary():
            native_config = _build_native_config(config)
            context = _binding.ffi.new("HIL_Application_Context_T *")
            _check_status(
                _native_init(context, native_config),
                ApplicationConfigurationError,
                "initialization",
            )
        self.__context = context
        self._config = config
        self._owner_thread = threading.current_thread()

    def _check_owner(self) -> None:
        if threading.current_thread() is not self._owner_thread:
            raise ApplicationOwnershipError(
                "ApplicationCodec may only be used by its creating thread"
            )

    @property
    def config(self) -> ApplicationConfig:
        """Immutable policy copied into the native context at initialization."""
        self._check_owner()
        return self._config

    def encode(self, message: ApplicationMessage) -> bytes:
        """Validate and encode one supported complete message through native C."""
        self._check_owner()
        if type(message) not in (TestConfiguration, TestInstruction, TestResult):
            raise TypeError("message must be a TestConfiguration, TestInstruction or TestResult")
        with _binding_boundary():
            native, owners = _build_message(message)
            required = _binding.ffi.new("size_t *")
            _check_status(
                _native_encoded_size(self.__context, native, required),
                ApplicationEncodeError,
                "encoded-size query",
            )
            capacity = int(required[0])
            if not 0 < capacity <= self._config.max_encoded_message_size:
                raise ApplicationBindingError(
                    "native encoded-size query returned an impossible size"
                )
            output = _binding.ffi.new("uint8_t[]", capacity)
            written = _binding.ffi.new("size_t *")
            _check_status(
                _native_encode(self.__context, native, output, capacity, written),
                ApplicationEncodeError,
                "encoding",
                sized_buffer=True,
            )
            if written[0] != capacity:
                raise ApplicationBindingError("encoded size disagrees with size query")
            result = bytes(_binding.ffi.buffer(output, capacity))
            # Retain all source spans through both synchronous native calls.
            del owners
            return result

    def decode(self, data: Buffer) -> ApplicationMessage:
        """Snapshot and decode one complete C-contiguous buffer through native C."""
        self._check_owner()
        snapshot = _snapshot(data)
        with _binding_boundary():
            # Always non-null, including empty input: C should report truncation.
            wire = _binding.ffi.new("uint8_t[]", snapshot or b"\x00")
            required = _binding.ffi.new("size_t *")
            _check_status(
                _native_storage_size(self.__context, wire, len(snapshot), required),
                ApplicationDecodeError,
                "decode-storage query",
            )
            capacity = int(required[0])
            if capacity > len(snapshot):
                raise ApplicationBindingError(
                    "native decode-storage query returned an impossible size"
                )
            if (
                _binding.ffi.alignof("max_align_t")
                % _binding.lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT
            ):
                raise ApplicationBindingError("native decode-storage alignment is incompatible")
            owner = _binding.ffi.NULL
            storage = _binding.ffi.NULL
            if capacity:
                width = _binding.ffi.sizeof("max_align_t")
                owner = _binding.ffi.new("max_align_t[]", (capacity + width - 1) // width)
                storage = _binding.ffi.cast("uint8_t *", owner)
            native = _binding.ffi.new("HIL_Application_Message_T *")
            used = _binding.ffi.new("size_t *")
            _check_status(
                _native_decode(
                    self.__context, wire, len(snapshot), native, storage, capacity, used
                ),
                ApplicationDecodeError,
                "decoding",
                sized_buffer=True,
            )
            if used[0] != capacity:
                raise ApplicationBindingError("used decode storage disagrees with storage query")
            result = _read_message(native, storage, capacity)
            del owner
            return result

    def __copy__(self) -> None:
        raise TypeError("ApplicationCodec instances cannot be copied")

    def __deepcopy__(self, memo: object) -> None:
        raise TypeError("ApplicationCodec instances cannot be deep-copied")

    def __reduce__(self) -> NoReturn:
        raise TypeError("ApplicationCodec instances cannot be pickled")

    def __reduce_ex__(self, protocol: SupportsIndex) -> NoReturn:
        raise TypeError("ApplicationCodec instances cannot be pickled")


__all__ = ["ApplicationCodec"]
