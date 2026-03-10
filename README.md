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
- `docs/CLI_BUILD.md` - CLI build and clean workflow without STM32CubeIDE UI

## CLI Build

- Build:
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug`
- Clean + build:
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug -Clean`
- Optional explicit CubeIDE path (if tools are not on PATH):
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\build.ps1 -Target stm32_f407_usbhost_can_joystick -Config Debug -CubeIdeRoot "C:\ST\STM32CubeIDE_1.19.0"`
- Remove `.elf` files:
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\tools\build\clean_artifacts.ps1 -Target stm32_f407_usbhost_can_joystick`
