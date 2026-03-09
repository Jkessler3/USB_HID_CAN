# Migration Summary

Moved from legacy layout to `firmware/shared + firmware/targets`:

- `can_core/include/can_core.h` -> split into:
  - `firmware/shared/can/include/can_types.h`
  - `firmware/shared/obd/include/obd_mode01.h`
- `can_core/src/can_core.c` -> split into:
  - `firmware/shared/can/src/can_types.c`
  - `firmware/shared/obd/src/obd_mode01.c`
- `firmware/stm32f407_usb_hid_can/Core/Src/usb_hid_transport.c` ->
  - `firmware/targets/stm32_f407_canusb_hid/App/usb_hid_transport.c`
- `firmware/stm32f407_usb_hid_can/Core/Src/main.c` app logic ->
  - `firmware/targets/stm32_f407_canusb_hid/App/app_main.c`
- Host test script:
  - `tools/hid_obd_test.py` -> `firmware/tools/host_test/hid_obd_test.py`

Removed:

- `can_core/`
- `firmware/stm32f407_usb_hid_can/`

## Targets

### `stm32_f407_canusb_hid`

- Path: `firmware/targets/stm32_f407_canusb_hid`
- MCU: STM32F407VGTx
- `.ioc`: `firmware/targets/stm32_f407_canusb_hid/stm32_f407_canusb_hid.ioc`
- Purpose: CAN1 <-> USB Custom HID bridge for Smart OBD host communication
- Transport: bidirectional USB Custom HID (IN+OUT), 64-byte reports with report IDs

## Regenerate (CubeMX)

1. Open `firmware/targets/stm32_f407_canusb_hid/stm32_f407_canusb_hid.ioc`.
2. Keep `ProjectManager.KeepUserCode=true`.
3. Generate code into the same target directory.
4. Verify generated sources still call:
   - `app_init()` from `Core/Src/main.c` after MX init
   - `app_loop()` in `while(1)`
5. Re-apply include paths for shared modules if your CubeIDE project metadata is regenerated.

## Build Notes

- This repo contains source layout and integration modules.
- Ensure your target build includes:
  - `firmware/targets/stm32_f407_canusb_hid/App/*.c`
  - `firmware/targets/stm32_f407_canusb_hid/Target/*.c`
  - `firmware/shared/**/src/*.c`
  - generated `Core`, `Drivers`, `Middlewares`, `USB_DEVICE` sources
- Include roots should be relative, e.g.:
  - `firmware/shared/core/include`
  - `firmware/shared/can/include`
  - `firmware/shared/obd/include`
  - `firmware/shared/protocol/include`
  - `firmware/targets/stm32_f407_canusb_hid/App`
  - `firmware/targets/stm32_f407_canusb_hid/Target`
