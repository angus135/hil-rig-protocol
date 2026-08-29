"""Generate the out-of-line CFFI API-mode source used by CMake."""

from __future__ import annotations

import argparse
from pathlib import Path

from cffi import FFI

from cdef import CDEF


def create_builder() -> FFI:
    """Create the binding builder without compiling anything."""
    builder = FFI()
    builder.cdef(CDEF)
    builder.set_source(
        "hil_rig_protocol._native",
        '#include "hil_rig_protocol_ffi.h"\n',
    )
    return builder


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        required=True,
        type=Path,
        help="Path where the generated C source should be written.",
    )
    args = parser.parse_args()

    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    create_builder().emit_c_code(str(output))


if __name__ == "__main__":
    main()
