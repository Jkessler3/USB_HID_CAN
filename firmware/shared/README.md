# Shared Modules

Reusable firmware modules for the standalone translator live here.

- `core/`: generic utility components (`spsc_ring`)
- `can/`: common CAN data structures
- `usb_host_hid/`: USB host HID state + generic joystick report parsing helpers
- `mapping/`: joystick-to-CAN mapping tables + fixed-rate/failsafe frame generation
