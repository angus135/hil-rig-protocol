"""Validate the single hil-rig-protocol package version authority."""

from __future__ import annotations

import argparse
import re
import sys
import tomllib
from pathlib import Path

VERSION_PATTERN = re.compile(
    r"(?P<major>0|[1-9][0-9]*)\.(?P<minor>0|[1-9][0-9]*)\.(?P<patch>0|[1-9][0-9]*)"
)


def read_version(root: Path) -> str:
    """Read and strictly validate the repository VERSION file."""
    raw = (root / "VERSION").read_text(encoding="ascii")
    if not raw.endswith("\n") or raw.count("\n") != 1:
        raise ValueError("VERSION must contain one newline-terminated MAJOR.MINOR.PATCH value")
    version = raw[:-1]
    if VERSION_PATTERN.fullmatch(version) is None:
        raise ValueError(f"VERSION is not strict MAJOR.MINOR.PATCH: {version!r}")
    return version


def validate_configuration(root: Path, version: str) -> None:
    """Verify CMake and Python metadata are wired to VERSION rather than literals."""
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    required_cmake = (
        'file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HIL_RIG_PROTOCOL_VERSION)',
        'project(hil_rig_protocol VERSION "${HIL_RIG_PROTOCOL_VERSION}" LANGUAGES C)',
    )
    for text in required_cmake:
        if text not in cmake:
            raise ValueError(f"CMake version wiring is missing: {text}")

    pyproject = tomllib.loads((root / "pyproject.toml").read_text(encoding="utf-8"))
    project = pyproject["project"]
    if "version" in project or "version" not in project.get("dynamic", []):
        raise ValueError("[project] version must be dynamic and must not be hard-coded")
    providers = pyproject.get("tool", {}).get("dynamic-metadata", [])
    version_providers = [item for item in providers if item.get("field") == "version"]
    if len(version_providers) != 1:
        raise ValueError("exactly one dynamic version provider is required")
    provider = version_providers[0]
    if provider.get("provider") != "scikit_build_core.metadata.regex":
        raise ValueError("version must use scikit-build-core's built-in regex provider")
    if provider.get("input") != "VERSION":
        raise ValueError("dynamic version metadata must read VERSION")
    match = re.search(provider["regex"], (root / "VERSION").read_text(encoding="ascii"))
    if match is None or match.group("value") != version:
        raise ValueError("dynamic metadata regex does not resolve to VERSION")


def validate_public_header(path: Path, version: str) -> None:
    """Verify the checked-in public header mirrors the authoritative VERSION value."""
    header = path.read_text(encoding="utf-8")
    major, minor, patch = version.split(".")
    expected = (
        f"#define HIL_RIG_PROTOCOL_VERSION_MAJOR {major}u",
        f"#define HIL_RIG_PROTOCOL_VERSION_MINOR {minor}u",
        f"#define HIL_RIG_PROTOCOL_VERSION_PATCH {patch}u",
        f'#define HIL_RIG_PROTOCOL_VERSION_STRING "{version}"',
    )
    for text in expected:
        if text not in header:
            raise ValueError(f"public version header does not contain {text!r}: {path}")


def validate_tag(tag: str, version: str) -> None:
    """Require a strict release tag whose value equals VERSION."""
    match = re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", tag)
    if match is None:
        raise ValueError(f"release tag must have strict vMAJOR.MINOR.PATCH syntax: {tag!r}")
    if tag[1:] != version:
        raise ValueError(f"release tag {tag!r} does not match VERSION {version!r}")


def validate_artifact_names(paths: list[Path], version: str) -> None:
    """Verify supplied distribution filenames contain the authoritative version."""
    expected_prefix = f"hil_rig_protocol-{version}"
    for path in paths:
        if not path.name.startswith(expected_prefix):
            raise ValueError(
                f"artifact filename does not use name/version {expected_prefix!r}: {path.name}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--tag", help="Optional release tag to validate")
    parser.add_argument("artifacts", nargs="*", type=Path)
    args = parser.parse_args()

    try:
        root = args.root.resolve()
        version = read_version(root)
        validate_configuration(root, version)
        validate_public_header(root / "include" / "hil_rig_protocol" / "version.h", version)
        if args.tag is not None:
            validate_tag(args.tag, version)
        validate_artifact_names(args.artifacts, version)
    except (KeyError, OSError, TypeError, ValueError) as error:
        print(f"version validation failed: {error}", file=sys.stderr)
        return 1

    print(f"version validation passed: {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
