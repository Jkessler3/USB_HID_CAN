# mapping

Shared joystick-to-CAN mapping layer.

Current behavior:

- axis/button mapping table structures for joystick-to-CAN translation
- fixed-rate frame scheduling API (`control_period_ms`, `status_period_ms`)
- timeout/disconnect failsafe with neutral control outputs
- control frame (`0x301`) + status frame (`0x302`) skeleton payload builders
