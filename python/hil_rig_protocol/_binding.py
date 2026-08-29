"""Private access to the compiled HIL-RIG protocol CFFI module."""

from hil_rig_protocol._native import ffi, lib

__all__ = ["ffi", "lib"]
