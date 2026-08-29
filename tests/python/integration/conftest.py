"""Fixtures for real two-endpoint Python Transport integration tests."""

from __future__ import annotations

import pytest

from hil_rig_protocol import TransportStatus

from .transport_pair_harness import TransportPairHarness, drain_events


@pytest.fixture
def transport_pair() -> TransportPairHarness:
    pair = TransportPairHarness()
    try:
        yield pair
    finally:
        pair.close()


@pytest.fixture
def established_pair(transport_pair: TransportPairHarness) -> TransportPairHarness:
    host_status, rig_status = transport_pair.connect()
    assert host_status is TransportStatus.OK
    assert rig_status is TransportStatus.OK
    transport_pair.establish_clean_session()
    drain_events(transport_pair.host)
    drain_events(transport_pair.rig)
    return transport_pair
