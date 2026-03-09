# Repository Status (Pre-Restructure Snapshot)

Snapshot date: 2026-03-09
Workspace root: `C:\Users\jonke\USD_HID_CAN`

This file intentionally captures the state before the target/shared migration.
For current layout and paths, see `docs/TARGETS.md`.

## 1) Existing Targets

- `firmware/stm32f407_usb_hid_can`
  - Target type: STM32CubeMX/CubeIDE-style firmware folder
  - MCU: `STM32F407VGTx` (from `STM32F407_USB_HID_CAN.ioc`)
  - `.ioc`: `firmware/stm32f407_usb_hid_can/STM32F407_USB_HID_CAN.ioc`
  - Peripherals configured in `.ioc`: `CAN1`, `USB_OTG_FS` device, `USB_DEVICE` middleware `CUSTOM_HID`

Other code group:

- `can_core` (portable C module; not a full firmware target)
  - Includes portable CAN/OBD helper logic and a standalone `CMakeLists.txt`

## 2) What Builds Today and How

- `can_core/` can be built as a standalone static library via CMake:
  - `cmake -S can_core -B build/can_core`
  - `cmake --build build/can_core`
- `firmware/stm32f407_usb_hid_can/` appears scaffolded for CubeIDE/CubeMX, but does not currently include a complete generated STM32 project tree (`Drivers/`, startup/linker, project metadata), so buildability is not guaranteed in current repo state.

## 3) Comms Transports Present

- USB Custom HID transport (bidirectional intent):
  - `firmware/stm32f407_usb_hid_can/USB_DEVICE/App/usbd_custom_hid_if.c`
  - `firmware/stm32f407_usb_hid_can/Core/Src/usb_hid_transport.c`
- No active UART/CDC transport implementation detected in this snapshot.

## 4) CAN / OBD Stack and Locations

- Portable CAN/OBD helpers:
  - `can_core/include/can_core.h`
  - `can_core/src/can_core.c`
- STM32 CAN HAL integration and callback path:
  - `firmware/stm32f407_usb_hid_can/Core/Src/can.c`
- HID bridge logic and host command processing:
  - `firmware/stm32f407_usb_hid_can/Core/Src/usb_hid_transport.c`
  - `firmware/stm32f407_usb_hid_can/Core/Src/host_interface_compat.c`

## 5) Active vs Legacy-Looking Areas

Likely active:

- `firmware/stm32f407_usb_hid_can/*` (new STM32 + HID path)
- `can_core/*` (portable OBD helper extracted for reuse)

Legacy/abandoned indicators:

- No explicit legacy folders found, but there is architectural overlap between `can_core` and target-level logic (`Core/Src/can.c`, `Core/Src/usb_hid_transport.c`), suggesting transitional duplication rather than clean module boundaries.

## 6) Regeneration/Architecture Code Smells

1. Application logic in generated-risk paths:
   - Significant custom logic resides in `Core/Src/main.c`, `Core/Src/can.c`, and USB middleware-side files.
   - Current files do not consistently use STM32Cube `/* USER CODE BEGIN */` boundaries.

2. Shared vs target coupling:
   - Transport logic (`usb_hid_transport.c`) mixes protocol behavior and OBD interpretation with target integration concerns.
   - Reusable logic is split between `can_core` and target files rather than a single shared module tree.

3. Missing standard target layout:
   - Current target does not follow a `targets/<target>/{Core,Drivers,Middlewares,App,Target}` boundary model.

4. Duplication risk:
   - CAN/OBD abstractions exist both in standalone portable module (`can_core`) and target-specific implementation.

5. Build-system fragmentation:
   - `can_core` has CMake, while the STM32 target is CubeIDE/CubeMX-oriented and currently incomplete, making reproducible CI builds harder.
