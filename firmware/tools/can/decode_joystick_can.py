#!/usr/bin/env python3
"""Decode joystick translator CANaerospace frames from text logs.

Supported input examples:
  can0  5DC   [8]  01 0D 00 22 80 00 7F FF
  (0.123456) can0 5DC#010D002280007FFF
  0x5DC#010D002280007FFF
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass


DATA_TYPES = {
    0x03: "LONG",
    0x04: "ULONG",
    0x05: "BLONG",
    0x06: "SHORT",
    0x07: "USHORT",
    0x08: "BSHORT",
    0x0A: "UCHAR",
    0x0B: "BCHAR",
    0x0C: "SHORT2",
    0x0D: "USHORT2",
    0x0E: "BSHORT2",
    0x10: "UCHAR4",
    0x11: "BCHAR4",
    0x13: "UCHAR2",
    0x14: "BCHAR2",
}

PARAMETERS = {
    1500: "Joystick Axis X/Y",
    1501: "Joystick Axis Z/Rx",
    1502: "Joystick Axis Ry/Rz",
    1503: "Joystick Axis Slider/Dial",
    1504: "Joystick Buttons 0..31",
    1505: "Joystick Buttons 32..63",
    1506: "Joystick Status",
}

SERVICE_FLAGS = (
    (0x01, "DATA_STALE"),
    (0x02, "USB_DISCONNECTED"),
    (0x04, "HID_PARSE_ERROR"),
    (0x08, "TX_BACKPRESSURE_SEEN"),
    (0x10, "JOYSTICK_NOT_PRESENT"),
    (0x20, "MAPPING_ERROR"),
)


@dataclass
class CanFrame:
    can_id: int
    data: bytes
    source: str


# Accept project-decimal IDs and the three-digit hexadecimal IDs in candump logs.
LOG_LINE = re.compile(
    r"^\s*(?:\([0-9.]+\)\s+)?(?:(?P<iface>[A-Za-z][\w.:-]*)\s+)?"
    r"(?P<id>0[xX][\da-fA-F]{1,3}|[\da-fA-F]{1,4})"
    r"(?:#(?P<compact>[\da-fA-F]{16})|\s+\[(?P<dlc>\d+)\]\s+"
    r"(?P<spaced>[\da-fA-F]{2}(?:\s+[\da-fA-F]{2}){7}))\s*$"
)


def parse_line(line: str) -> CanFrame | None:
    match = LOG_LINE.fullmatch(line)
    if not match or (match["dlc"] is not None and int(match["dlc"]) != 8):
        return None

    text = match["id"]
    socketcan_hex = False
    if match["iface"] is not None and not text.lower().startswith("0x"):
        socketcan_hex = len(text) == 3
        if not socketcan_hex:
            # Historical logs also use the four-digit decimal project identifiers.
            if len(text) != 4 or not text.isdecimal() or text.startswith("0"):
                return None
    base = 16 if socketcan_hex or not text.isdecimal() else 10
    can_id = int(text, base)
    if can_id > 0x7FF:
        return None
    return CanFrame(can_id, bytes.fromhex(match["compact"] or match["spaced"]), line.rstrip())


def format_service(service: int) -> str:
    names = [name for mask, name in SERVICE_FLAGS if service & mask]
    return ",".join(names) if names else "VALID"


def u16_be(data: bytes, offset: int) -> int:
    return (data[offset] << 8) | data[offset + 1]


def decode_parameter(can_id: int, data_type: int, data: bytes) -> str:
    if data_type == 0x0D:
        labels = {
            1500: ("X", "Y"),
            1501: ("Z", "Rx"),
            1502: ("Ry", "Rz"),
            1503: ("Slider", "Dial"),
        }.get(can_id, ("A", "B"))
        return f"{labels[0]}={u16_be(data, 4)} {labels[1]}={u16_be(data, 6)}"
    if data_type == 0x05 and can_id in (1504, 1505):
        return f"buttons=0x{int.from_bytes(data[4:8], 'big'):08X}"
    if data_type == 0x10 and can_id == 1506:
        return f"status=0x{data[4]:02X} hid_sample={data[5]} frame={data[6]} age_bucket={data[7]}"
    return f"data={data[4:8].hex(' ').upper()}"


def decode(frame: CanFrame) -> str:
    if len(frame.data) != 8:
        return f"{frame.can_id}/0x{frame.can_id:03X} INVALID_DLC len={len(frame.data)}"

    node_id, data_type, service, message_code = frame.data[:4]
    type_name = DATA_TYPES.get(data_type, f"UNKNOWN_0x{data_type:02X}")
    parameter = PARAMETERS.get(frame.can_id, "Unassigned parameter")
    if frame.can_id in PARAMETERS:
        expected = 0x0D if frame.can_id < 1504 else (0x05 if frame.can_id < 1506 else 0x10)
        if data_type != expected:
            return (f"{frame.can_id}/0x{frame.can_id:03X} {parameter} INVALID_TYPE "
                    f"expected={DATA_TYPES[expected]}/0x{expected:02X} actual={type_name}/0x{data_type:02X} "
                    f"raw={frame.data[4:].hex().upper()}")
    values = decode_parameter(frame.can_id, data_type, frame.data)

    return (
        f"{frame.can_id}/0x{frame.can_id:03X} {parameter} "
        f"node={node_id} type={type_name}/0x{data_type:02X} "
        f"svc=0x{service:02X}[{format_service(service)}] msg=0x{message_code:02X} {values}"
    )


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
            print(decode(frame))
    finally:
        if args.logfile:
            stream.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
