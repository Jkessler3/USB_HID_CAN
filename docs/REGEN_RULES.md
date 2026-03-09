# CubeMX Regeneration Rules

## Hard Rules

1. Do not place business logic in generated files except inside USER CODE blocks.
2. Keep application logic in:
   - `firmware/targets/<target>/App/`
   - `firmware/targets/<target>/Target/`
   - `firmware/shared/`
3. Keep transport protocol/packing logic out of USB middleware files.
4. Keep ISR callbacks short: enqueue only, no blocking work.

## File Ownership

Generated or generated-adjacent (edit only in USER CODE blocks when possible):

- `firmware/targets/stm32_f407_canusb_hid/Core/*`
- `firmware/targets/stm32_f407_canusb_hid/USB_DEVICE/*`
- `firmware/targets/stm32_f407_canusb_hid/Drivers/*`
- `firmware/targets/stm32_f407_canusb_hid/Middlewares/*`
- `firmware/targets/stm32_f407_canusb_hid/stm32_f407_canusb_hid.ioc`

User-owned files (safe for full edits):

- `firmware/targets/stm32_f407_canusb_hid/App/*`
- `firmware/targets/stm32_f407_canusb_hid/Target/*`
- `firmware/shared/**/*`
- `firmware/tools/**/*`

## Required Control Flow Contract

`Core/Src/main.c` must keep this pattern:

1. HAL + clock + MX init
2. `app_init()` (USER CODE block)
3. `while(1)` calls `app_loop()` (USER CODE block)

`USB_DEVICE/App/usbd_custom_hid_if.c`:

1. Keep report descriptor and callbacks
2. OUT callback enqueues report via transport API and returns quickly

CAN RX callback:

1. Convert HAL frame to shared `can_frame_t`
2. Enqueue to transport
3. Do not perform USB TX in ISR

Main app loop:

1. Process queued host commands
2. Drain CAN RX queue
3. Attempt HID IN transmit and retry later on USB busy
