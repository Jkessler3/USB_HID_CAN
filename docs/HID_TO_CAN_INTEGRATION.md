# HID-to-CAN Integration

Historical integration snapshot, 2026-04-28. Use [BRINGUP.md](BRINGUP.md) and [CANAEROSPACE_PROTOCOL.md](CANAEROSPACE_PROTOCOL.md) for the present source and receiver acceptance.

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
| `1500 / 0x5DC` | CANaerospace Axis X/Y | Node-ID, Data Type, Service Code, Message Code, X/Y as big-endian `USHORT2` |
| `1501..1506 / 0x5DD..0x5E2` | CANaerospace joystick parameters | Remaining axes, buttons, and status using CANaerospace DLC-8 headers |

The frozen protocol contract is documented in `docs/CANAEROSPACE_PROTOCOL.md`.
The default mapping is defined in `firmware/shared/mapping/src/joystick_can_mapping.c`.

## Bench Validation Checklist

Minimum pass conditions for Phase 3:

- USB joystick/gamepad enumerates as HID.
- Moving X/Y changes decoded axis values in UART logs.
- Button presses produce stable bit transitions in UART logs.
- CAN IDs `1500..1506` / `0x5DC..0x5E2` change with the same axes/buttons seen in UART logs.
- Service Code is `0x00` while HID reports are fresh and carries stale/disconnect/error bits during faults.
- Disconnect or stale HID input forces neutral/failsafe output.
- Debug build runs for 10-15 minutes without CAN TX stalls or UART lockups.

## Deferred Items

- Hardware validation of the actual joystick descriptor and report layout.
- Optional migration from target-local generic HID decoding to the shared layout parser for a known VID/PID profile.
- UART receive buffer guard fix applies only if/when the CH559/SERAERO `processUart()` path is restored into this repository.
