#ifndef USB_HOST_HID_H
#define USB_HOST_HID_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_HOST_HID_MAX_AXES 8U
#define USB_HOST_HID_MAX_HATS 2U
#define USB_HOST_HID_MAX_REPORT_LEN 64U

typedef enum {
    USB_HOST_HID_APP_IDLE = 0,
    USB_HOST_HID_APP_START,
    USB_HOST_HID_APP_READY,
    USB_HOST_HID_APP_DISCONNECT
} usb_host_hid_app_state_t;

typedef struct {
    bool connected;
    bool class_active;
    uint16_t vid;
    uint16_t pid;
    uint8_t speed;
    uint8_t interface_count;
    uint32_t last_event_ms;
    uint32_t last_report_ms;
    uint32_t report_sequence;
    uint32_t stale_timeout_ms;
    usb_host_hid_app_state_t app_state;
} usb_host_hid_device_info_t;

typedef struct {
    bool connected;
    bool valid;
    bool stale;
    uint8_t report_id;
    uint8_t axis_count;
    uint8_t hat_count;
    uint8_t button_count;
    int16_t axes[USB_HOST_HID_MAX_AXES];
    int8_t hats[USB_HOST_HID_MAX_HATS];
    uint32_t buttons;
    uint32_t sequence;
    uint32_t timestamp_ms;
} usb_host_hid_joystick_state_t;

typedef enum {
    USB_HOST_HID_FIELD_UNUSED = 0,
    USB_HOST_HID_FIELD_S8,
    USB_HOST_HID_FIELD_U8,
    USB_HOST_HID_FIELD_S16_LE,
    USB_HOST_HID_FIELD_U16_LE
} usb_host_hid_field_encoding_t;

typedef struct {
    usb_host_hid_field_encoding_t encoding;
    uint8_t offset;
    int16_t logical_min;
    int16_t logical_max;
} usb_host_hid_axis_field_t;

typedef struct {
    bool report_id_present;
    uint8_t report_id;
    uint8_t report_len;
    uint8_t axis_count;
    usb_host_hid_axis_field_t axes[USB_HOST_HID_MAX_AXES];
    uint8_t buttons_offset;
    uint8_t buttons_bytes;
    uint8_t hats_offset;
    uint8_t hat_count;
} usb_host_hid_report_layout_t;

void usb_host_hid_device_reset(usb_host_hid_device_info_t *info);
void usb_host_hid_device_set_stale_timeout(usb_host_hid_device_info_t *info,
                                           uint32_t stale_timeout_ms);

void usb_host_hid_on_connection(usb_host_hid_device_info_t *info,
                                uint32_t now_ms);

void usb_host_hid_on_disconnection(usb_host_hid_device_info_t *info,
                                   uint32_t now_ms);

void usb_host_hid_on_class_selected(usb_host_hid_device_info_t *info,
                                    uint16_t vid,
                                    uint16_t pid,
                                    uint8_t speed,
                                    uint8_t interface_count,
                                    uint32_t now_ms);

void usb_host_hid_on_class_active(usb_host_hid_device_info_t *info,
                                  uint32_t now_ms);

void usb_host_hid_on_report(usb_host_hid_device_info_t *info,
                            uint32_t now_ms);

void usb_host_hid_joystick_state_clear(usb_host_hid_joystick_state_t *state);
void usb_host_hid_joystick_mark_disconnected(usb_host_hid_joystick_state_t *state,
                                              uint32_t now_ms);

void usb_host_hid_default_layout_init(usb_host_hid_report_layout_t *layout);

bool usb_host_hid_parse_report(const usb_host_hid_report_layout_t *layout,
                               const uint8_t *report,
                               uint16_t report_len,
                               uint32_t now_ms,
                               uint32_t sequence,
                               usb_host_hid_joystick_state_t *out_state);

bool usb_host_hid_joystick_is_stale(const usb_host_hid_device_info_t *info,
                                    uint32_t now_ms);

void usb_host_hid_joystick_apply_stale_timeout(const usb_host_hid_device_info_t *info,
                                               uint32_t now_ms,
                                               usb_host_hid_joystick_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* USB_HOST_HID_H */
