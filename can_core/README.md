# CAN Core (Portable)

This directory contains the minimum CAN/OBD helpers extracted into a portable,
ESP-IDF-free C module so it can be reused in another project (for example,
STM32 + USB HID to CAN bridge).

## What this includes

- Generic CAN frame type (`can_core_frame_t`)
- OBD-II Mode 01 request builder (`0x7DF`)
- Single-frame OBD response matcher (`0x7E8`..`0x7EF`)
- Basic PID decoders:
  - `0x0C` RPM
  - `0x0D` vehicle speed
  - `0x05` coolant temperature

## Layout

- `include/can_core.h`
- `src/can_core.c`
- `CMakeLists.txt` (optional standalone build)

## Typical integration (STM32 project)

1. Add `include/` to your compiler include path.
2. Compile `src/can_core.c` into your firmware/library.
3. Convert your CAN HAL frame struct to/from `can_core_frame_t`.
4. Use:
   - `can_obd_build_mode01_request()` to send requests
   - `can_obd_extract_mode01_payload()` to filter/parse responses
   - decode helpers for PID payloads

## Notes

- This module assumes 11-bit standard OBD CAN IDs for basic Mode 01 polling.
- Multi-frame diagnostics (ISO-TP) are intentionally out of scope here.
- Existing `firmware/drivers` and `firmware/services` module boundaries are
  unchanged; this folder is additive for reuse in other repos.
