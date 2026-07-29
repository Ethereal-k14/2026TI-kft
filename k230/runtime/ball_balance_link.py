"""Portable K230/CPython vision-to-STM32 protocol and 1-D ball tracker."""

try:
    import struct
except ImportError:  # pragma: no cover - CanMV firmware normally provides it
    import ustruct as struct

SOF = b"\xA5\x5A"
VERSION = 1
MSG_VISION_POSE = 0x20
MSG_VISION_STATUS = 0x21


def crc16_ccitt(data):
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class ProtocolEncoder:
    """Allocation-bounded A5/5A encoder matching stm32f4 App_Protocol."""

    def __init__(self):
        self.sequence = 0

    def encode(self, message_id, payload, timestamp_us):
        if payload is None:
            payload = b""
        if len(payload) > 256:
            raise ValueError("payload exceeds protocol limit")
        body = struct.pack("<BBHHI", VERSION, message_id, len(payload),
                           self.sequence, timestamp_us & 0xFFFFFFFF) + payload
        self.sequence = (self.sequence + 1) & 0xFFFF
        return SOF + body + struct.pack("<H", crc16_ccitt(body))

    def vision_pose(self, position_um, velocity_um_s, confidence,
                    frame_age_us, timestamp_us):
        confidence = max(0, min(1000, int(confidence)))
        payload = struct.pack("<iiHI", int(position_um), int(velocity_um_s),
                              confidence, int(frame_age_us))
        return self.encode(MSG_VISION_POSE, payload, timestamp_us)


class BallPositionTracker:
    """Calibrated beam-axis tracker with jump rejection and filtered velocity."""

    def __init__(self, pixel_min, pixel_max, position_min_mm, position_max_mm,
                 velocity_alpha=0.35, max_jump_mm=35.0, timeout_ms=200):
        if pixel_max == pixel_min or position_max_mm == position_min_mm:
            raise ValueError("calibration span must be non-zero")
        if not 0.0 < velocity_alpha <= 1.0:
            raise ValueError("velocity_alpha must be in (0, 1]")
        self.scale_um_per_pixel = ((position_max_mm - position_min_mm) * 1000.0 /
                                   (pixel_max - pixel_min))
        self.offset_um = position_min_mm * 1000.0 - self.scale_um_per_pixel * pixel_min
        self.velocity_alpha = velocity_alpha
        self.max_jump_um = max_jump_mm * 1000.0
        self.timeout_us = timeout_ms * 1000
        self.position_um = 0.0
        self.velocity_um_s = 0.0
        self.timestamp_us = 0
        self.locked = False
        self.rejected = 0

    def update(self, pixel_x, detector_confidence, timestamp_us):
        """Return (position_um, velocity_um_s, confidence_0_1000, accepted)."""
        measured = self.scale_um_per_pixel * float(pixel_x) + self.offset_um
        confidence = max(0.0, min(1.0, float(detector_confidence)))
        if confidence <= 0.0:
            return int(self.position_um), int(self.velocity_um_s), 0, False
        if not self.locked:
            self.position_um = measured
            self.timestamp_us = timestamp_us
            self.locked = True
            return int(self.position_um), 0, int(confidence * 1000.0), True
        dt_us = (timestamp_us - self.timestamp_us) & 0xFFFFFFFF
        if dt_us == 0 or dt_us > self.timeout_us:
            self.locked = False
            self.velocity_um_s = 0.0
            return int(self.position_um), 0, 0, False
        residual = measured - self.position_um
        dynamic_gate = self.max_jump_um + abs(self.velocity_um_s) * dt_us * 1.0e-6
        if abs(residual) > dynamic_gate:
            self.rejected += 1
            return int(self.position_um), int(self.velocity_um_s), 0, False
        raw_velocity = residual * 1.0e6 / dt_us
        self.velocity_um_s += self.velocity_alpha * (raw_velocity - self.velocity_um_s)
        self.position_um = measured
        self.timestamp_us = timestamp_us
        return (int(self.position_um), int(self.velocity_um_s),
                int(confidence * 1000.0), True)


def detection_to_frame(tracker, encoder, center_x, confidence,
                       capture_timestamp_us, send_timestamp_us):
    """Single integration entry for any detector returning center-x/confidence."""
    pos, vel, quality, accepted = tracker.update(
        center_x, confidence, capture_timestamp_us)
    if not accepted:
        return None
    frame_age = (send_timestamp_us - capture_timestamp_us) & 0xFFFFFFFF
    return encoder.vision_pose(pos, vel, quality, frame_age, send_timestamp_us)
