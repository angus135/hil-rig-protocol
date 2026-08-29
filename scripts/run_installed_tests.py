"""Run consumer tests against an installed wheel from outside its source tree."""

from __future__ import annotations

import argparse
import importlib.metadata
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True, type=Path)
    args = parser.parse_args()
    project_root = args.project_root.resolve()

    import hil_rig_protocol
    import hil_rig_protocol._native as native

    package_path = Path(hil_rig_protocol.__file__).resolve()
    native_path = Path(native.__file__).resolve()
    source_package = project_root / "python" / "hil_rig_protocol"
    if package_path.is_relative_to(source_package) or native_path.is_relative_to(project_root):
        raise RuntimeError(
            "wheel test imported from source/build tree: "
            f"package={package_path}, native={native_path}"
        )
    version = (project_root / "VERSION").read_text(encoding="ascii").strip()
    if importlib.metadata.version("hil-rig-protocol") != version:
        raise RuntimeError("installed metadata version does not match VERSION")

    test_paths = [
        project_root / "tests" / "python",
        project_root / "tests" / "examples",
        project_root / "tests" / "installed",
    ]
    with tempfile.TemporaryDirectory(prefix="hil-rig-installed-tests-") as directory:
        environment = os.environ.copy()
        environment["HIL_RIG_PROTOCOL_PROJECT_ROOT"] = str(project_root)
        result = subprocess.run(
            [sys.executable, "-m", "pytest", *(str(path) for path in test_paths)],
            cwd=directory,
            env=environment,
            check=False,
        )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
