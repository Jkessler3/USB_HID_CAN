#ifndef JOYSTICK_TRANSLATOR_APP_H
#define JOYSTICK_TRANSLATOR_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"
#include "joystick_can_mapping.h"
#include "usb_host_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    usb_host_hid_device_info_t hid_device;
    usb_host_hid_report_layout_t hid_layout;
    usb_host_hid_joystick_state_t joystick_state;
    joystick_can_mapping_table_t mapping;
    joystick_can_pipeline_state_t pipeline;
} joystick_translator_app_t;

void joystick_translator_app_init(joystick_translator_app_t *app, uint32_t now_ms);

void joystick_translator_app_on_usb_connect(joystick_translator_app_t *app,
                                            uint32_t now_ms);

void joystick_translator_app_on_usb_disconnect(joystick_translator_app_t *app,
                                               uint32_t now_ms);

void joystick_translator_app_on_usb_class_selected(joystick_translator_app_t *app,
                                                   uint16_t vid,
                                                   uint16_t pid,
                                                   uint8_t speed,
                                                   uint8_t interface_count,
                                                   uint32_t now_ms);

void joystick_translator_app_on_usb_class_active(joystick_translator_app_t *app,
                                                 uint32_t now_ms);

bool joystick_translator_app_on_hid_report(joystick_translator_app_t *app,
                                           const uint8_t *report,
                                           uint16_t report_len,
                                           uint32_t now_ms);

bool joystick_translator_app_poll_can_frame(joystick_translator_app_t *app,
                                            uint32_t now_ms,
                                            can_frame_t *out_frame,
                                            joystick_can_tx_kind_t *out_kind);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_TRANSLATOR_APP_H */
