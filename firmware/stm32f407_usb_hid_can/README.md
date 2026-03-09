# STM32F407 USB HID CAN Bridge

This is a CubeMX-structured target for CAN1 + USB OTG FS Custom HID.

## Key Files

- `.ioc`: `STM32F407_USB_HID_CAN.ioc`
- Transport: `Core/Inc/usb_hid_transport.h`, `Core/Src/usb_hid_transport.c`
- USB HID interface: `USB_DEVICE/App/usbd_custom_hid_if.c`
- CAN bridge: `Core/Src/can.c`

## Main Loop Model

1. USB OUT callback enqueues host reports.
2. `hid_transport_process_host_reports()` executes commands in main context.
3. CAN RX ISR enqueues received frames.
4. `hid_transport_poll_tx_queue()` drains CAN queue and retries HID IN transmit when USB is busy.

## Notes

- Report IDs are required and included in each 64-byte report.
- Endpoint/report size should be kept at 64 bytes in generated USB Custom HID middleware config.
