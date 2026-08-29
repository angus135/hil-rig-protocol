"""Integration coverage for Python-owned Transport configuration policy."""

from __future__ import annotations

from hil_rig_protocol import SessionState, TransportConfig

from .parity_helpers import H2R, complete_application, establish
from .transport_pair_harness import TransportPairHarness

_UINT64_MAX = (1 << 64) - 1


def test_generated_host_session_seed_establishes_real_session() -> None:
    host_config = TransportConfig(
        session_seed=None,
        initial_reliable_sequence=17,
        retransmit_timeout_ms=10,
        max_retries=2,
    )
    rig_config = TransportConfig(
        session_seed=0,
        initial_reliable_sequence=917,
        retransmit_timeout_ms=10,
        max_retries=2,
    )

    with TransportPairHarness(host_config=host_config, rig_config=rig_config) as pair:
        generated_seed = pair.host.config.session_seed
        assert generated_seed is not None
        assert 1 <= generated_seed <= _UINT64_MAX - 1
        assert generated_seed != _UINT64_MAX

        establish(pair)
        assert pair.host.get_status().session_state is SessionState.ESTABLISHED
        assert pair.rig.get_status().session_state is SessionState.ESTABLISHED
        complete_application(pair, H2R, b"generated-seed-real-session", base_time=700)
