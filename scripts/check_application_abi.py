"""Compare public native layouts using the same C11 compiler for two source trees."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any

_PROBE = r"""
#include <stddef.h>
#include <stdio.h>
#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/version.h"

#define LAYOUT(type) \
    printf("\"" #type "\":{\"size\":%zu,\"alignment\":%zu},", sizeof(type), _Alignof(type))
int main(void)
{
    printf("{\"version\":\"%s\",\"layouts\":{", HIL_RIG_PROTOCOL_VERSION_STRING);
    LAYOUT(HIL_Application_Byte_Span_T);
    LAYOUT(HIL_Application_System_Info_Response_T);
    LAYOUT(HIL_Application_Test_Configuration_T);
    LAYOUT(HIL_Application_Logical_Operation_T);
    LAYOUT(HIL_Application_Captured_Record_T);
    LAYOUT(HIL_Application_Message_T);
    printf("\"span_fields\":{\"data_offset\":%zu,\"size_offset\":%zu,\"size_width\":%zu},",
        offsetof(HIL_Application_Byte_Span_T, data), offsetof(HIL_Application_Byte_Span_T, size),
        sizeof(((HIL_Application_Byte_Span_T){0}).size));
    printf("\"message_fields\":{\"body_offset\":%zu,\"body_size\":%zu}}}\n",
        offsetof(HIL_Application_Message_T, body), sizeof(((HIL_Application_Message_T){0}).body));
    return 0;
}
"""


def _probe(source_tree: Path, compiler: str, work_dir: Path, name: str) -> dict[str, Any]:
    """Compile actual public headers; no guessed native sizes or field offsets."""
    source = work_dir / f"{name}.c"
    executable = work_dir / name
    source.write_text(_PROBE, encoding="utf-8")
    subprocess.run(
        [
            compiler,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(source_tree.resolve() / "include"),
            str(source),
            "-o",
            str(executable),
        ],
        check=True,
    )
    result: dict[str, Any] = json.loads(subprocess.check_output([str(executable)], text=True))
    return result


def _abi_generation(version: str) -> str:
    major, minor, _ = version.split(".")
    return f"{major}.{minor}" if major == "0" else major


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--current", type=Path, default=Path("."))
    parser.add_argument("--cc", default="cc", help="C11 compiler with GCC-style arguments")
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    work_dir = args.work_dir.resolve()
    work_dir.mkdir(parents=True, exist_ok=True)
    baseline = _probe(args.baseline, args.cc, work_dir, "baseline")
    current = _probe(args.current, args.cc, work_dir, "current")
    changed = baseline["layouts"] != current["layouts"]
    different_generation = _abi_generation(baseline["version"]) != _abi_generation(
        current["version"]
    )
    print(
        json.dumps(
            {
                "baseline": baseline,
                "current": current,
                "layout_changed": changed,
                "abi_generation_changed": different_generation,
            },
            indent=2,
        )
    )
    if changed and not different_generation:
        raise SystemExit("public layout changed without changing the C ABI generation")


if __name__ == "__main__":
    main()
