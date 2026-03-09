# CubeMX Regeneration Rules

## Hard Rules

1. Do not place business logic in generated files except inside USER CODE blocks.
2. Keep translator logic in shared modules:
   - `firmware/shared/core`
   - `firmware/shared/can`
   - `firmware/shared/usb_host_hid`
   - `firmware/shared/mapping`
3. Keep target-specific glue in `firmware/targets/stm32_f407_usbhost_can_joystick`.
4. Keep ISR callbacks short and non-blocking.

## File Ownership

Generated or generated-adjacent (edit only in USER CODE blocks when possible):

- `firmware/targets/stm32_f407_usbhost_can_joystick/Core/*`
- `firmware/targets/stm32_f407_usbhost_can_joystick/USB_HOST/*`
- `firmware/targets/stm32_f407_usbhost_can_joystick/Drivers/*`
- `firmware/targets/stm32_f407_usbhost_can_joystick/Middlewares/*`
- `firmware/targets/stm32_f407_usbhost_can_joystick/stm32_f407_usbhost_can_joystick.ioc`

User-owned files (safe for full edits):

- `firmware/shared/**/*`
- `firmware/tools/**/*`
- `docs/**/*`

## Required Control Flow Contract

`Core/Src/main.c` must keep this pattern:

1. HAL + clock + MX init
2. USB host init
3. CAN init/start/filter setup
4. `while(1)` continuously calls `MX_USB_HOST_Process()` and translator task code in USER CODE blocks

USB host callback path:

1. Keep USB host class/event plumbing in `USB_HOST/App/usb_host.c`
2. Keep joystick report parsing and mapping logic in shared modules

CAN path:

1. Use CAN1 for translator output frames
2. Keep disconnect/timeout failsafe logic active in the transmit path

Active architecture is USB host HID to CAN translation only. Do not add PC USB device HID in the active target path.
