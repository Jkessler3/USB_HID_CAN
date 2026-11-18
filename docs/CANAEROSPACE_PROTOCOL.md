# CANaerospace Joystick Protocol

This project uses classic 11-bit CANaerospace data frames for the USB-HID joystick-to-CAN path. The older `0x301` / `0x302` proprietary debug layout is superseded and must not be treated as the primary receiver contract.

## Frame Format

Every transmitted CANaerospace frame is DLC 8:

| Byte | Field |
| --- | --- |
| 0 | Node-ID |
| 1 | CANaerospace Data Type |
| 2 | Service Code |
| 3 | Message Code |
| 4..7 | Parameter data |

Multi-byte parameter data is big-endian. Node-ID defaults to `1`; Node-ID `0` is reserved for all-nodes / broadcast behavior and is not used as the transmitting node default.

## Service Code Policy

For Normal Operation Data, byte 2 carries the joystick data-quality bits:

| Bit | Meaning |
| --- | --- |
| `0x00` | valid / normal when no fault bits are set |
| `0x01` | data stale |
| `0x02` | USB disconnected |
| `0x04` | HID parse error |
| `0x08` | CAN TX backpressure seen before this successful retry |
| `0x10` | joystick not present |
| `0x20` | mapping/profile error |
| `0x40`, `0x80` | reserved |

If multiple conditions are true, the bits are ORed together. The detailed status parameter also carries status in byte 4, but the CANaerospace Service Code remains the primary validity indication.

## Message Code

Byte 3 is an 8-bit Message Code. The firmware maintains a counter per CANaerospace parameter/CAN identifier and advances it after the controller accepts the frame into a TX mailbox. A failed enqueue leaves the counter and deadline available for retry. External bus acknowledgment and freshness still require receiver-side observation. It wraps `255 -> 0`.

## Default Identifier Assignment

CANaerospace references often list identifiers in decimal. The mapper and documentation always show both decimal and hex.

| Decimal | Hex | Parameter | Data Type | Parameter data |
| --- | --- | --- | --- | --- |
| 1500 | `0x5DC` | Joystick Axis X/Y | `USHORT2` / `0x0D` | bytes 4..5 X, bytes 6..7 Y |
| 1501 | `0x5DD` | Joystick Axis Z/Rx | `USHORT2` / `0x0D` | bytes 4..5 Z, bytes 6..7 Rx |
| 1502 | `0x5DE` | Joystick Axis Ry/Rz | `USHORT2` / `0x0D` | bytes 4..5 Ry, bytes 6..7 Rz |
| 1503 | `0x5DF` | Joystick Axis Slider/Dial | `USHORT2` / `0x0D` | bytes 4..5 Slider, bytes 6..7 Dial |
| 1504 | `0x5E0` | Joystick Buttons 0..31 | `BLONG` / `0x05` | bytes 4..7 32 button bits |
| 1505 | `0x5E1` | Joystick Buttons 32..63 | `BLONG` / `0x05` | bytes 4..7 32 button bits |
| 1506 | `0x5E2` | Joystick Status | `UCHAR4` / `0x10` | byte 4 flags, byte 5 HID sample counter, byte 6 CAN frame counter, byte 7 age bucket |

The receiver assignment is provisional. A later brief calls for UCHAR4 button banks. This implementation preserves the April BLONG type and its byte order. Obtain receiver-owned vectors for buttons 0, 7, 8, 31, 32 and 63 before changing either firmware or desktop tooling. Available inputs depend on the attached joystick and supported descriptor fields.

Button 0 is bit 0 of the 32-bit BLONG value. The BLONG is serialized big-endian on CAN, so button 0 appears as bit 0 of byte 7 on the wire.

## Scaling and Failsafe

Axis values are unsigned 16-bit centered values. Center is `0x8000`; lower values indicate negative motion and higher values indicate positive motion. When data is stale, disconnected, or invalid, axis parameters transmit centered values and the Service Code fault bits indicate why the receiver should not trust the data.

Receivers should enter failsafe if fault bits are set, if Message Code gaps exceed expected bus loss under normal load, or if no fresh joystick parameter is received within the receiver timeout. A practical initial timeout is 250 ms.

## Bench Validation

1. Verify Node-ID is `1` unless intentionally configured otherwise.
2. Confirm every frame has DLC 8 and bytes 0..3 are the CANaerospace header.
3. Move X/Y/Z/Rx/Ry/Rz/Slider/Dial and confirm the matching decimal IDs 1500..1503 update.
4. Press buttons and confirm IDs 1504/1505 update using big-endian BLONG packing.
5. Disconnect and reconnect USB HID and confirm Service Code bits clear after valid samples resume.
6. Log CAN traffic for 10 minutes with UART debug on, then 30-60 minutes with UART debug off.
