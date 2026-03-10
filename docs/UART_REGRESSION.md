# UART Regression Check (Current vs Last Known Good)

Baseline used: commit `de418cd` (last revision before the current local edits and rebuild/flash cycle).

## 1) What Changed

| Area | Last Known Good (`de418cd`) | Current Working Tree | Changed |
|---|---|---|---|
| `SystemClock_Config()` | HSE on, PLL from HSE, `PLLM=8`, `PLLN=336`, `PLLP=2`, `PLLQ=7`, APB1 `/4`, APB2 `/2` | Same values | No |
| `HSE_VALUE` + PLL-related defines/settings | `HSE_VALUE=8000000U` in HAL config; IOC has `RCC.HSE_VALUE=8000000`, `PLLM=8`, `PLLN=336`, `PLLQ=7` | Same values | No |
| `MX_USART2_UART_Init()` | `115200`, `8N1`, `TX_RX`, no flow control, oversampling 16 | Same values | No |
| `HAL_UART_MspInit()` | USART2 clock on, `PA2/PA3` as AF7 USART2, USART2 IRQ enabled | Same values | No |
| USART2 TX/RX pins | IOC: `PA2=USART2_TX`, `PA3=USART2_RX` | Same values | No |
| `printf`/log retarget | `_write()` -> `HAL_UART_Transmit(&huart2, ...)` | Same code | No |
| Extra UART write in app init | Not present | Not present (restored) | No |
| UART use inside CAN RX ISR | `printf` used in ISR callback | Removed; ISR now snapshots data only | Yes (mitigation) |
| HID raw log rate | Unthrottled hex dump | Throttled (`200 ms`, first `16` bytes) | Yes (mitigation) |

Current firmware-side mitigation deltas:
- Removed direct startup `HAL_UART_Transmit("UART OK")` line from `app_init()`.
- Removed `printf` from `HAL_CAN_RxFifo0MsgPendingCallback()` ISR.
- Added deferred CAN RX log print in main loop (`app_flush_can_rx_log()`).
- Added HID raw log throttling to reduce UART bandwidth pressure.

## 2) Most Likely Reason For Garbled UART

There is no regression in UART peripheral setup or RCC/PLL values versus the last known good commit.  
Most likely firmware reason for garbled output was UART contention/overload:
- `printf` from interrupt context (CAN RX callback) competing with main-context logging.
- high-volume unthrottled HID hex dumps saturating `115200` UART.

## 3) Exact File/Line To Restore

The prior two-line startup probe was already removed.  
Current key mitigation lines:

- `firmware/targets/stm32_f407_usbhost_can_joystick/Core/Src/main.c:201`
  - deferred main-loop log handler `app_flush_can_rx_log()`
- `firmware/targets/stm32_f407_usbhost_can_joystick/Core/Src/main.c:502`
  - `_write()` guard against ISR-context UART transmission
- `firmware/targets/stm32_f407_usbhost_can_joystick/Core/Src/main.c:517`
  - CAN RX ISR no longer calls `printf`

Reference lines (unchanged and verified):
- `SystemClock_Config`: `Core/Src/main.c:357`
- `MX_USART2_UART_Init`: `Core/Src/main.c:440`
- `_write` retarget: `Core/Src/main.c:502`
- `HAL_UART_MspInit`: `Core/Src/stm32f4xx_hal_msp.c:159`
- IOC USART pins: `stm32_f407_usbhost_can_joystick.ioc:74`, `:76`
- HAL `HSE_VALUE`: `Core/Inc/stm32f4xx_hal_conf.h:99`
