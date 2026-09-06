"""Application/Transport composition uses only the existing opaque byte facade."""

import hil_rig_protocol as p
import pytest
from application_fixtures import configuration, instruction, result

from .transport_pair_harness import TransportTestDirection


def deliver(pair, sender, receiver, direction, ack_direction, encoded):
    assert sender.submit_application_data(encoded) is p.TransportStatus.OK
    transfer = pair.transfer_one_output(direction)
    assert transfer.accept.status is p.TransportStatus.OK
    assert transfer.delivery.status is p.TransportStatus.OK
    received = receiver.read_application_data()
    assert received == encoded
    assert receiver.read_application_data() is None
    ack = pair.transfer_one_output(ack_direction)
    assert ack.accept.status is p.TransportStatus.OK
    assert ack.delivery.status is p.TransportStatus.OK
    event = sender.read_event()
    assert event.type is p.EventType.DELIVERY_CONFIRMED
    assert event.status is p.TransportStatus.OK
    assert sender.read_event() is None
    assert not sender.get_status().reliable_delivery_pending
    return received


@pytest.mark.parametrize("extension", [b"", b"extension", bytes(range(255))])
def test_complete_configuration_instruction_result_exchange(established_pair, extension):
    pair = established_pair
    host_codec = p.ApplicationCodec(p.ApplicationConfig())
    rig_codec = p.ApplicationCodec(p.ApplicationConfig())
    forward = TransportTestDirection.HOST_TO_RIG
    reverse = TransportTestDirection.RIG_TO_HOST
    assert pair.host.config.max_application_message_size == 512
    assert pair.rig.config.max_application_message_size == 512
    for public in (configuration(extension), instruction()):
        encoded = host_codec.encode(public)
        received = deliver(pair, pair.host, pair.rig, forward, reverse, encoded)
        assert rig_codec.decode(received) == public
        if type(public) is p.TestConfiguration:
            assert len(encoded) == 226 + len(extension)
    public = result()
    encoded = rig_codec.encode(public)
    received = deliver(pair, pair.rig, pair.host, reverse, forward, encoded)
    assert host_codec.decode(received) == public


def test_delivery_confirmation_does_not_validate_application(established_pair):
    pair = established_pair
    malformed = b"not an Application message"
    received = deliver(
        pair,
        pair.host,
        pair.rig,
        TransportTestDirection.HOST_TO_RIG,
        TransportTestDirection.RIG_TO_HOST,
        malformed,
    )
    codec = p.ApplicationCodec(p.ApplicationConfig())
    with pytest.raises(p.ApplicationDecodeError):
        codec.decode(received)
    assert pair.host.get_status().session_state is p.SessionState.ESTABLISHED
    assert pair.rig.get_status().session_state is p.SessionState.ESTABLISHED
