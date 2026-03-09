# IOC Comparison (Current Two Targets)

Compared files:

- `firmware/targets/USB HID MAUSE/USB HID MAUSE.ioc` (P1)
- `firmware/targets/USB HOST HID MOUSE MICE STM32F407 ILI9341 FSMC TFT/USB HID MAUSE.ioc` (P2)

## Config Table

| Item | P1: USB HID MAUSE | P2: USB HOST HID MOUSE MICE STM32F407 ILI9341 FSMC TFT | Notes |
|---|---|---|---|
| MCU part + package | `Mcu.CPN=STM32F407VGT6`, `Mcu.UserName=STM32F407VGTx`, `Mcu.Package=LQFP100` | `Mcu.CPN=STM32F407VGT6`, `Mcu.UserName=STM32F407VGTx`, `Mcu.Package=LQFP100` | Both are STM32F407VG LQFP100 variants. |
| USB config | `USB_OTG_FS.VirtualMode=Host_Only`, `USB_HOST` enabled | `USB_OTG_FS.VirtualMode=Host_Only`, `USB_HOST` enabled | Both are USB host projects, not USB device projects. |
| USB device middleware (`USB_DEVICE`) | Not present | Not present | Neither has `USB_DEVICE` middleware enabled. |
| USB class | `USB_HOST.VirtualModeFS=Hid` (host HID class) | `USB_HOST.VirtualModeFS=Hid` (host HID class) | Neither has `CUSTOM_HID` device class nor CDC device class. |
| CAN peripheral | `CAN1` enabled | Not enabled | P1 includes CAN; P2 does not. |
| CAN bitrate/timing | `CalculateBaudRate=500000`, `Prescaler=6`, `BS1=11TQ`, `BS2=2TQ`, `SJW=1TQ` | N/A | P1 is configured for 500 kbps CAN. |
| CAN RX interrupt/callback readiness | `NVIC.CAN1_RX0_IRQn=true` | N/A | P1 has RX0 IRQ enabled in IOC. |
| CAN filter mode | Not declared in IOC | N/A | IOC does not carry runtime CAN filter setup. In generated P1 code, filter is ID-mask (`CAN_FILTERMODE_IDMASK`). |
| Clocking / USB 48 MHz | `PLLM=8`, `PLLN=336`, `PLLQ=7`, `PLLQCLKFreq_Value=48000000` | `PLLM=8`, `PLLN=336`, `PLLQ=7`, `PLLQCLKFreq_Value=48000000` | Both provide valid 48 MHz from PLLQ. |
| HSE/HSI usage | `HSE_VALUE=8000000`, `HSI_VALUE=16000000` | `HSE_VALUE=8000000`, `HSI_VALUE=16000000` | Generated code in both uses HSE as PLL source. |
| USB pins | `PA11=USB_OTG_FS_DM`, `PA12=USB_OTG_FS_DP`, `PA8=USB_OTG_FS_SOF` | `PA11=USB_OTG_FS_DM`, `PA12=USB_OTG_FS_DP`, `PA9=USB_OTG_FS_VBUS` | Both are wired for FS host signals. |
| CAN pins | `PB8=CAN1_RX`, `PB9=CAN1_TX` | Not configured | P1 has CAN pins mapped; P2 does not. |
| Transceiver EN/STB pins | No explicit EN/STB labels in IOC | No explicit CAN EN/STB labels (PD12 labeled `RST` for display reset) | Neither IOC shows a clear CAN transceiver enable/standby pin mapping. |
| Debug SWD pins | No explicit `SYS.Debug`/PA13/PA14 entries | No explicit `SYS.Debug`/PA13/PA14 entries | SWD is not explicitly declared in either IOC. |
| CubeMX + FW + toolchain | CubeMX `6.15.0`, FW `STM32Cube FW_F4 V1.28.3`, toolchain `STM32CubeIDE` | CubeMX `6.6.1`, FW `STM32Cube FW_F4 V1.27.1`, toolchain `STM32CubeIDE` | P1 is on newer CubeMX/FW package. |

## File Completeness Check

Scope: target folder content and generated project structure.

- P1 non-Debug file count: `128`
- P2 non-Debug file count: `169`
- P1 total file count: `280`
- P2 total file count: `293`

Observations:

- P2 has more non-Debug files because it includes additional display/FSMC assets (`ILI9341.c/.h`, FSMC config path).
- P1 includes CAN-related integration (CAN IRQ in IOC and generated code path), which is more relevant to Smart OBD/CAN bridging.
- Neither target includes `USB_DEVICE` folder/class setup (both are USB host-stack projects).
- P2 includes a `.metadata` folder, which is IDE workspace noise rather than target firmware source.

## Recommended Pick

Recommended base: **P1 (`firmware/targets/USB HID MAUSE`)**.

Reasons:

1. It already has `CAN1` enabled and pinned (`PB8/PB9`) with RX IRQ configured.
2. It includes CAN timing at 500 kbps and generated CAN init/start notification path.
3. It is on newer CubeMX/F4 firmware package than P2.
4. P2 is more display/FSMC-focused and lacks CAN entirely.

## Missing Checklist In The Other File (P2) To Match P1 + Project Goal

If you were to use P2, it is missing these key items:

- [ ] Enable `CAN1` peripheral in IOC.
- [ ] Map CAN pins (`PB8=CAN1_RX`, `PB9=CAN1_TX` or board-specific alternate mapping).
- [ ] Enable `NVIC.CAN1_RX0_IRQn`.
- [ ] Configure CAN nominal timing/bitrate (e.g., 500 kbps baseline).
- [ ] Add CAN filter init strategy (runtime setup in code).
- [ ] Remove/disable unrelated FSMC/ILI9341 pieces unless required.

Additionally, for **both** P1 and P2 to satisfy your HID-CAN device architecture:

- [ ] Switch `USB_OTG_FS` from `Host_Only` to `Device` mode.
- [ ] Enable `USB_DEVICE` middleware.
- [ ] Select `CUSTOM_HID` class (bidirectional IN+OUT reports).
- [ ] Add/confirm explicit debug config (`SYS.Debug=Serial Wire`) in IOC.
- [ ] Add/confirm CAN transceiver EN/STB pin labeling and behavior.
