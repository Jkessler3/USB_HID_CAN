# HID-to-CAN Integration

Snapshot date: 2026-04-28

## Transport Baseline Checkpoint

Current known-good baseline for this repository:

- Target: `firmware/targets/stm32_f407_usbhost_can_joystick`
- Build command:
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug`
- CAN1 initialization and transmit path build successfully.
- USART2 debug output is wired through `_write(...)` in `App/joystick_translator_app.c`.
- USB host HID enumeration/polling stack is generated under `USB_HOST/*` and should be treated as frozen while moving up-stack.

This workspace does not currently contain the CH559/SERAERO symbols referenced in some bring-up notes:

- `config.h`
- `SEND_SERAERO_USBHOST`
- `pollHIDdevice()`
- `txBuffer`
- `sendSERaero()`
- `processUart()`
- `uartRxBuff`

For this STM32 target, the equivalent active path is:

1. `USBH_HID_EventCallback(...)` in `Core/Src/main.c`
2. `joystick_translator_app_on_hid_report(...)`
3. normalized `usb_host_hid_joystick_state_t`
4. `joystick_can_pipeline_build_next_frame(...)`
5. structured classic-CAN frames

## CAN Packet Format

The production-facing CAN path is signal-based rather than a raw UART-packet mirror.

| CAN ID | Purpose | Payload |
| --- | --- | --- |
| `0x301` | Control | steer int16 LE, throttle int16 LE, aux0, aux1, button flags, HID sample counter |
| `0x302` | Status | validity/stale/error flags, axis count, HID sample counter, CAN frame counter, last HID report timestamp |

The frozen protocol contract is documented in `docs/CAN_PROTOCOL.md`.
The default mapping is defined in `firmware/shared/mapping/src/joystick_can_mapping.c`.

## Bench Validation Checklist

Minimum pass conditions for Phase 3:

- USB joystick/gamepad enumerates as HID.
- Moving X/Y changes decoded axis values in UART logs.
- Button presses produce stable bit transitions in UART logs.
- CAN ID `0x301` changes with the same axes/buttons seen in UART logs.
- CAN ID `0x302` reports connected and non-failsafe while HID reports are fresh.
- Disconnect or stale HID input forces neutral/failsafe output.
- Debug build runs for 10-15 minutes without CAN TX stalls or UART lockups.

## Deferred Items

- Hardware validation of the actual joystick descriptor and report layout.
- Optional migration from target-local generic HID decoding to the shared layout parser for a known VID/PID profile.
- UART receive buffer guard fix applies only if/when the CH559/SERAERO `processUart()` path is restored into this repository.
