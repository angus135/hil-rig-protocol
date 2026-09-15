"""Stateless, direction-neutral Application codec backed exclusively by native C."""

from __future__ import annotations

import threading
from collections.abc import Buffer, Iterator
from contextlib import contextmanager
from typing import Any, NoReturn, SupportsIndex

from . import _binding
from ._application_conversion import (
    _build_native_config,
    _read_error,
    _read_execution_control,
    _read_global_control,
    _read_response,
    _read_system_info_request,
    _read_system_info_response,
    _read_test_configuration,
    _read_test_instruction,
    _read_test_result,
    _read_update_instruction,
    _read_variable_test_result,
    _write_error,
    _write_execution_control,
    _write_global_control,
    _write_response,
    _write_system_info_request,
    _write_system_info_response,
    _write_test_configuration,
    _write_test_instruction,
    _write_test_result,
    _write_update_instruction,
    _write_variable_test_result,
)
from .application_types import (
    PROTOCOL_VERSION,
    ApplicationConfig,
    ApplicationErrorMessage,
    ApplicationMessage,
    ApplicationResponse,
    ApplicationStatus,
    ExecutionControl,
    GlobalControl,
    ProtocolVersion,
    SystemInfoRequest,
    SystemInfoResponse,
    TestConfiguration,
    TestId,
    TestInstruction,
    TestResult,
    UpdateInstruction,
    VariableTestResult,
)
from .errors import (
    ApplicationBindingError,
    ApplicationConfigurationError,
    ApplicationDecodeError,
    ApplicationEncodeError,
    ApplicationError,
    ApplicationInternalError,
    ApplicationOwnershipError,
    ApplicationVersionMismatchError,
)

_native_init = _binding.lib.HIL_APPLICATION_Init
_native_encoded_size = _binding.lib.HIL_APPLICATION_Encoded_Size
_native_encode = _binding.lib.HIL_APPLICATION_Encode_Message
_native_storage_size = _binding.lib.HIL_APPLICATION_Decode_Storage_Size
_native_decode = _binding.lib.HIL_APPLICATION_Decode_Message
_native_check_protocol_version = _binding.lib.HIL_APPLICATION_Check_Protocol_Version


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


def check_protocol_version(peer_version: ProtocolVersion) -> None:
    """Require exact equality between a received discovery version and this build."""
    if type(peer_version) is not ProtocolVersion:
        raise TypeError("peer_version must be a ProtocolVersion")
    with _binding_boundary():
        status = ApplicationStatus(
            _native_check_protocol_version(
                peer_version.major, peer_version.minor, peer_version.patch
            )
        )
    if status is ApplicationStatus.OK:
        return
    if status is ApplicationStatus.VERSION_MISMATCH:
        raise ApplicationVersionMismatchError(PROTOCOL_VERSION, peer_version)
    raise ApplicationBindingError(
        f"protocol-version check returned unexpected Application status {status.name}",
        status=status,
    )


def _build_message(message: ApplicationMessage) -> tuple[Any, list[Any]]:
    native = _binding.ffi.new("HIL_Application_Message_T *")
    if type(message) is SystemInfoRequest:
        native.has_test_id = 0
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST
        owners = _write_system_info_request(message, native.body.system_info_request)
    elif type(message) is SystemInfoResponse:
        native.has_test_id = 0
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE
        owners = _write_system_info_response(message, native.body.system_info_response)
    elif type(message) is GlobalControl:
        native.has_test_id = 0
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL
        owners = _write_global_control(message, native.body.global_control)
    elif type(message) is ExecutionControl:
        native.has_test_id = 1
        native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL
        owners = _write_execution_control(message, native.body.execution_control)
    elif type(message) is TestConfiguration:
        native.has_test_id = 1
        native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION
        owners = _write_test_configuration(message, native.body.test_configuration)
    elif type(message) is TestInstruction:
        native.has_test_id = 1
        native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION
        owners = _write_test_instruction(message, native.body.test_instruction)
    elif type(message) is TestResult:
        native.has_test_id = 1
        native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
        owners = _write_test_result(message, native.body.test_result)
    elif type(message) is UpdateInstruction:
        native.has_test_id = 1
        native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION
        owners = _write_update_instruction(message, native.body.update_instruction)
    elif type(message) is VariableTestResult:
        native.has_test_id = 1
        native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT
        owners = _write_variable_test_result(message, native.body.variable_test_result)
    elif type(message) is ApplicationResponse:
        native.has_test_id = int(message.test_id is not None)
        if message.test_id is not None:
            native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_RESPONSE
        owners = _write_response(message, native.body.response)
    elif type(message) is ApplicationErrorMessage:
        native.has_test_id = int(message.test_id is not None)
        if message.test_id is not None:
            native.test_id.bytes[0:16] = message.test_id.bytes
        native.subtype = _binding.lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE
        native.type = _binding.lib.HIL_APPLICATION_MESSAGE_TYPE_ERROR
        owners = _write_error(message, native.body.error)
    else:
        raise TypeError("message is not a supported Application message value")
    return native, owners


def _copy_response_spans(native: Any, storage: Any, capacity: int) -> tuple[bytes, bytes]:
    """Verify native response span ownership before making detached Python copies."""
    diagnostic = native.body.system_info_response.diagnostic_data
    git_hash = native.body.system_info_response.firmware_git_hash
    diagnostic_size = int(diagnostic.size)
    hash_size = int(git_hash.size)
    if diagnostic_size + hash_size != capacity:
        raise ApplicationBindingError("native response span sizes disagree with decode storage")
    if diagnostic_size:
        if diagnostic.data != storage:
            raise ApplicationBindingError("native diagnostic span does not start at decode storage")
    elif diagnostic.data != _binding.ffi.NULL:
        raise ApplicationBindingError("native empty diagnostic span has a pointer")
    if hash_size:
        if git_hash.data != storage + diagnostic_size:
            raise ApplicationBindingError(
                "native Git-hash span has an invalid decode-storage offset"
            )
    elif git_hash.data != _binding.ffi.NULL:
        raise ApplicationBindingError("native empty Git-hash span has a pointer")
    diagnostic_bytes = (
        bytes(_binding.ffi.buffer(diagnostic.data, diagnostic_size)) if diagnostic_size else b""
    )
    hash_bytes = bytes(_binding.ffi.buffer(git_hash.data, hash_size)) if hash_size else b""
    return diagnostic_bytes, hash_bytes


def _read_message(native: Any, storage: Any, capacity: int) -> ApplicationMessage:
    lib = _binding.lib
    supported = (
        lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST,
        lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE,
        lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION,
        lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION,
        lib.HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL,
        lib.HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL,
        lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT,
        lib.HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION,
        lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT,
        lib.HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
        lib.HIL_APPLICATION_MESSAGE_TYPE_ERROR,
    )
    if native.type not in supported:
        if native.type in (
            lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA,
            lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA,
        ):
            # C may support a family that this public Python subset defers. No
            # native failure occurred, so do not manufacture a failure status.
            raise ApplicationDecodeError("Application message family is not supported by Python")
        raise ApplicationBindingError("native decoder returned an impossible message type")
    if native.type in (
        lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST,
        lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE,
    ):
        if native.has_test_id != 0 or native.subtype != lib.HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC:
            raise ApplicationBindingError(
                "native decoder returned an inconsistent discovery envelope"
            )
        if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            if capacity != 0:
                raise ApplicationBindingError(
                    "native fixed message unexpectedly used decode storage"
                )
            return _read_system_info_request(native.body.system_info_request)
        diagnostic_data, firmware_git_hash = _copy_response_spans(native, storage, capacity)
        return _read_system_info_response(
            native.body.system_info_response, diagnostic_data, firmware_git_hash
        )
    if native.type in (
        lib.HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
        lib.HIL_APPLICATION_MESSAGE_TYPE_ERROR,
    ):
        if native.subtype != lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE:
            raise ApplicationBindingError("native decoder returned an inconsistent envelope")
        if native.has_test_id not in (0, 1):
            raise ApplicationBindingError("native decoder returned invalid Test-ID presence")
        test_id = (
            TestId(bytes(_binding.ffi.buffer(native.test_id.bytes, 16)))
            if native.has_test_id
            else None
        )
        if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            if capacity != 0:
                raise ApplicationBindingError(
                    "native fixed message unexpectedly used decode storage"
                )
            return _read_response(test_id, native.body.response)
        diagnostic = native.body.error.diagnostic_data
        diagnostic_size = int(diagnostic.size)
        if diagnostic_size != capacity:
            raise ApplicationBindingError("native Error span size disagrees with decode storage")
        if diagnostic_size:
            if diagnostic.data != storage:
                raise ApplicationBindingError("native Error span does not start at decode storage")
            diagnostic_data = bytes(_binding.ffi.buffer(diagnostic.data, diagnostic_size))
        else:
            if diagnostic.data != _binding.ffi.NULL:
                raise ApplicationBindingError("native empty Error span has a pointer")
            diagnostic_data = b""
        return _read_error(test_id, native.body.error, diagnostic_data)
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
        if native.has_test_id != 0 or native.subtype != lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE:
            raise ApplicationBindingError("native decoder returned an inconsistent global envelope")
        if capacity != 0:
            raise ApplicationBindingError("native fixed message unexpectedly used decode storage")
        return _read_global_control(native.body.global_control)
    if native.has_test_id != 1 or native.subtype != lib.HIL_APPLICATION_MESSAGE_SUBTYPE_NONE:
        raise ApplicationBindingError("native decoder returned an inconsistent message envelope")
    test_id = TestId(bytes(_binding.ffi.buffer(native.test_id.bytes, 16)))
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION:
        return _read_update_instruction(test_id, native.body.update_instruction, storage, capacity)
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT:
        return _read_variable_test_result(
            test_id, native.body.variable_test_result, storage, capacity
        )
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
        if capacity != 0:
            raise ApplicationBindingError("native fixed message unexpectedly used decode storage")
        return _read_execution_control(test_id, native.body.execution_control)
    if native.type == lib.HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
        span = native.body.test_configuration.extension_data
        # Check ownership before dereferencing any native pointer.
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
        if type(message) not in (
            SystemInfoRequest,
            SystemInfoResponse,
            TestConfiguration,
            TestInstruction,
            ExecutionControl,
            GlobalControl,
            TestResult,
            UpdateInstruction,
            VariableTestResult,
            ApplicationResponse,
            ApplicationErrorMessage,
        ):
            raise TypeError("message is not a supported Application message value")
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
            storage_limit = len(snapshot)
            if len(snapshot) >= 31 and snapshot[19] in (21, 34):
                # TLV descriptors expand into native structs, so storage may exceed wire size.
                ctype = (
                    "HIL_Application_Logical_Operation_T" if snapshot[19] == 21
                    else "HIL_Application_Captured_Record_T"
                )
                alignment = int(_binding.lib.HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT)
                descriptor_bytes = snapshot[27] * _binding.ffi.sizeof(ctype)
                storage_limit += ((descriptor_bytes + alignment - 1) // alignment) * alignment
            if capacity > storage_limit:
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


__all__ = ["ApplicationCodec", "check_protocol_version"]
