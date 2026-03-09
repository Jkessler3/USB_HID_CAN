# stm32_f407_canusb_hid

STM32F407 target for CAN1 + USB OTG FS Custom HID bridge.

## Layout

- `stm32_f407_canusb_hid.ioc`: CubeMX source of truth
- `Core/`, `Drivers/`, `Middlewares/`, `USB_DEVICE/`: generated-or-generated-adjacent tree
- `App/`: application glue and transport logic
- `Target/`: board-specific hooks

## App Entry Points

- `App/app_main.c` provides:
  - `app_init()`
  - `app_loop()`
  - `app_tick_1ms()`

`Core/Src/main.c` calls only `app_init()` and `app_loop()` inside USER CODE blocks.
