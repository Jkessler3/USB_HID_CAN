# App Layer

Non-generated translator glue for the active target.

`joystick_translator_app.[ch]` wires:

- USB host HID device state and report parsing (`shared/usb_host_hid`)
- joystick-to-CAN mapping and fixed-rate frame scheduling (`shared/mapping`)
- output `can_frame_t` generation for target CAN TX path

This file set is intended to be called from generated code USER CODE blocks only.
