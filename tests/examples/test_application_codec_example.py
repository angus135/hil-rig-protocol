"""The Application example runs entirely without an external link."""

import os
import runpy
from pathlib import Path


def test_application_example_round_trips_and_explicit_transport_calls(capsys):
    root = Path(os.environ.get("HIL_RIG_PROTOCOL_PROJECT_ROOT", Path(__file__).parents[2]))
    path = root / "examples/python/application_codec.py"
    runpy.run_path(str(path), run_name="__main__")
    assert capsys.readouterr().out.splitlines() == [
        "TestConfiguration: 227 bytes, round trip OK",
        "TestInstruction: 73 bytes, round trip OK",
        "TestResult: 62 bytes, round trip OK",
        "Disconnected Transport submission: NOT_READY",
    ]
