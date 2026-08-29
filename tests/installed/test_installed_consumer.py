"""Self-contained downstream smoke test for an installed binary wheel."""

from __future__ import annotations

import importlib.metadata
import os
import runpy
from pathlib import Path

import hil_rig_protocol
import hil_rig_protocol._native as native
from hil_rig_protocol import (
    LinkState,
    OperatingMode,
    Role,
    SessionState,
    Transport,
    TransportConfig,
    TransportStatus,
)


def _transfer(sender: Transport, receiver: Transport, now_ms: int) -> bool:
    output = sender.peek_output()
    if output is None:
        return False
    assert sender.peek_output() == output
    assert sender.commit_output(now_ms) is TransportStatus.OK
    received = receiver.receive_bytes(output)
    assert received.status is TransportStatus.OK
    assert received.bytes_consumed == len(output)
    return True


def _drain_events(transport: Transport) -> None:
    while transport.read_event() is not None:
        pass


def _establish(host: Transport, rig: Transport) -> None:
    assert host.notify_link_state(LinkState.CONNECTED, 0) is TransportStatus.OK
    assert rig.notify_link_state(LinkState.CONNECTED, 0) is TransportStatus.OK
    for now_ms in range(32):
        assert host.process(now_ms, OperatingMode.NORMAL) in {
            TransportStatus.OK,
            TransportStatus.NOT_READY,
        }
        assert rig.process(now_ms, OperatingMode.NORMAL) in {
            TransportStatus.OK,
            TransportStatus.NOT_READY,
        }
        for _ in range(16):
            moved = _transfer(host, rig, now_ms)
            moved = _transfer(rig, host, now_ms) or moved
            if not moved:
                break
        if (
            host.get_status().session_state is SessionState.ESTABLISHED
            and rig.get_status().session_state is SessionState.ESTABLISHED
        ):
            return
    raise AssertionError("installed HOST/RIG pair did not establish")


def _exchange(sender: Transport, receiver: Transport, payload: bytes, now_ms: int) -> None:
    assert sender.submit_application_data(payload) is TransportStatus.OK
    assert _transfer(sender, receiver, now_ms)
    assert receiver.read_application_data() == payload
    assert receiver.read_application_data() is None
    assert _transfer(receiver, sender, now_ms)
    _drain_events(sender)


def test_installed_distribution_metadata_imports_handshake_and_example() -> None:
    project_root = Path(os.environ["HIL_RIG_PROTOCOL_PROJECT_ROOT"]).resolve()
    package_path = Path(hil_rig_protocol.__file__).resolve()
    native_path = Path(native.__file__).resolve()
    assert not package_path.is_relative_to(project_root / "python")
    assert not native_path.is_relative_to(project_root)
    assert (
        importlib.metadata.version("hil-rig-protocol")
        == (project_root / "VERSION").read_text(encoding="ascii").strip()
    )

    host_config = TransportConfig(
        session_seed=0x81230000,
        initial_reliable_sequence=10,
        retransmit_timeout_ms=10,
        max_retries=2,
    )
    rig_config = TransportConfig(
        session_seed=0,
        initial_reliable_sequence=500,
        retransmit_timeout_ms=10,
        max_retries=2,
    )
    with Transport(Role.HOST, host_config) as host, Transport(Role.RIG, rig_config) as rig:
        _establish(host, rig)
        _drain_events(host)
        _drain_events(rig)
        _exchange(host, rig, b"installed-host-to-rig", 40)
        _exchange(rig, host, b"installed-rig-to-host\x00\xff", 41)

    runpy.run_path(
        str(project_root / "examples" / "python" / "transport_servicing.py"),
        run_name="__main__",
    )
