"""Compile actual public headers to catch a span width change hidden by padding."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.mark.parametrize("version,accepted", [("0.3.1", False), ("0.4.0", True)])
def test_width_change_requires_new_abi_generation(
    tmp_path: Path, version: str, accepted: bool
) -> None:
    compiler = shutil.which("cc")
    if compiler is None:
        pytest.skip("a GCC-compatible C11 compiler is required for the layout probe")
    root = Path(__file__).resolve().parents[2]
    baseline = tmp_path / "baseline"
    current = tmp_path / "current"
    source_version = (root / "VERSION").read_text(encoding="ascii").strip()
    for tree, tree_version in ((baseline, "0.3.0"), (current, version)):
        shutil.copytree(root / "include", tree / "include")
        header = tree / "include/hil_rig_protocol/version.h"
        text = header.read_text(encoding="utf-8")
        text = text.replace(
            f'#define HIL_RIG_PROTOCOL_VERSION_STRING "{source_version}"',
            f'#define HIL_RIG_PROTOCOL_VERSION_STRING "{tree_version}"',
        )
        header.write_text(text, encoding="utf-8")
    span = baseline / "include/hil_rig_protocol/application/application_types.h"
    text = span.read_text(encoding="utf-8")
    assert "uint16_t size;" in text
    span.write_text(text.replace("uint16_t size;", "uint8_t size;"), encoding="utf-8")
    result = subprocess.run(
        [
            sys.executable,
            str(root / "scripts/check_application_abi.py"),
            "--baseline",
            str(baseline),
            "--current",
            str(current),
            "--cc",
            compiler,
            "--work-dir",
            str(tmp_path / "probe"),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    measured = json.loads(result.stdout)
    assert measured["baseline"]["layouts"]["span_fields"]["size_width"] == 1
    assert measured["current"]["layouts"]["span_fields"]["size_width"] == 2
    assert measured["layout_changed"] is True
    assert (result.returncode == 0) is accepted
    if not accepted:
        assert "public layout changed" in result.stderr
