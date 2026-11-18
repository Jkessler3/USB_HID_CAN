# Architecture: USB Host Joystick -> CAN Translator

## Purpose

Standalone translator box:

- USB Thrustmaster joystick plugs into STM32F407 USB FS host port
- STM32 reads HID input reports
- Input is mapped to control/state CAN frames
- CAN frames are sent to the RC controller

This architecture does **not** use PC USB device HID.

## High-Level Data Flow

1. USB host stack initializes (`MX_USB_HOST_Init`).
2. USB host background polling runs in main loop (`MX_USB_HOST_Process`).
3. Host HID events/report data are converted into joystick samples.
4. Mapping layer converts joystick samples to CAN control frames.
5. The foreground scheduler submits due parameters to CAN1.
6. On invalid input, disconnect or timeout, the mapper builds neutral frames.

## Active Target

- `firmware/targets/stm32_f407_usbhost_can_joystick`
- `.ioc`: `stm32_f407_usbhost_can_joystick.ioc`
- USB mode: `USB_OTG_FS Host_Only`, HID host class
- CAN: `CAN1` enabled (`PB8/PB9`), RX interrupt configured

## Shared Modules

- `firmware/shared/core`
  - generic utility support
- `firmware/shared/can`
  - `can_frame_t` and frame helpers
- `firmware/shared/usb_host_hid`
  - USB host HID device state tracking
  - generic joystick state model + report layout parser
- `firmware/shared/mapping`
  - joystick state -> CAN control/status frame mapping
  - fixed-rate frame scheduling + timeout failsafe state handling

## HID Input Handling Strategy

During enumeration the host selects an eligible HID interface and compiles its report descriptor. Each completed interrupt-IN transfer passes its actual length to the decoder. A complete supported report updates the joystick sample and its freshness timestamp. The [HID profile](../firmware/shared/usb_host_hid/README.md) defines supported fields, report IDs and rejection behavior. Compatibility with a particular joystick requires a descriptor and labeled input captures.

## Joystick-to-CAN mapping

The default transmitter produces seven standard 11-bit CANaerospace IDs. IDs 1500–1503 carry pairs of unsigned, centered axes in big-endian order. IDs 1504–1505 carry button banks. Their current type is BLONG pending receiver confirmation of the later UCHAR4 requirement. ID 1506 carries status. Every frame includes the node, type, quality flags and per-ID message counter. See the [wire format and receiver vectors](CANAEROSPACE_PROTOCOL.md).

The foreground scheduler targets 20 ms for each control parameter and 100 ms for status. It updates each parameter's counter and deadline when bxCAN accepts a frame. USB attachment and reset advance on elapsed time so the scheduler can continue during enumeration. Arbitration, bus health and receiver timeout affect when frames arrive.

## Failsafe behavior

The default timeout is 250 ms since the last valid sample. Before the first valid report, after a rejected report, on timeout or on disconnect, the output centers axes at 0x8000 and releases all buttons. The service bits distinguish stale data, disconnection and parser errors. A fresh supported report restores control. [Bench acceptance](BRINGUP.md) includes tracing these transitions on the receiver.

## Out of Scope (By Direction)

- PC USB device HID transport
- OBD request/response protocol path
