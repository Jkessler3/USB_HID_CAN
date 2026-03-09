# PIN and Peripheral Dependencies

Audit date: 2026-03-09  
Active target: `firmware/targets/stm32_f407_usbhost_can_joystick`

Scope: dependencies referenced directly by target code in `Core/Src/*` and `USB_HOST/*`.

## GPIO Dependencies

| Pin(s) | Mode | Function in code | Where referenced | Notes |
| --- | --- | --- | --- | --- |
| `PB8`, `PB9` | AF9 | `CAN1_RX`, `CAN1_TX` | `Core/Src/stm32f4xx_hal_msp.c` | Primary CAN bus pins. |
| `PA8`, `PA11`, `PA12` | AF10 | `USB_OTG_FS_SOF`, `USB_OTG_FS_DM`, `USB_OTG_FS_DP` | `USB_HOST/Target/usbh_conf.c` | USB full-speed host PHY pins. |
| `PC9` | GPIO output | USB FS VBUS drive control (`MX_DriverVbusFS`) | `USB_HOST/Target/usbh_platform.c`, `Core/Src/main.c` | Controls external VBUS switch/charge pump. Active polarity is board-dependent. |
| `PA2`, `PA3` | AF7 | `USART2_TX`, `USART2_RX` | `Core/Src/stm32f4xx_hal_msp.c` | Debug/printf UART path (`_write`). |

## Peripheral + Interrupt Dependencies

| Peripheral | Usage | Interrupt(s) | Where referenced |
| --- | --- | --- | --- |
| `CAN1` | CAN RX/TX (filter, start, notifications, TX test frame) | `CAN1_RX0_IRQn` | `Core/Src/main.c`, `Core/Src/stm32f4xx_hal_msp.c`, `Core/Src/stm32f4xx_it.c` |
| `USB_OTG_FS` (Host/HCD) | USB host stack for HID device enumeration/polling | `OTG_FS_IRQn` | `USB_HOST/Target/usbh_conf.c`, `Core/Src/stm32f4xx_it.c`, `USB_HOST/App/usb_host.c` |
| `USART2` | Debug transport for `printf` via `_write` | `USART2_IRQn` | `Core/Src/main.c`, `Core/Src/stm32f4xx_hal_msp.c`, `Core/Src/stm32f4xx_it.c` |
| `SysTick` | HAL tick used by `HAL_Delay` and host timing | `SysTick_IRQn` | `Core/Src/stm32f4xx_it.c`, HAL/USB host timing paths |

## Clock Dependencies

| Clock element | Current code dependency | Where referenced | Notes |
| --- | --- | --- | --- |
| `HSE` | System clock source is external crystal/clock | `Core/Src/main.c` (`RCC_OscInitStruct.HSEState = RCC_HSE_ON`) | Required for current PLL setup. |
| `PLLQ = 7` | USB 48 MHz generation path | `Core/Src/main.c` | Needed for valid USB FS host operation. |
| `APB1 clocking` | CAN1 timing assumptions | `Core/Src/main.c` (`CAN Prescaler/BS1/BS2`) | Current settings imply a specific CAN bitrate target. |

## Code-Referenced Functional Dependencies

- USB host state machine requires `MX_USB_HOST_Process()` to run continuously in the main loop.
- CAN RX callbacks depend on FIFO0 notifications (`CAN_IT_RX_FIFO0_MSG_PENDING`) being enabled.
- Debug logging path currently depends on blocking `HAL_UART_Transmit` in `_write`.

## Unassigned / Board-Specific GPIO Still Missing in Code

- CAN transceiver control pins (`EN`, `STB`, `SILENT`) are not assigned in the active target code.
- USB VBUS fault/overcurrent sense pin is not assigned in the active target code.
- A dedicated failsafe/status indicator GPIO is not yet defined (current LED use is debug-oriented).
