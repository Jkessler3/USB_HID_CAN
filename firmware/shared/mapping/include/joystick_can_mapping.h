#ifndef JOYSTICK_CAN_MAPPING_H
#define JOYSTICK_CAN_MAPPING_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"
#include "usb_host_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JOYSTICK_CAN_CONTROL_ID 0x301U
#define JOYSTICK_CAN_STATUS_ID  0x302U
#define JOYSTICK_CAN_MAX_AXIS_MAPS USB_HOST_HID_MAX_AXES
#define JOYSTICK_CAN_MAX_BUTTON_MAPS 16U

#define JOYSTICK_CAN_STATUS_JOYSTICK_PRESENT        0x01U
#define JOYSTICK_CAN_STATUS_HID_SAMPLE_VALID        0x02U
#define JOYSTICK_CAN_STATUS_DATA_STALE              0x04U
#define JOYSTICK_CAN_STATUS_TX_BACKPRESSURE_SEEN    0x08U
#define JOYSTICK_CAN_STATUS_HID_PARSE_ERROR         0x10U
#define JOYSTICK_CAN_STATUS_USB_DISCONNECTED        0x20U

typedef struct {
    uint8_t axis_index;
    uint8_t signal_index;
    int16_t input_min;
    int16_t input_max;
    int16_t output_min;
    int16_t output_max;
    uint16_t deadzone;
    bool invert;
} joystick_can_axis_map_t;

typedef struct {
    uint8_t button_index;
    uint8_t out_byte_index;
    uint8_t out_bit_mask;
    bool active_high;
} joystick_can_button_map_t;

typedef struct {
    uint32_t control_can_id;
    uint32_t status_can_id;
    uint16_t control_period_ms;
    uint16_t status_period_ms;
    uint16_t stale_timeout_ms;
    uint8_t axis_map_count;
    joystick_can_axis_map_t axis_maps[JOYSTICK_CAN_MAX_AXIS_MAPS];
    uint8_t button_map_count;
    joystick_can_button_map_t button_maps[JOYSTICK_CAN_MAX_BUTTON_MAPS];
} joystick_can_mapping_table_t;

typedef enum {
    JOYSTICK_CAN_SIGNAL_STEER = 0,
    JOYSTICK_CAN_SIGNAL_THROTTLE,
    JOYSTICK_CAN_SIGNAL_AUX0,
    JOYSTICK_CAN_SIGNAL_AUX1,
    JOYSTICK_CAN_SIGNAL_COUNT
} joystick_can_signal_t;

typedef enum {
    JOYSTICK_CAN_TX_NONE = 0,
    JOYSTICK_CAN_TX_CONTROL,
    JOYSTICK_CAN_TX_STATUS
} joystick_can_tx_kind_t;

typedef struct {
    bool connected;
    bool failsafe_active;
    bool has_sample;
    bool can_tx_backpressure_seen;
    bool hid_parse_error;
    bool usb_disconnected;
    uint32_t can_tx_backpressure_count;
    uint32_t last_report_ms;
    uint32_t last_control_tx_ms;
    uint32_t last_status_tx_ms;
    uint32_t frame_sequence;
    usb_host_hid_joystick_state_t sample;
} joystick_can_pipeline_state_t;

void joystick_can_mapping_table_init_default(joystick_can_mapping_table_t *table);
void joystick_can_pipeline_reset(joystick_can_pipeline_state_t *state);

void joystick_can_pipeline_on_connect(joystick_can_pipeline_state_t *state,
                                      uint32_t now_ms);

void joystick_can_pipeline_on_disconnect(joystick_can_pipeline_state_t *state,
                                         uint32_t now_ms);

void joystick_can_pipeline_submit_sample(joystick_can_pipeline_state_t *state,
                                         const usb_host_hid_joystick_state_t *sample);

void joystick_can_pipeline_note_tx_backpressure(joystick_can_pipeline_state_t *state);

void joystick_can_pipeline_note_hid_parse_error(joystick_can_pipeline_state_t *state);

bool joystick_can_pipeline_is_failsafe(const joystick_can_mapping_table_t *table,
                                       const joystick_can_pipeline_state_t *state,
                                       uint32_t now_ms);

bool joystick_can_pipeline_build_next_frame(const joystick_can_mapping_table_t *table,
                                            joystick_can_pipeline_state_t *state,
                                            uint32_t now_ms,
                                            can_frame_t *out_frame,
                                            joystick_can_tx_kind_t *out_kind);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_CAN_MAPPING_H */
