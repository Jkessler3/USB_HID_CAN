# Architecture: USB Host Joystick -> CAN Translator

## Purpose

Standalone translator box:

- USB Thrustmaster joystick plugs into STM32F407 USB FS host port
- STM32 reads HID input reports
- Input is mapped to control/state CAN frames
- CAN frames are sent to the RC controller

This architecture does **not** use PC USB device HID.

## High-Level Data Flow

1. USB host stack initializes (`MX_USB_HOST_Init`).
2. USB host background polling runs in main loop (`MX_USB_HOST_Process`).
3. Host HID events/report data are converted into joystick samples.
4. Mapping layer converts joystick samples to CAN control frames.
5. CAN1 transmits mapped frames at a fixed update period.
6. On disconnect/timeouts, failsafe mapping sends neutral/safe frames.

## Active Target

- `firmware/targets/stm32_f407_usbhost_can_joystick`
- `.ioc`: `stm32_f407_usbhost_can_joystick.ioc`
- USB mode: `USB_OTG_FS Host_Only`, HID host class
- CAN: `CAN1` enabled (`PB8/PB9`), RX interrupt configured

## Shared Modules

- `firmware/shared/core`
  - generic utility support
- `firmware/shared/can`
  - `can_frame_t` and frame helpers
- `firmware/shared/usb_host_hid`
  - USB host HID device state tracking
  - generic joystick state model + report layout parser
- `firmware/shared/mapping`
  - joystick state -> CAN control/status frame mapping
  - fixed-rate frame scheduling + timeout failsafe state handling

## HID Input Handling Strategy

Current baseline in target code captures raw HID bytes in `USBH_HID_EventCallback(...)` using `USBH_HID_FifoRead(...)`. This is suitable for bring-up and report discovery but not yet full joystick semantic decoding.

Planned robust path:

1. Keep USB host class-level enumeration and state handling from `USB_HOST/App/usb_host.c`.
2. Add joystick-specific HID report decode path in `shared/usb_host_hid`:
   - parse report IDs/axis/button fields for the target joystick profile
   - normalize to common sample struct
3. Feed normalized sample into mapping layer for CAN output.

## Joystick-to-CAN Mapping

Mapping output uses two standard IDs:

- control frame: `0x301`
- status/health frame: `0x302`

Control frame contents include:

- steer/throttle axes (signed 16-bit, little-endian)
- button bitmap
- connected/failsafe flags

Status frame contents include:

- connection state
- failsafe state
- last-input timestamp fields for diagnostics

## Failsafe Behavior

Failsafe is entered when either condition is true:

1. USB disconnection event is received.
2. Input sample timeout exceeds configured threshold (`stale_timeout_ms`).

Failsafe actions:

- force neutral command outputs (`steer=0`, `throttle=0`)
- assert failsafe flag in output frame
- continue periodic status frame emission

## Out of Scope (By Direction)

- PC USB device HID transport
- OBD request/response protocol path
