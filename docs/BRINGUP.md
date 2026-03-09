# Bring-Up Checklist

This checklist is for the active standalone translator target:

- USB joystick (host) -> STM32F407 -> CAN
- Target path: `firmware/targets/stm32_f407_usbhost_can_joystick`

## 1) Preconditions

- Regenerated target keeps `USB_OTG_FS` in host mode with HID class enabled.
- `CAN1` remains enabled with RX interrupt path.
- External hardware is present:
  - USB connector + valid 5V VBUS switching path
  - CAN transceiver wired to `PB8/PB9`
  - Correct CAN bus termination

If the board has no GPIO-controlled VBUS switch, keep `USBH_HAS_VBUS_SWITCH` at `0` in:

- `firmware/targets/stm32_f407_usbhost_can_joystick/USB_HOST/Target/usbh_platform.h`

This makes `MX_DriverVbusFS()` a no-op so `USBH_LL_DriverVBUS()` will not toggle a nonexistent switch pin.

## 1.1) Post-Regeneration Sanity Checks

- Target still builds from `firmware/targets/stm32_f407_usbhost_can_joystick`.
- `.ioc` still has `USB_OTG_FS.VirtualMode=Host_Only`.
- `.ioc` still has `USB_HOST.VirtualModeFS=Hid`.
- `.ioc` keeps `CAN1` enabled on `PB8/PB9`.
- `.ioc` keeps `SYS` debug on SWD (`PA13/PA14`).
- `.ioc` does not introduce `PA9` USB VBUS sensing in active pinout.
- Regenerated `main.c` still calls:
  - `MX_USB_HOST_Init()`
  - `MX_CAN1_Init()`
  - `HAL_CAN_Start()`
  - `HAL_CAN_ActivateNotification(...CAN_IT_RX_FIFO0_MSG_PENDING...)`

## 2) Shared Module Configuration

1. Configure HID layout parser:
   - `usb_host_hid_report_layout_t` in `firmware/shared/usb_host_hid`
   - set report ID policy, axis fields, button/hat offsets for target joystick
2. Configure mapping:
   - `joystick_can_mapping_table_t` in `firmware/shared/mapping`
   - confirm axis maps, button maps, CAN IDs, and periods

## 3) App-Level Integration (USER CODE blocks only in generated files)

1. Initialize pipeline state in app layer:
   - `usb_host_hid_device_info_t`
   - `usb_host_hid_joystick_state_t`
   - `joystick_can_pipeline_state_t`
2. On USB events:
   - connection -> `usb_host_hid_on_connection`, `joystick_can_pipeline_on_connect`
   - disconnection -> `usb_host_hid_on_disconnection`, `joystick_can_pipeline_on_disconnect`
   - class active -> `usb_host_hid_on_class_active`
3. On HID input report:
   - `usb_host_hid_on_report`
   - `usb_host_hid_parse_report`
   - `usb_host_hid_joystick_apply_stale_timeout`
   - `joystick_can_pipeline_submit_sample`
4. In main loop:
   - call `joystick_can_pipeline_build_next_frame(...)`
   - transmit resulting `can_frame_t` through target CAN TX function

## 4) Hardware Validation Sequence

1. Power board with CAN transceiver connected.
2. Connect joystick to USB host port.
3. Confirm host state transitions (`connect` -> `class selected` -> `class active`).
4. Verify HID reports update joystick state (axes/buttons changing).
5. Verify control frame period on CAN bus.
6. Unplug joystick and verify failsafe frames are emitted.

## 5) Expected Baseline Behavior

- Before first valid HID report: failsafe active, neutral CAN outputs.
- With valid fresh reports: control frames at configured fixed rate.
- On disconnect or stale input timeout: immediate neutral command frames + failsafe flag.

## 6) Minimal Proof-of-Function Path

1. Confirm USB host detect messages:
   - connect/disconnect
   - class selected
   - VID/PID
   - HID report length / poll interval
2. Confirm raw HID bytes are printed from callback and latest report state is updated.
3. Confirm fixed-rate CAN proof frame is present on bus:
   - ID `0x321`
   - marker bytes `A5 5A`
   - payload includes USB/HID state fields.
