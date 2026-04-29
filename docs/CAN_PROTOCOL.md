# Joystick CAN Protocol

Version: 1

This document defines the CAN interface emitted by the STM32F407 USB-host joystick translator.

## Transport

- CAN type: classic CAN 2.0
- Identifier type: standard 11-bit IDs
- Byte order: little-endian for multi-byte numeric fields
- Nominal control update period: 20 ms
- Nominal status update period: 100 ms
- HID stale timeout: 250 ms
- Receiver trust timeout: treat control data as stale if no fresh `0x301` or valid `0x302` is seen for more than 250 ms

The transmitter checks CAN TX mailbox availability before building the next scheduled frame. If all TX mailboxes are full, it does not advance the CAN scheduler and sets the `TX_BACKPRESSURE_SEEN` status bit for later status publication.

## Frame `0x301`: Control

DLC: 8

| Byte | Field | Format |
| --- | --- | --- |
| 0-1 | steer | signed int16, little-endian |
| 2-3 | throttle | signed int16, little-endian |
| 4 | aux0 | signed int8 |
| 5 | aux1 | signed int8 |
| 6 | buttons | mapped button flags, bit 0 is button map 0, active-high |
| 7 | sample_counter | low 8 bits of HID sample counter |

Axis fields are centered signed values after mapping, deadzone, optional inversion, and saturation. The default range is `-1000..1000`.

When data is stale or invalid, control axes are forced to neutral (`0`) and mapped button flags are cleared.

## Frame `0x302`: Status

DLC: 8

| Byte | Field | Format |
| --- | --- | --- |
| 0 | status_flags | bitfield described below |
| 1 | axis_count | number of parsed axes in the current HID sample |
| 2 | sample_counter | low 8 bits of HID sample counter |
| 3 | frame_counter | low 8 bits of emitted CAN frame counter |
| 4-7 | last_report_ms | low 32 bits of `HAL_GetTick()` when the last HID report was accepted |

`sample_counter` matches the `0x301` counter when both frames describe the same accepted HID sample. `frame_counter` increments once per successfully built CAN frame and wraps modulo 256 in byte 3.

## Status Flags

| Bit | Mask | Name | Meaning |
| --- | --- | --- | --- |
| 0 | `0x01` | `JOYSTICK_PRESENT` | USB HID device is connected according to the app state |
| 1 | `0x02` | `HID_SAMPLE_VALID` | current sample is parsed, fresh, and usable |
| 2 | `0x04` | `DATA_STALE` | failsafe/neutral output is active due to stale, invalid, or disconnected input |
| 3 | `0x08` | `TX_BACKPRESSURE_SEEN` | at least one CAN TX attempt found no free mailbox since boot |
| 4 | `0x10` | `HID_PARSE_ERROR` | most recent HID report could not be parsed into a usable sample |
| 5 | `0x20` | `USB_DISCONNECTED` | USB device is not connected |
| 6-7 | `0xC0` | reserved | must be ignored by receivers |

Receivers should treat control data as usable only when `HID_SAMPLE_VALID` is set and `DATA_STALE` is clear.

## Counter Behavior

- `sample_counter` comes from the accepted HID report sequence and wraps modulo 256 on the bus.
- `frame_counter` is independent of `sample_counter` and wraps modulo 256 on the bus.
- If `0x301` repeats the same `sample_counter`, the joystick input has not changed or no newer HID report has been accepted.
- If `0x302.frame_counter` jumps unexpectedly under normal bus load, the receiver should record a missed status frame or logging gap.
- If `TX_BACKPRESSURE_SEEN` is set, inspect bus load and transmitter mailbox pressure before treating counter gaps as input-side faults.

## Receiver Rules

1. Decode little-endian signed axis values from `0x301`.
2. Cache the last `0x302` status.
3. Use control data only when the latest status says `HID_SAMPLE_VALID` and not `DATA_STALE`.
4. Force receiver-side neutral/failsafe if valid data is older than 250 ms.
5. Ignore reserved status bits.
6. Treat unknown DLCs as protocol errors.

