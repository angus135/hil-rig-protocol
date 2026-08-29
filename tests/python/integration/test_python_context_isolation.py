"""Python-specific native-context isolation and lifetime integration tests."""

from __future__ import annotations

import gc
import weakref

from hil_rig_protocol import EventType, SessionState, TransportStatus

from .parity_helpers import (
    H2R,
    R2H,
    complete_application,
    deliver_item,
    establish,
    make_pair,
    take_output,
)


def test_two_python_transport_pairs_remain_isolated_when_interleaved() -> None:
    payload_a = b"pair-A-only"
    payload_b = b"pair-B-only"

    with (
        make_pair(
            host_seed=0x1111222233334444,
            host_sequence=101,
            rig_sequence=501,
            host_now_ms=1000,
            rig_now_ms=2000,
        ) as pair_a,
        make_pair(
            host_seed=0xAAAABBBBCCCCDDDD,
            host_sequence=202,
            rig_sequence=602,
            host_now_ms=3000,
            rig_now_ms=4000,
        ) as pair_b,
    ):
        establish(pair_a)
        establish(pair_b)

        assert pair_a.host.submit_application_data(payload_a) is TransportStatus.OK
        assert pair_b.host.submit_application_data(payload_b) is TransportStatus.OK

        item_a = take_output(pair_a, H2R, now_ms=1100)
        item_b = take_output(pair_b, H2R, now_ms=3100)

        # Deliberately service B before A after both native contexts own work.
        assert deliver_item(pair_b, item_b) is TransportStatus.OK
        assert pair_b.rig.read_application_data() == payload_b
        assert pair_a.rig.read_application_data() is None

        assert deliver_item(pair_a, item_a) is TransportStatus.OK
        assert pair_a.rig.read_application_data() == payload_a
        assert pair_b.rig.read_application_data() is None

        ack_b = take_output(pair_b, R2H, now_ms=3101)
        ack_a = take_output(pair_a, R2H, now_ms=1101)
        assert deliver_item(pair_a, ack_a) is TransportStatus.OK
        assert deliver_item(pair_b, ack_b) is TransportStatus.OK

        event_a = pair_a.host.read_event()
        event_b = pair_b.host.read_event()
        assert event_a is not None and event_a.type is EventType.DELIVERY_CONFIRMED
        assert event_b is not None and event_b.type is EventType.DELIVERY_CONFIRMED

        assert pair_a.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair_b.host.get_status().session_state is SessionState.ESTABLISHED


def test_python_transport_lifetime_isolation_between_pairs() -> None:
    pair_a = make_pair(host_seed=0x1010101010101010)
    pair_b = make_pair(host_seed=0x2020202020202020)
    try:
        establish(pair_a)
        establish(pair_b)

        pair_a.close()
        assert pair_b.host.get_status().session_state is SessionState.ESTABLISHED
        complete_application(pair_b, H2R, b"pair-B-after-explicit-close", base_time=500)

        # Create a third live native pair, then rely solely on CFFI GC fallback.
        pair_c = make_pair(host_seed=0x3030303030303030)
        establish(pair_c)
        host_ref = weakref.ref(pair_c.host)
        rig_ref = weakref.ref(pair_c.rig)
        pair_c = None
        gc.collect()
        assert host_ref() is None
        assert rig_ref() is None

        # Releasing unrelated native contexts must not affect pair B.
        assert pair_b.host.get_status().session_state is SessionState.ESTABLISHED
        complete_application(pair_b, R2H, b"pair-B-after-gc-close", base_time=520)
    finally:
        pair_a.close()
        pair_b.close()
