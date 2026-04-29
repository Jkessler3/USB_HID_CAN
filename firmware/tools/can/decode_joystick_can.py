#!/usr/bin/env python3
"""Decode joystick translator CAN frames from text logs.

Supported input examples:
  can0  301   [8]  00 00 10 00 00 00 01 42
  (0.123456) can0 302#0302421001020304
  301#0000100000000142
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass


CONTROL_ID = 0x301
STATUS_ID = 0x302

STATUS_FLAGS = (
    (0x01, "JOYSTICK_PRESENT"),
    (0x02, "HID_SAMPLE_VALID"),
    (0x04, "DATA_STALE"),
    (0x08, "TX_BACKPRESSURE_SEEN"),
    (0x10, "HID_PARSE_ERROR"),
    (0x20, "USB_DISCONNECTED"),
)


@dataclass
class CanFrame:
    can_id: int
    data: bytes
    source: str


def s16_le(data: bytes, offset: int) -> int:
    value = data[offset] | (data[offset + 1] << 8)
    if value & 0x8000:
        value -= 0x10000
    return value


def s8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def parse_line(line: str) -> CanFrame | None:
    compact = re.search(r"\b([0-9A-Fa-f]{3})#([0-9A-Fa-f]{16})\b", line)
    if compact:
        return CanFrame(int(compact.group(1), 16), bytes.fromhex(compact.group(2)), line.rstrip())

    spaced = re.search(
        r"\b([0-9A-Fa-f]{3})\b\s+\[8\]\s+"
        r"([0-9A-Fa-f]{2}(?:\s+[0-9A-Fa-f]{2}){7})\b",
        line,
    )
    if spaced:
        return CanFrame(int(spaced.group(1), 16), bytes.fromhex(spaced.group(2)), line.rstrip())

    return None


def format_flags(flags: int) -> str:
    names = [name for mask, name in STATUS_FLAGS if flags & mask]
    return ",".join(names) if names else "none"


def decode(frame: CanFrame) -> str | None:
    if len(frame.data) != 8:
        return f"0x{frame.can_id:03X} INVALID_DLC len={len(frame.data)}"

    if frame.can_id == CONTROL_ID:
        steer = s16_le(frame.data, 0)
        throttle = s16_le(frame.data, 2)
        aux0 = s8(frame.data[4])
        aux1 = s8(frame.data[5])
        buttons = frame.data[6]
        sample = frame.data[7]
        return (
            f"0x301 control sample={sample:3d} "
            f"steer={steer:5d} throttle={throttle:5d} "
            f"aux0={aux0:4d} aux1={aux1:4d} buttons=0x{buttons:02X}"
        )

    if frame.can_id == STATUS_ID:
        flags = frame.data[0]
        axis_count = frame.data[1]
        sample = frame.data[2]
        frame_counter = frame.data[3]
        last_report_ms = int.from_bytes(frame.data[4:8], "little")
        return (
            f"0x302 status  sample={sample:3d} frame={frame_counter:3d} "
            f"axes={axis_count:2d} last_report_ms={last_report_ms:10d} "
            f"flags=0x{flags:02X} [{format_flags(flags)}]"
        )

    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logfile", nargs="?", help="Text CAN log. Reads stdin when omitted.")
    args = parser.parse_args()

    stream = open(args.logfile, "r", encoding="utf-8") if args.logfile else sys.stdin
    try:
        for line in stream:
            frame = parse_line(line)
            if frame is None:
                continue
            decoded = decode(frame)
            if decoded:
                print(decoded)
    finally:
        if args.logfile:
            stream.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
