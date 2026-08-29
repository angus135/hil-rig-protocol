"""Smoke tests for the private CFFI Transport binding."""

from __future__ import annotations

import hil_rig_protocol
from hil_rig_protocol import _binding


def _create_transport(role: int, *, session_seed: int | None = None):
    ffi = _binding.ffi
    lib = _binding.lib
    config = ffi.new("HIL_Transport_Config_T *")
    transport = ffi.new("HIL_Python_Transport_T **")
    core_status = ffi.new("HIL_Transport_Status_T *")

    lib.HIL_PY_TRANSPORT_Default_Config(config)
    if session_seed is not None:
        config.session_seed = session_seed

    create_status = lib.HIL_PY_TRANSPORT_Create(role, config, transport, core_status)
    try:
        assert create_status == lib.HIL_PY_ADAPTER_STATUS_OK
        assert core_status[0] == lib.HIL_TRANSPORT_STATUS_OK
        assert transport[0] != ffi.NULL
    except BaseException:
        if transport[0] != ffi.NULL:
            lib.HIL_PY_TRANSPORT_Destroy(transport[0])
        raise

    return transport[0]


def test_package_and_private_binding_import() -> None:
    assert hil_rig_protocol.__name__ == "hil_rig_protocol"
    assert _binding.ffi is not None
    assert _binding.lib is not None


def test_complete_adapter_surface_is_declared() -> None:
    expected_functions = (
        "HIL_PY_TRANSPORT_Default_Config",
        "HIL_PY_TRANSPORT_Create",
        "HIL_PY_TRANSPORT_Destroy",
        "HIL_PY_TRANSPORT_Reset",
        "HIL_PY_TRANSPORT_Notify_Link_State",
        "HIL_PY_TRANSPORT_Submit_Application_Data",
        "HIL_PY_TRANSPORT_Receive_Bytes",
        "HIL_PY_TRANSPORT_Process",
        "HIL_PY_TRANSPORT_Peek_Output",
        "HIL_PY_TRANSPORT_Commit_Output",
        "HIL_PY_TRANSPORT_Read_Application_Data",
        "HIL_PY_TRANSPORT_Read_Event",
        "HIL_PY_TRANSPORT_Get_Status",
    )

    for function_name in expected_functions:
        assert hasattr(_binding.lib, function_name)


def test_create_and_destroy_host_transport() -> None:
    transport = _binding.ffi.NULL
    try:
        transport = _create_transport(
            _binding.lib.HIL_TRANSPORT_ROLE_HOST,
            session_seed=0x12345678,
        )
    finally:
        _binding.lib.HIL_PY_TRANSPORT_Destroy(transport)


def test_create_and_destroy_rig_transport() -> None:
    transport = _binding.ffi.NULL
    try:
        transport = _create_transport(_binding.lib.HIL_TRANSPORT_ROLE_RIG)
    finally:
        _binding.lib.HIL_PY_TRANSPORT_Destroy(transport)
