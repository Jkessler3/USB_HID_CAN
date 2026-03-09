# Targets

## Active

### `stm32_f407_usbhost_can_joystick`

- Path: `firmware/targets/stm32_f407_usbhost_can_joystick`
- MCU: STM32F407VGT6 / STM32F407VGTx package LQFP100
- USB role: USB OTG FS Host (`USB_HOST` HID class)
- CAN role: CAN1 enabled for translator output
- Purpose: standalone USB-host joystick-to-CAN translator

## Notes

- Previous non-active target variants were removed from the active path during the joystick translator pivot.
- Active architecture details are documented in `docs/ARCHITECTURE.md`.
