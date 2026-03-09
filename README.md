# USB-HID-CAN

This repository contains:

- `can_core/`: portable CAN/OBD helpers extracted from the existing Smart OBD logic.
- `firmware/stm32f407_usb_hid_can/`: STM32F407 CubeMX-style target with CAN1 + USB FS Custom HID transport.

## STM32 HID Transport

Target path: `firmware/stm32f407_usb_hid_can`

- MCU: STM32F407
- CAN: CAN1 with RX FIFO0 interrupt + TX mailbox
- USB: USB_OTG_FS Device + USB Device middleware `CUSTOM_HID`
- Clock: 48 MHz USB clock derived from PLLQ (`HSE=8 MHz, PLLM=8, PLLN=336, PLLP=2, PLLQ=7`)

## HID Report Protocol (64 bytes total)

Byte layout (little-endian for 32-bit fields):

- `0`: `report_id`
- `1`: `type/cmd`
- `2`: `flags` (`bit0=extended`, `bit1=RTR`)
- `3`: `dlc`
- `4..7`: `can_id`
- `8..11`: `seq`
- `12..15`: `value` (`timestamp_ms` for events, bitrate/filter mask for config)
- `16..23`: `data[8]`
- `24..63`: reserved

Report IDs:

- `0x01`: HOST -> DEV command
- `0x81`: DEV -> HOST event

Commands (`type` in report ID `0x01`):

- `0x01` `HID_CMD_CAN_TX`
- `0x02` `HID_CMD_OBD_REQUEST` (`data[0]=mode`, `data[1]=pid`)
- `0x03` `HID_CMD_SET_BITRATE` (`value=bitrate_bps`)
- `0x04` `HID_CMD_SET_FILTER` (`can_id=id`, `value=mask`)

Events (`type` in report ID `0x81`):

- `0x01` `HID_EVENT_CAN_RX`
- `0x02` `HID_EVENT_OBD_RESPONSE`
- `0x03` `HID_EVENT_STATUS`
- `0x7F` `HID_EVENT_ERROR`

Status/error values are carried in `value`.

## CubeMX Regeneration Safety

- Custom logic is in dedicated files under `Core/Src` and `Core/Inc` (`usb_hid_transport.*`, `host_interface_compat.*`).
- USB OUT callback only enqueues reports; heavy work is done in main context.
- CAN RX ISR only pushes to a ring buffer; USB sends are done in polling context.
- Keep `ProjectManager.KeepUserCode=true` in the `.ioc` and avoid placing business logic in generated init blocks.
