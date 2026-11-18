# USB host joystick input

The STM32 target compiles the attached device's HID report descriptor once during enumeration, then passes each completed interrupt-IN transfer and its actual byte count to `usb_host_hid_decode_input`. Only a complete, valid sample updates the timestamp, sample counter and CAN control state. Unplugging clears the active layout. The board requests unchanged reports every 20 ms with SET_IDLE. Devices that decline this request must still provide fresh input within the configured stale interval for controls to remain valid.

## Supported input

- One eligible nonboot HID interface with one interrupt-IN endpoint, and one top-level Generic Desktop Joystick or Game Pad application. Physical or logical collections can be nested within it.
- One self-contained input report, unnumbered or carrying one descriptor-declared nonzero report ID. The total report, including any ID, can occupy up to 64 bytes. The decoder requires the exact declared length rounded up to bytes.
- Absolute variable X, Y, Z, Rx, Ry, Rz, Slider and Dial, in any bit position or order, with declared signed or unsigned logical ranges and field widths of 1–16 bits. The lowest, upper-midpoint and highest declared positions map to signed values −32768, 0 and 32767. A two-position axis uses the two endpoints. An omitted axis slot is neutral.
- Single-bit Button Page usages 1–64, individually declared or supplied as a usage range. Usage 1 becomes CAN button bit 0. An optional eight-direction hat uses offsets 0–7 from its logical minimum, with −1 for an out-of-range value when the descriptor declares a null state.
- Constant padding consumes its declared bits. Explicitly sized absolute variable vendor fields consume space without controlling an output. Feature and Output items have separate offsets. The parser supports bounded global push/pop.

## Unsupported layouts

A boot keyboard/mouse, ambiguous interface or endpoint, multiple input report IDs, mixed numbered/unnumbered framing, multiple top-level applications, relative or array controls, duplicate mapped usages, an unsupported data usage, or a malformed descriptor yields no usable profile. Long items, alternate usage sets, more than 64 button usages, and non-eight-direction hats are outside this profile. Independently numbered Feature or Output reports can coexist with one explicitly numbered Input report. Unknown input IDs, short/long packets and out-of-range control values do not update a good sample. The legacy manual byte-layout API is retained for existing callers. The target uses the descriptor APIs.

## Verification

Run `python firmware/tests/test_hid.py` and `python firmware/tests/test_hid_transport.py` with a local C11 compiler. The fixtures cover packed signed axes, unsigned full scale, stationary reports, nonzero IDs, button bits 0/31/32/63, independent hat data, report lengths, and unsupported devices or shapes.

To qualify a particular joystick, save its actual descriptor and labeled reports for center, extrema and individual buttons. Verify the corresponding CAN traces, stationary operation beyond the stale timeout, invalid packet handling, unplug and reconnect on the receiver. The fixtures do not establish compatibility with a specific retail device.
