# mapping

Shared joystick-to-CAN mapping layer.

Current behavior:

- axis/button mapping table structures for joystick-to-CAN translation
- fixed-rate frame scheduling API (`parameter_period_ms`, `status_period_ms`)
- invalid-input, timeout and disconnect failsafe with neutral control outputs
- CANaerospace DLC-8 joystick parameter builders (`1500..1506` / `0x5DC..0x5E2`)
