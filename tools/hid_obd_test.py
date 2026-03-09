#!/usr/bin/env python3
"""
Minimal HID smoke test:
- sends OBD Mode 01 PID 0C request over report ID 0x01
- prints incoming events from report ID 0x81

Requires:
    pip install hidapi
"""

from __future__ import annotations

import argparse
import struct
import time

import hid

REPORT_SIZE = 64

RID_HOST_CMD = 0x01
RID_DEV_EVENT = 0x81

CMD_OBD_REQUEST = 0x02
EVT_CAN_RX = 0x01
EVT_OBD_RESPONSE = 0x02
EVT_STATUS = 0x03
EVT_ERROR = 0x7F


def build_report(report_id: int, cmd_type: int, seq: int, data: bytes = b"", value: int = 0) -> bytes:
    buf = bytearray(REPORT_SIZE)
    buf[0] = report_id
    buf[1] = cmd_type
    buf[2] = 0
    buf[3] = min(len(data), 8)
    struct.pack_into("<I", buf, 4, 0)      # can_id
    struct.pack_into("<I", buf, 8, seq)     # seq
    struct.pack_into("<I", buf, 12, value)  # value
    buf[16 : 16 + min(len(data), 8)] = data[:8]
    return bytes(buf)


def decode_event(report: bytes) -> str:
    if len(report) != REPORT_SIZE:
        return f"short report ({len(report)} bytes)"

    rid = report[0]
    evt_type = report[1]
    flags = report[2]
    dlc = report[3]
    can_id = struct.unpack_from("<I", report, 4)[0]
    seq = struct.unpack_from("<I", report, 8)[0]
    value = struct.unpack_from("<I", report, 12)[0]
    data = report[16 : 16 + min(dlc, 8)]

    if rid != RID_DEV_EVENT:
        return f"unexpected report id 0x{rid:02X}"

    if evt_type == EVT_CAN_RX:
        return f"CAN_RX seq={seq} id=0x{can_id:X} dlc={dlc} flags=0x{flags:02X} data={data.hex()}"
    if evt_type == EVT_OBD_RESPONSE:
        return f"OBD_RESPONSE seq={seq} payload={data.hex()} ts={value}"
    if evt_type == EVT_STATUS:
        return f"STATUS seq={seq} code={value}"
    if evt_type == EVT_ERROR:
        return f"ERROR seq={seq} code={value}"
    return f"UNKNOWN evt=0x{evt_type:02X} seq={seq} value={value} data={data.hex()}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vid", default="0x0483", help="USB VID (hex)")
    parser.add_argument("--pid", default="0x5750", help="USB PID (hex)")
    parser.add_argument("--timeout", type=float, default=3.0, help="seconds to wait for response")
    args = parser.parse_args()

    vid = int(args.vid, 16)
    pid = int(args.pid, 16)
    seq = 1

    dev = hid.device()
    dev.open(vid, pid)
    dev.set_nonblocking(True)

    req = build_report(
        report_id=RID_HOST_CMD,
        cmd_type=CMD_OBD_REQUEST,
        seq=seq,
        data=bytes([0x01, 0x0C]),  # Mode 01, PID 0C (RPM)
    )
    written = dev.write(req)
    print(f"sent {written} bytes")

    deadline = time.time() + args.timeout
    while time.time() < deadline:
        rx = dev.read(REPORT_SIZE, timeout_ms=50)
        if rx:
            print(decode_event(bytes(rx)))

    dev.close()


if __name__ == "__main__":
    main()
