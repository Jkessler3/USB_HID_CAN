# USB Host Joystick to CAN Translator

Standalone translator firmware workspace:

- USB Thrustmaster joystick input over STM32 USB Host HID
- mapped into CAN control/status frames for an RC controller

## Active Layout

- `firmware/shared/core`
- `firmware/shared/can`
- `firmware/shared/usb_host_hid`
- `firmware/shared/mapping`
- `firmware/targets/stm32_f407_usbhost_can_joystick`

## Direction

- USB is host-mode HID only in the active path.
- CAN1 is the outbound control transport.
- PC USB device HID and OBD request/response are not part of the active architecture.

## Key Docs

- `docs/ARCHITECTURE.md` - end-to-end translator architecture
- `docs/STATUS_USB_HOST.md` - audit status for USB host enumeration/polling and rewrite scope
