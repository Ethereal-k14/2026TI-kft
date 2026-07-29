import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from runtime.ball_balance_link import (  # noqa: E402
    BallPositionTracker, ProtocolEncoder, crc16_ccitt, detection_to_frame,
)


def test_protocol_and_tracker():
    tracker = BallPositionTracker(100, 500, -100.0, 100.0)
    encoder = ProtocolEncoder()
    frame = detection_to_frame(tracker, encoder, 300, 0.9, 1_000_000, 1_004_000)
    assert frame[:2] == b"\xA5\x5A"
    assert frame[2] == 1 and frame[3] == 0x20
    assert struct.unpack_from("<H", frame, 4)[0] == 14
    assert struct.unpack_from("<i", frame, 12)[0] == 0
    assert struct.unpack_from("<H", frame, len(frame) - 2)[0] == crc16_ccitt(frame[2:-2])

    frame = detection_to_frame(tracker, encoder, 320, 0.8, 1_020_000, 1_024_000)
    assert frame is not None
    assert struct.unpack_from("<i", frame, 12)[0] == 10_000
    assert struct.unpack_from("<i", frame, 16)[0] > 0

    assert detection_to_frame(tracker, encoder, 500, 0.9,
                              1_040_000, 1_044_000) is None
    assert tracker.rejected == 1


if __name__ == "__main__":
    test_protocol_and_tracker()
    print("K230 ball balance link tests: PASS")
