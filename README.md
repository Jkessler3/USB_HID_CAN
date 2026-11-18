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
- [Current CANaerospace identifiers and wire types](docs/CANAEROSPACE_PROTOCOL.md)
- [Hardware bring-up and receiver acceptance](docs/BRINGUP.md)
- [Windows CANaerospace mapper and offline capture replay](tools/CANaerospaceJoystickMapper/README.md)
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

## Software verification

Run the firmware host regressions from the repository root with Python 3 and a native C compiler. On Windows the runners accept a MinGW GCC installation. The tests exercise descriptors, USB transport and attachment, CAN quality and scheduling, and the text decoder.

```powershell
Get-ChildItem firmware/tests/test_*.py | ForEach-Object {
    python -B $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "Failed: $($_.Name)" }
}
```

The [mapper build](tools/CANaerospaceJoystickMapper/README.md) runs its tests and builds the Windows x64 application. Use a fresh build of this revision for bench work.
