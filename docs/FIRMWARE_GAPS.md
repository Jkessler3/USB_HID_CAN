# Firmware Gaps (Post-Regeneration)

This March 2026 note records earlier gaps. See [current bring-up](BRINGUP.md), [the supported HID layouts](../firmware/shared/usb_host_hid/README.md) and [current CAN frames](CANAEROSPACE_PROTOCOL.md) for the implemented behavior and outstanding bench work.

Status date: 2026-03-09  
Scope: active path only (`stm32_f407_usbhost_can_joystick`)

## 1) HID Parser Specifics

- Final `usb_host_hid_report_layout_t` values for the actual Thrustmaster report descriptor are not set yet.
- Current parser supports byte-aligned fields (`S8/U8/S16/U16`) only; bit-packed fields may need an additional extractor.
- Multi-report-ID joystick variants need exact per-report layout selection logic in app glue.
- Generated target callback path in `Core/Src/main.c` is still mouse/keyboard-oriented and must be replaced with app-layer report handoff in USER CODE blocks.
- The STM32Cube HID host class in this target is boot-protocol oriented; true non-boot joystick support may require middleware-level HID class changes.

## 2) Final CAN Payload Layout

- `joystick_can_pipeline_build_next_frame()` now emits CANaerospace joystick parameters. Remaining protocol work should focus on bench validation against the receiver and any client-assigned identifier changes.
- Exact RC controller contract is still pending:
  - final byte/bit assignments
  - signed scaling and units per channel
  - checksum/counter policy (if required by receiver)
- Final CAN IDs and standard vs extended frame choice are still provisional defaults.

## 3) Timing / Rate Assumptions

- Default rates (`control_period_ms=20`, `status_period_ms=100`) are placeholders.
- Required end-to-end latency/jitter budget for RC control path is not yet confirmed.
- CAN bus loading impact versus chosen frame rate is not yet validated on the full network.

## 4) Failsafe Behavior

- Failsafe currently drives neutral outputs with a fail flag.
- Remaining decisions:
  - whether to hold last command briefly before neutraling
  - whether to emit dedicated fault/status frame on every cycle while disconnected
  - timeout threshold tuning for noisy USB links
  - reconnect debounce policy before exiting failsafe

## 5) Board-Controlled GPIO Not Yet Assigned

- CAN transceiver control pins (`EN`/`STB`/`SILENT`) are not assigned in active code.
- USB VBUS fault/overcurrent input pin is not assigned in active code.
- Dedicated user status/failsafe LED policy is not finalized (debug LEDs only today).
