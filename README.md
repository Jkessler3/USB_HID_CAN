# USB-HID-CAN

Smart OBD firmware workspace structured for CubeMX regeneration and target/module separation.

## Repository Layout

- `firmware/shared/`
  - `core/`: generic support modules (`spsc_ring`)
  - `can/`: CAN frame abstraction (`can_types`)
  - `obd/`: OBD-II Mode 01 helpers
  - `protocol/`: HID report protocol definitions/packers
- `firmware/targets/stm32_f407_canusb_hid/`
  - CubeMX target (`.ioc`, `Core`, `USB_DEVICE`, `Drivers`, `Middlewares`)
  - app logic in `App/`, board hooks in `Target/`
- `firmware/tools/host_test/`
  - host-side smoke scripts
- `docs/`
  - status/inventory, target docs, regeneration rules

## HID Protocol (64-byte reports)

Byte layout (little-endian):

- `0`: `report_id`
- `1`: `type/cmd`
- `2`: `flags` (`bit0=extended`, `bit1=RTR`)
- `3`: `dlc`
- `4..7`: `can_id`
- `8..11`: `seq`
- `12..15`: `value`
- `16..23`: `data[8]`
- `24..63`: reserved

Report IDs:

- `0x01` HOST -> DEV command
- `0x81` DEV -> HOST event

Commands (`type` on `0x01`):

- `0x01` CAN TX
- `0x02` OBD request (`data[0]=mode`, `data[1]=pid`)
- `0x03` set CAN bitrate (`value=bitrate_bps`)
- `0x04` set CAN filter (`can_id=id`, `value=mask`)

Events (`type` on `0x81`):

- `0x01` CAN RX frame
- `0x02` OBD response
- `0x03` status
- `0x7F` error

Status/error codes are returned in `value`.
