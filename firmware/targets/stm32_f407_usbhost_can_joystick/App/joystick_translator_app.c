#include "joystick_translator_app.h"

#include <string.h>

void joystick_translator_app_init(joystick_translator_app_t *app, uint32_t now_ms)
{
    if (app == NULL) {
        return;
    }

    memset(app, 0, sizeof(*app));
    usb_host_hid_device_reset(&app->hid_device);
    usb_host_hid_default_layout_init(&app->hid_layout);
    usb_host_hid_joystick_state_clear(&app->joystick_state);
    joystick_can_mapping_table_init_default(&app->mapping);
    joystick_can_pipeline_reset(&app->pipeline);
    app->pipeline.last_control_tx_ms = now_ms;
    app->pipeline.last_status_tx_ms = now_ms;
}

void joystick_translator_app_on_usb_connect(joystick_translator_app_t *app,
                                            uint32_t now_ms)
{
    if (app == NULL) {
        return;
    }

    usb_host_hid_on_connection(&app->hid_device, now_ms);
    joystick_can_pipeline_on_connect(&app->pipeline, now_ms);
}

void joystick_translator_app_on_usb_disconnect(joystick_translator_app_t *app,
                                               uint32_t now_ms)
{
    if (app == NULL) {
        return;
    }

    usb_host_hid_on_disconnection(&app->hid_device, now_ms);
    joystick_can_pipeline_on_disconnect(&app->pipeline, now_ms);
}

void joystick_translator_app_on_usb_class_selected(joystick_translator_app_t *app,
                                                   uint16_t vid,
                                                   uint16_t pid,
                                                   uint8_t speed,
                                                   uint8_t interface_count,
                                                   uint32_t now_ms)
{
    if (app == NULL) {
        return;
    }

    usb_host_hid_on_class_selected(&app->hid_device, vid, pid, speed, interface_count, now_ms);
}

void joystick_translator_app_on_usb_class_active(joystick_translator_app_t *app,
                                                 uint32_t now_ms)
{
    if (app == NULL) {
        return;
    }

    usb_host_hid_on_class_active(&app->hid_device, now_ms);
}

bool joystick_translator_app_on_hid_report(joystick_translator_app_t *app,
                                           const uint8_t *report,
                                           uint16_t report_len,
                                           uint32_t now_ms)
{
    bool parsed;

    if ((app == NULL) || (report == NULL)) {
        return false;
    }

    usb_host_hid_on_report(&app->hid_device, now_ms);
    parsed = usb_host_hid_parse_report(&app->hid_layout,
                                       report,
                                       report_len,
                                       now_ms,
                                       app->hid_device.report_sequence,
                                       &app->joystick_state);
    usb_host_hid_joystick_apply_stale_timeout(&app->hid_device, now_ms, &app->joystick_state);
    if (!parsed) {
        return false;
    }

    joystick_can_pipeline_submit_sample(&app->pipeline, &app->joystick_state);
    return true;
}

bool joystick_translator_app_poll_can_frame(joystick_translator_app_t *app,
                                            uint32_t now_ms,
                                            can_frame_t *out_frame,
                                            joystick_can_tx_kind_t *out_kind)
{
    if ((app == NULL) || (out_frame == NULL) || (out_kind == NULL)) {
        return false;
    }

    if (usb_host_hid_joystick_is_stale(&app->hid_device, now_ms)) {
        app->joystick_state.stale = true;
        app->joystick_state.valid = false;
        app->joystick_state.connected = false;
        app->pipeline.connected = false;
    }

    return joystick_can_pipeline_build_next_frame(&app->mapping,
                                                  &app->pipeline,
                                                  now_ms,
                                                  out_frame,
                                                  out_kind);
}
