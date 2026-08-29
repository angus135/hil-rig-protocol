"""Validate hil-rig-protocol source and binary distribution contents."""

from __future__ import annotations

import argparse
import email.parser
import re
import sys
import tarfile
import zipfile
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

if __package__:
    from .check_version import read_version
else:  # Direct execution sets sys.path[0] to scripts/.
    from check_version import read_version  # type: ignore[import-not-found,no-redef]

PACKAGE_MODULES = {
    "hil_rig_protocol/__init__.py",
    "hil_rig_protocol/_binding.py",
    "hil_rig_protocol/errors.py",
    "hil_rig_protocol/transport.py",
    "hil_rig_protocol/transport_types.py",
    "hil_rig_protocol/py.typed",
}

SDIST_REQUIRED = {
    "VERSION",
    "CMakeLists.txt",
    "pyproject.toml",
    "README.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "include/hil_rig_protocol/version.h",
    "include/hil_rig_protocol/transport/transport.h",
    "include/hil_rig_protocol/transport/transport_types.h",
    "src/version.c",
    "src/application/CMakeLists.txt",
    "src/application/application.c",
    "src/application/application_message.c",
    "src/transport/transport.c",
    "src/transport/transport_profiles.cmake",
    "src/transport/internal/third_party/cobs/LICENSE.txt",
    "src/transport/internal/third_party/cobs/cobs.c",
    "bindings/python/CMakeLists.txt",
    "bindings/python/build_ffi.py",
    "bindings/python/cdef.py",
    "bindings/python/hil_rig_protocol_ffi.c",
    "bindings/python/hil_rig_protocol_ffi.h",
    "bindings/python/hil_rig_protocol_ffi_allocator.c",
    "bindings/python/hil_rig_protocol_ffi_allocator.h",
    *(f"python/{name}" for name in PACKAGE_MODULES),
}


class ValidationError(RuntimeError):
    """One or more distribution invariants were violated."""


def _require_entries(names: set[str], required: Iterable[str], artifact: Path) -> None:
    missing = sorted(set(required) - names)
    if missing:
        raise ValidationError(f"{artifact}: missing required entries: {', '.join(missing)}")


def _sdist_names(path: Path) -> set[str]:
    with tarfile.open(path, "r:gz") as archive:
        raw_names = [name.rstrip("/") for name in archive.getnames() if name.rstrip("/")]
    roots = {PurePosixPath(name).parts[0] for name in raw_names}
    if len(roots) != 1:
        raise ValidationError(f"{path}: sdist must contain exactly one top-level directory")
    root = next(iter(roots))
    return {
        PurePosixPath(name).relative_to(root).as_posix()
        for name in raw_names
        if PurePosixPath(name).as_posix() != root
    }


def validate_sdist(path: Path, version: str) -> None:
    """Validate an sdist manifest needed for an isolated native-wheel rebuild."""
    expected = f"hil_rig_protocol-{version}.tar.gz"
    if path.name != expected:
        raise ValidationError(f"{path}: expected sdist filename {expected}")
    names = _sdist_names(path)
    _require_entries(names, SDIST_REQUIRED, path)
    source_prefixes = (
        "src/transport/internal/common/",
        "src/transport/internal/mvp/",
        "include/hil_rig_protocol/application/",
    )
    for prefix in source_prefixes:
        if not any(name.startswith(prefix) for name in names):
            raise ValidationError(f"{path}: missing required source tree {prefix}")
    prohibited = sorted(
        name
        for name in names
        if re.search(
            r"(?:^|/)(?:build[^/]*|cmake-build[^/]*)/"
            r"|(?:^|/)(?:\.pytest_cache|__pycache__)(?:/|$)|\.pyc$",
            name,
        )
    )
    if prohibited:
        raise ValidationError(f"{path}: contains prohibited build/cache entries: {prohibited}")


def _metadata(archive: zipfile.ZipFile, metadata_name: str) -> email.message.Message:
    parser = email.parser.Parser()
    return parser.parsestr(archive.read(metadata_name).decode("utf-8"))


def validate_wheel(path: Path, version: str) -> None:
    """Validate wheel identity, metadata, native containment, and hygiene."""
    expected_prefix = f"hil_rig_protocol-{version}-"
    if not path.name.startswith(expected_prefix) or path.suffix != ".whl":
        raise ValidationError(f"{path}: wheel filename must start with {expected_prefix!r}")

    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        _require_entries(names, PACKAGE_MODULES, path)
        native = [
            name
            for name in names
            if name.startswith("hil_rig_protocol/_native")
            and (name.endswith(".so") or name.endswith(".pyd"))
        ]
        if len(native) != 1:
            raise ValidationError(f"{path}: expected exactly one _native .so/.pyd, found {native}")
        if "win_amd64" in path.name and not native[0].endswith(".pyd"):
            raise ValidationError(f"{path}: Windows wheel must contain _native.pyd")
        if "manylinux" in path.name and not native[0].endswith(".so"):
            raise ValidationError(f"{path}: manylinux wheel must contain _native.so")
        extra_native_libraries = sorted(
            name
            for name in names
            if name != native[0] and re.search(r"\.(?:a|so|dylib|lib|dll)$", name)
        )
        if extra_native_libraries:
            raise ValidationError(
                f"{path}: contains separate native libraries: {extra_native_libraries}"
            )

        dist_info = f"hil_rig_protocol-{version}.dist-info"
        metadata_name = f"{dist_info}/METADATA"
        wheel_name = f"{dist_info}/WHEEL"
        record_name = f"{dist_info}/RECORD"
        _require_entries(names, {metadata_name, wheel_name, record_name}, path)
        metadata = _metadata(archive, metadata_name)
        if metadata.get("Name") != "hil-rig-protocol":
            raise ValidationError(f"{path}: METADATA Name is {metadata.get('Name')!r}")
        if metadata.get("Version") != version:
            raise ValidationError(f"{path}: METADATA Version is {metadata.get('Version')!r}")
        if metadata.get("License-Expression") != "MIT":
            raise ValidationError(f"{path}: METADATA does not declare License-Expression: MIT")

        licence_entries = [name for name in names if name.startswith(f"{dist_info}/licenses/")]
        required_licences = ("LICENSE", "LICENSE.txt", "THIRD_PARTY_NOTICES.md")
        for filename in required_licences:
            if not any(PurePosixPath(name).name == filename for name in licence_entries):
                raise ValidationError(f"{path}: missing packaged licence material {filename}")

        prohibited_patterns = {
            "generated CFFI source": re.compile(r"hil_rig_protocol_native\.c$"),
            "adapter test executable": re.compile(
                r"hil_rig_protocol_python_adapter_tests(?:\.exe)?$"
            ),
            "separate protocol library": re.compile(
                r"(?:^|/)(?:lib)?hil_rig_protocol\.(?:a|so|dylib|lib|dll)$"
            ),
            "CMake build artifact": re.compile(
                r"(?:CMakeCache\.txt|CMakeFiles/|cmake_install\.cmake)"
            ),
            "object file": re.compile(r"\.(?:o|obj)$"),
            "test source": re.compile(r"(?:^|/)tests?/"),
            "Python cache": re.compile(r"(?:__pycache__/|\.pyc$)"),
            "instrumentation runtime": re.compile(r"(?:asan|ubsan|gcov)", re.IGNORECASE),
        }
        for description, pattern in prohibited_patterns.items():
            matches = sorted(name for name in names if pattern.search(name))
            if matches:
                raise ValidationError(f"{path}: contains prohibited {description}: {matches}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("sdist", "wheel"))
    parser.add_argument("artifacts", nargs="+", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()

    try:
        version = read_version(args.root.resolve())
        validator = validate_sdist if args.kind == "sdist" else validate_wheel
        for artifact in args.artifacts:
            validator(artifact.resolve(), version)
            print(f"validated {args.kind}: {artifact}")
    except (OSError, ValidationError, ValueError, zipfile.BadZipFile) as error:
        print(f"distribution validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
