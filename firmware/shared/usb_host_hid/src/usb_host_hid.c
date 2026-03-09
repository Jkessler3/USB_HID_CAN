#include "usb_host_hid.h"

#include <string.h>

static int16_t clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value > max_value) {
        return max_value;
    }
    if (value < min_value) {
        return min_value;
    }
    return (int16_t)value;
}

static bool read_axis_value(const uint8_t *payload,
                            uint16_t payload_len,
                            const usb_host_hid_axis_field_t *field,
                            int16_t *out_value)
{
    int32_t value;

    if ((payload == NULL) || (field == NULL) || (out_value == NULL)) {
        return false;
    }

    switch (field->encoding) {
    case USB_HOST_HID_FIELD_S8:
        if ((uint16_t)(field->offset + 1U) > payload_len) {
            return false;
        }
        value = (int8_t)payload[field->offset];
        break;
    case USB_HOST_HID_FIELD_U8:
        if ((uint16_t)(field->offset + 1U) > payload_len) {
            return false;
        }
        value = payload[field->offset];
        break;
    case USB_HOST_HID_FIELD_S16_LE:
        if ((uint16_t)(field->offset + 2U) > payload_len) {
            return false;
        }
        value = (int16_t)((uint16_t)payload[field->offset] |
                          ((uint16_t)payload[field->offset + 1U] << 8U));
        break;
    case USB_HOST_HID_FIELD_U16_LE:
        if ((uint16_t)(field->offset + 2U) > payload_len) {
            return false;
        }
        value = (uint16_t)payload[field->offset] |
                ((uint16_t)payload[field->offset + 1U] << 8U);
        break;
    case USB_HOST_HID_FIELD_UNUSED:
    default:
        value = 0;
        break;
    }

    *out_value = clamp_i16(value, field->logical_min, field->logical_max);
    return true;
}

void usb_host_hid_device_reset(usb_host_hid_device_info_t *info)
{
    if (info == NULL) {
        return;
    }

    memset(info, 0, sizeof(*info));
    info->app_state = USB_HOST_HID_APP_IDLE;
    info->stale_timeout_ms = 250U;
}

void usb_host_hid_device_set_stale_timeout(usb_host_hid_device_info_t *info,
                                           uint32_t stale_timeout_ms)
{
    if (info == NULL) {
        return;
    }

    info->stale_timeout_ms = stale_timeout_ms;
}

void usb_host_hid_on_connection(usb_host_hid_device_info_t *info,
                                uint32_t now_ms)
{
    if (info == NULL) {
        return;
    }

    info->connected = true;
    info->class_active = false;
    info->app_state = USB_HOST_HID_APP_START;
    info->last_event_ms = now_ms;
}

void usb_host_hid_on_disconnection(usb_host_hid_device_info_t *info,
                                   uint32_t now_ms)
{
    if (info == NULL) {
        return;
    }

    info->connected = false;
    info->class_active = false;
    info->app_state = USB_HOST_HID_APP_DISCONNECT;
    info->last_event_ms = now_ms;
    info->last_report_ms = now_ms;
}

void usb_host_hid_on_class_selected(usb_host_hid_device_info_t *info,
                                    uint16_t vid,
                                    uint16_t pid,
                                    uint8_t speed,
                                    uint8_t interface_count,
                                    uint32_t now_ms)
{
    if (info == NULL) {
        return;
    }

    info->connected = true;
    info->vid = vid;
    info->pid = pid;
    info->speed = speed;
    info->interface_count = interface_count;
    info->last_event_ms = now_ms;
}

void usb_host_hid_on_class_active(usb_host_hid_device_info_t *info,
                                  uint32_t now_ms)
{
    if (info == NULL) {
        return;
    }

    info->class_active = true;
    info->app_state = USB_HOST_HID_APP_READY;
    info->last_event_ms = now_ms;
}

void usb_host_hid_on_report(usb_host_hid_device_info_t *info,
                            uint32_t now_ms)
{
    if (info == NULL) {
        return;
    }

    info->last_report_ms = now_ms;
    info->report_sequence += 1U;
}

void usb_host_hid_joystick_state_clear(usb_host_hid_joystick_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

void usb_host_hid_joystick_mark_disconnected(usb_host_hid_joystick_state_t *state,
                                             uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    usb_host_hid_joystick_state_clear(state);
    state->timestamp_ms = now_ms;
    state->stale = true;
    state->connected = false;
    state->valid = false;
}

void usb_host_hid_default_layout_init(usb_host_hid_report_layout_t *layout)
{
    if (layout == NULL) {
        return;
    }

    memset(layout, 0, sizeof(*layout));
}

bool usb_host_hid_parse_report(const usb_host_hid_report_layout_t *layout,
                               const uint8_t *report,
                               uint16_t report_len,
                               uint32_t now_ms,
                               uint32_t sequence,
                               usb_host_hid_joystick_state_t *out_state)
{
    const uint8_t *payload;
    uint16_t payload_offset;
    uint8_t axis_count;
    uint8_t hat_count;
    uint8_t i;
    uint32_t buttons;

    if ((layout == NULL) || (report == NULL) || (out_state == NULL)) {
        return false;
    }
    if (layout->report_len == 0U) {
        return false;
    }

    payload_offset = layout->report_id_present ? 1U : 0U;
    if (report_len < payload_offset) {
        return false;
    }

    if (layout->report_id_present && (report[0] != layout->report_id)) {
        return false;
    }

    if (report_len < (uint16_t)(payload_offset + layout->report_len)) {
        return false;
    }

    payload = &report[payload_offset];
    usb_host_hid_joystick_state_clear(out_state);

    axis_count = layout->axis_count;
    if (axis_count > USB_HOST_HID_MAX_AXES) {
        axis_count = USB_HOST_HID_MAX_AXES;
    }
    out_state->axis_count = axis_count;

    for (i = 0U; i < axis_count; i++) {
        int16_t axis_value = 0;
        if (!read_axis_value(payload, layout->report_len, &layout->axes[i], &axis_value)) {
            return false;
        }
        out_state->axes[i] = axis_value;
    }

    buttons = 0U;
    if ((layout->buttons_bytes > 0U) &&
        (layout->buttons_bytes <= 4U) &&
        ((uint16_t)(layout->buttons_offset + layout->buttons_bytes) <= layout->report_len)) {
        for (i = 0U; i < layout->buttons_bytes; i++) {
            buttons |= ((uint32_t)payload[layout->buttons_offset + i]) << ((uint32_t)i * 8U);
        }
        out_state->button_count = (uint8_t)(layout->buttons_bytes * 8U);
    }
    out_state->buttons = buttons;

    hat_count = layout->hat_count;
    if (hat_count > USB_HOST_HID_MAX_HATS) {
        hat_count = USB_HOST_HID_MAX_HATS;
    }
    if ((hat_count > 0U) &&
        ((uint16_t)(layout->hats_offset + hat_count) <= layout->report_len)) {
        for (i = 0U; i < hat_count; i++) {
            out_state->hats[i] = (int8_t)payload[layout->hats_offset + i];
        }
        out_state->hat_count = hat_count;
    }

    out_state->connected = true;
    out_state->valid = true;
    out_state->stale = false;
    out_state->report_id = layout->report_id_present ? report[0] : 0U;
    out_state->sequence = sequence;
    out_state->timestamp_ms = now_ms;
    return true;
}

bool usb_host_hid_joystick_is_stale(const usb_host_hid_device_info_t *info,
                                    uint32_t now_ms)
{
    if (info == NULL) {
        return true;
    }

    if (!info->connected || !info->class_active) {
        return true;
    }

    if (info->stale_timeout_ms == 0U) {
        return false;
    }

    return ((now_ms - info->last_report_ms) > info->stale_timeout_ms);
}

void usb_host_hid_joystick_apply_stale_timeout(const usb_host_hid_device_info_t *info,
                                               uint32_t now_ms,
                                               usb_host_hid_joystick_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (usb_host_hid_joystick_is_stale(info, now_ms)) {
        state->stale = true;
        state->valid = false;
        state->connected = false;
    }
}
