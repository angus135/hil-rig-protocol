"""Focused tests for reusable package-version and artifact validators."""

from __future__ import annotations

import io
import tarfile
import zipfile
from pathlib import Path

import pytest

from scripts.check_version import read_version, validate_public_header, validate_tag
from scripts.verify_distribution import (
    PACKAGE_MODULES,
    SDIST_REQUIRED,
    ValidationError,
    validate_sdist,
    validate_wheel,
)


def test_version_reader_and_tag_validation_are_strict(tmp_path: Path) -> None:
    version_file = tmp_path / "VERSION"
    version_file.write_text("1.2.3\n", encoding="ascii")
    assert read_version(tmp_path) == "1.2.3"
    validate_tag("v1.2.3", "1.2.3")

    for invalid in ("01.2.3\n", "1.2\n", "1.2.3", "v1.2.3\n"):
        version_file.write_text(invalid, encoding="ascii")
        with pytest.raises(ValueError):
            read_version(tmp_path)
    with pytest.raises(ValueError, match="does not match"):
        validate_tag("v1.2.4", "1.2.3")


def test_checked_in_public_header_must_match_version(tmp_path: Path) -> None:
    header = tmp_path / "version.h"
    header.write_text(
        "#define HIL_RIG_PROTOCOL_VERSION_MAJOR 1u\n"
        "#define HIL_RIG_PROTOCOL_VERSION_MINOR 2u\n"
        "#define HIL_RIG_PROTOCOL_VERSION_PATCH 3u\n"
        '#define HIL_RIG_PROTOCOL_VERSION_STRING "1.2.3"\n',
        encoding="utf-8",
    )
    validate_public_header(header, "1.2.3")

    header.write_text(header.read_text(encoding="utf-8").replace("3u", "4u"), encoding="utf-8")
    with pytest.raises(ValueError, match="public version header"):
        validate_public_header(header, "1.2.3")


def _write_tar_member(archive: tarfile.TarFile, name: str, content: bytes = b"x") -> None:
    info = tarfile.TarInfo(name)
    info.size = len(content)
    archive.addfile(info, io.BytesIO(content))


def test_sdist_validator_accepts_required_rebuild_manifest(tmp_path: Path) -> None:
    sdist = tmp_path / "hil_rig_protocol-1.2.3.tar.gz"
    names = set(SDIST_REQUIRED)
    names.update(
        {
            "src/transport/internal/common/transport_crc.c",
            "src/transport/internal/mvp/transport_profile_mvp.c",
            "include/hil_rig_protocol/application/application.h",
        }
    )
    with tarfile.open(sdist, "w:gz") as archive:
        for name in names:
            _write_tar_member(archive, f"hil_rig_protocol-1.2.3/{name}")
    validate_sdist(sdist, "1.2.3")


def _write_wheel(path: Path, *, include_native: bool = True) -> None:
    dist_info = "hil_rig_protocol-1.2.3.dist-info"
    with zipfile.ZipFile(path, "w") as archive:
        for name in PACKAGE_MODULES:
            archive.writestr(name, "")
        if include_native:
            archive.writestr("hil_rig_protocol/_native.cp312-win_amd64.pyd", b"binary")
        archive.writestr(
            f"{dist_info}/METADATA",
            "Metadata-Version: 2.4\nName: hil-rig-protocol\nVersion: 1.2.3\n"
            "License-Expression: MIT\n\n",
        )
        archive.writestr(f"{dist_info}/WHEEL", "Wheel-Version: 1.0\n")
        archive.writestr(f"{dist_info}/RECORD", "")
        for name in ("LICENSE", "LICENSE.txt", "THIRD_PARTY_NOTICES.md"):
            archive.writestr(f"{dist_info}/licenses/{name}", "licence")


def test_wheel_validator_is_cross_platform_and_diagnoses_missing_native(tmp_path: Path) -> None:
    valid = tmp_path / "hil_rig_protocol-1.2.3-cp312-cp312-win_amd64.whl"
    _write_wheel(valid)
    validate_wheel(valid, "1.2.3")

    invalid = tmp_path / "hil_rig_protocol-1.2.3-cp312-cp312-manylinux_2_28_x86_64.whl"
    _write_wheel(invalid, include_native=False)
    with pytest.raises(ValidationError, match="exactly one _native"):
        validate_wheel(invalid, "1.2.3")
