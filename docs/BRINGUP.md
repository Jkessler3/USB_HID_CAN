# Standalone translator bring-up

The target is `firmware/targets/stm32_f407_usbhost_can_joystick`. Use [CLI build instructions](CLI_BUILD.md) and [regeneration rules](REGEN_RULES.md) for the exact firmware being qualified. Record the firmware revision, local changes, PCB revision, joystick model, descriptor, receiver version and test equipment with each capture.

## Before board power

1. Identify the board revision and inspect its condition, connector population and host data path against the schematic. Verify continuity for the intended USB host port. Distinguish the USB-A interface from J1 USB-C.
2. Verify the RJ11 pin numbering, supply voltage and polarity, ground, CAN H/L, transceiver wiring, termination, USB host VBUS source/current limit and protection against the board revision and receiver cable. Check continuity and idle resistance unpowered. Apply current-limited power and record rail and host VBUS measurements before attaching a joystick.
3. Check connector alignment, clearances, strain relief and enclosure fit in the specified TEKO Tekal 1. Record any temporary wiring and repeat electrical and mechanical qualification on the production configuration.
4. Confirm bxCAN1 PB8/PB9, USB OTG FS host and SWD against the generated project. The default `USBH_HAS_VBUS_SWITCH=0` depends on the board providing VBUS separately.

## USB and CAN acceptance

- If CAN initialization or start fails at power-up, correct the startup condition and reset the board. Test startup separately from a running bus losing its acknowledgment peer.
- Begin a timestamped external CAN capture before connecting USB. Check bus speed with the agreed receiver, standard 11-bit IDs 1500–1506 (0x5DC–0x5E2), DLC 8, node, frame intervals and independent message counters. The sender advances counters at controller enqueue. Record CAN acknowledgments, bus load, errors and worst-case frame gaps separately.
- Record the joystick's VID/PID, interface, report descriptor, endpoint size/interval and raw reports. Exercise stationary holds beyond the stale interval, all available axes at both ends and center, the hat and individual buttons. Correlate inputs to the actual wire bytes. An omitted axis is centered and an omitted button is clear. See the [supported HID layouts](../firmware/shared/usb_host_hid/README.md).
- Exercise startup with no device, attachment, enumeration, a short or unsupported report on a fixture, input silence, unplug and reconnect. Compare all four axis pairs and both button banks before and after each transition. The default failsafe centers all axes at 0x8000 and clears buttons. Check the service byte for stale, disconnected, parse error and presence independently. Confirm recovery on fresh input. Record CAN continuity during reset and enumeration with the configured debug output.
- Capture buttons 0, 7, 8, 31, 32 and 63 where available, with a fixture for controls absent from the joystick. The current transmitter uses BLONG and big-endian payloads. A later requirement specifies UCHAR4. Obtain the receiver type, byte order and accepted vectors before selecting a production button format. See the [current wire mapping](CANAEROSPACE_PROTOCOL.md).
- Add malformed input, CAN congestion, missing acknowledgment and recovery on an isolated bus. Verify neutral frames after invalidation, the backpressure indication after a missed enqueue and wraparound of counters. Agree receiver timeout and maximum frame gap, then measure with and without UART diagnostics over a sustained run.
- Replay known captures in the Windows mapper, apply a remapped profile, and compare its offline preview and decoded fields with the expected outputs. Pasted data does not establish live adapter operation or program the device.

Keep the raw descriptor, labeled USB/UART and receiver-side CAN traces, power measurements and enclosure photos together. Host regressions and successful links can be reproduced without a board. Hardware acceptance requires these observations on the identified assembly.
