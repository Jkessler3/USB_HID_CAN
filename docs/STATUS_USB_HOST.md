# USB Host Status Audit

Audit date: 2026-03-09  
Active target reviewed: `firmware/targets/stm32_f407_usbhost_can_joystick`

## Summary

- USB host HID enumeration path: **implemented in code, not hardware-verified in this session**
- HID polling loop: **implemented in code, not hardware-verified in this session**
- CAN1 integration: **present and initialized in active target**
- Raw HID callback path: **implemented (no mouse/keyboard assumption in active callback)**
- Joystick-specific HID parsing: **not complete (raw capture only; final field decode pending)**

## 1) Enumeration Status

Evidence:

- `USB_HOST/App/usb_host.c`
  - `USBH_Init(...)`
  - `USBH_RegisterClass(..., USBH_HID_CLASS)`
  - `USBH_Start(...)`
  - `USBH_UserProcess(...)` handles connect/disconnect/class events
- `USB_HOST/Target/usbh_conf.c`
  - HCD low-level init and callbacks (`HAL_HCD_Connect_Callback`, `HAL_HCD_Disconnect_Callback`)
  - URB/pipe interface functions

Assessment:

- Enumeration state-machine wiring is present and appears correct for STM32Cube USB host HID.
- Runtime confirmation with a physical joystick is still required.

## 2) HID Polling Status

Evidence:

- `Core/Src/main.c`: main loop continuously calls `MX_USB_HOST_Process()`.
- `USB_HOST/App/usb_host.c`: `MX_USB_HOST_Process()` calls `USBH_Process(&hUsbHostFS)`.
- `Core/Src/main.c`: `USBH_HID_EventCallback(...)` exists and reads raw HID bytes from class FIFO (`USBH_HID_FifoRead`).

Assessment:

- Polling infrastructure exists and should drive HID class event handling.
- Current app callback captures raw report bytes and stores latest sample state.
- Final joystick field extraction/parsing still needs device-specific layout configuration.

## 3) Reusable Code

High-confidence reusable:

- USB host bring-up and HID class registration:
  - `USB_HOST/App/usb_host.c`
  - `USB_HOST/Target/usbh_conf.c`
  - `USB_HOST/Target/usbh_platform.c`
- CAN peripheral bring-up and IRQ path:
  - `Core/Src/main.c` CAN init/start/filter setup
  - `Core/Src/stm32f4xx_hal_msp.c` CAN GPIO/IRQ MSP
  - `Core/Src/stm32f4xx_it.c` CAN IRQ dispatch
- New shared translator-oriented modules:
  - `firmware/shared/can`
  - `firmware/shared/usb_host_hid`
  - `firmware/shared/mapping`

## 4) What Needs Rewrite / Hardening

1. HID parsing for joystick reports:
   - replace raw-byte capture-only callback logic with joystick semantic parser path
   - support target joystick axis/button report format

2. Event-to-CAN pipeline:
   - current firmware sends periodic proof CAN frame (`ID 0x321`)
   - next step is replacing proof payload with mapped control/status translator output
   - integrate full disconnect/stale failsafe policy in final mapped output

3. App structure cleanup:
   - move business logic out of generated `main.c` user blocks into dedicated app-layer source
   - keep CubeMX regeneration-safe boundaries

4. Runtime validation:
   - verify enumeration and report polling with actual Thrustmaster joystick
   - confirm CAN frame timing/content against RC controller expectations

## 5) Notes About Removed Active Paths

- OBD-focused shared modules and PC USB device HID transport path were removed from active architecture.
- Active direction is now strictly USB host HID joystick input to CAN translation.
