# usb_host_hid

Shared USB host HID helpers for translator firmware.

Current contents:

- host connection/class state container with stale timeout tracking
- generic joystick state model (axes/hats/buttons + sequence/timestamp)
- configurable byte-aligned HID report layout parser (no mouse/keyboard assumptions)

Note: final joystick-specific HID field layout still needs board/device profile tuning.
