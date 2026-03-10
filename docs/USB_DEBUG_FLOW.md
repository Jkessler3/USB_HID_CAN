# USB Debug Flow

This target now emits staged USB host logs and keeps a compact debug snapshot in RAM:

- target: `firmware/targets/stm32_f407_usbhost_can_joystick`
- files:
  - `USB_HOST/App/usb_host.c`
  - `USB_HOST/App/usb_host.h`
  - `USB_HOST/Target/usbh_conf.c` (VBUS driver diagnostics)

## 1) Event Logs

`USBH_UserProcess()` now logs distinct events:

- `HOST_USER_CONNECTION`
- `HOST_USER_DISCONNECTION`
- `HOST_USER_SET_CONFIGURATION` (mapped to Cube event `HOST_USER_SELECT_CONFIGURATION`)
- `HOST_USER_CLASS_SELECTED`
- `HOST_USER_CLASS_ACTIVE`
- `HOST_USER_UNRECOVERED_ERROR`

Example:

```text
USB EVT HOST_USER_CONNECTION id=4 t=1234
USB EVT HOST_USER_SET_CONFIGURATION id=1 t=1240
USB EVT HOST_USER_CLASS_SELECTED id=3 t=1260
USB EVT HOST_USER_CLASS_ACTIVE id=2 t=1280
```

## 2) Enumeration Detail Log

On `set configuration`, `class selected`, and `class active`, the firmware prints:

- device speed
- VID/PID
- device class/subclass/protocol
- interface class/subclass/protocol
- endpoint count
- HID report descriptor length
- HID poll interval

Example:

```text
USB ENUM class_active spd=1(FULL) vid=044F pid=B10A dev=00/00/00 if=03/00/00 ep=1 hid_rep=52 poll=8
```

## 3) RAM Debug Status (SWD Watch)

The latest compact status is always updated in RAM, even if UART output is unavailable:

- symbol: `g_usb_host_debug_status`
- type: `usb_host_debug_status_t`

Fields:

- `last_app_state`
- `last_core_host_state`
- `last_enum_step`
- `last_error_code`
- `last_vid`
- `last_pid`
- `last_update_ms`

Error code values:

- `0` = `USB_HOST_DBG_FAIL_NONE`
- `1` = `USB_HOST_DBG_FAIL_DISCONNECT`
- `2` = `USB_HOST_DBG_FAIL_UNRECOVERED`
- `3` = `USB_HOST_DBG_FAIL_OTHER`

## 4) VBUS Driver Log

`USBH_LL_DriverVBUS()` prints compact power-driver diagnostics:

```text
USB VBUS drv n=... state=... sw=... pc9=... t=... g=... e=... r=... c=... d=...
```

Useful fields:

- `state`: requested VBUS state (0/1)
- `sw`: `USBH_HAS_VBUS_SWITCH` configuration
- `pc9`: current `PC9` level (if mapped)
- `g/e/r`: host core state, enum step, request state

## 5) Quick Interpretation

Healthy attach path:

1. `HOST_USER_CONNECTION`
2. `HOST_USER_SET_CONFIGURATION`
3. `HOST_USER_CLASS_SELECTED`
4. `HOST_USER_CLASS_ACTIVE`

Failing attach loop (power/signal/enumeration issue):

1. `HOST_USER_CONNECTION`
2. repeated disconnect or unrecovered error before `CLASS_ACTIVE`
