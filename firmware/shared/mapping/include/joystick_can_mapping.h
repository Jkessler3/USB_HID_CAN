#ifndef JOYSTICK_CAN_MAPPING_H
#define JOYSTICK_CAN_MAPPING_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"
#include "usb_host_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JOYSTICK_CANAEROSPACE_NODE_ID_DEFAULT 1U
#define JOYSTICK_CANAEROSPACE_AXIS_XY_ID      1500U
#define JOYSTICK_CANAEROSPACE_AXIS_Z_RX_ID    1501U
#define JOYSTICK_CANAEROSPACE_AXIS_RY_RZ_ID   1502U
#define JOYSTICK_CANAEROSPACE_AXIS_SLIDER_DIAL_ID 1503U
#define JOYSTICK_CANAEROSPACE_BUTTONS_0_31_ID 1504U
#define JOYSTICK_CANAEROSPACE_BUTTONS_32_63_ID 1505U
#define JOYSTICK_CANAEROSPACE_STATUS_ID       1506U
#define JOYSTICK_CANAEROSPACE_PARAM_COUNT     7U

#ifdef LEGACY_PROPRIETARY_CAN_DEBUG
#define JOYSTICK_CAN_CONTROL_ID 0x301U
#define JOYSTICK_CAN_STATUS_ID  0x302U
#endif
#define JOYSTICK_CAN_MAX_AXIS_MAPS USB_HOST_HID_MAX_AXES
#define JOYSTICK_CAN_MAX_BUTTON_MAPS 16U

#define JOYSTICK_CANAEROSPACE_SERVICE_VALID                 0x00U
#define JOYSTICK_CANAEROSPACE_SERVICE_DATA_STALE            0x01U
#define JOYSTICK_CANAEROSPACE_SERVICE_USB_DISCONNECTED      0x02U
#define JOYSTICK_CANAEROSPACE_SERVICE_HID_PARSE_ERROR       0x04U
#define JOYSTICK_CANAEROSPACE_SERVICE_TX_BACKPRESSURE_SEEN  0x08U
#define JOYSTICK_CANAEROSPACE_SERVICE_JOYSTICK_NOT_PRESENT  0x10U
#define JOYSTICK_CANAEROSPACE_SERVICE_MAPPING_ERROR         0x20U

#define JOYSTICK_CANAEROSPACE_TYPE_BLONG   0x05U
#define JOYSTICK_CANAEROSPACE_TYPE_USHORT2 0x0DU
#define JOYSTICK_CANAEROSPACE_TYPE_UCHAR4  0x10U

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
    uint8_t node_id;
    uint32_t parameter_can_ids[JOYSTICK_CANAEROSPACE_PARAM_COUNT];
    uint16_t parameter_period_ms;
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
    JOYSTICK_CAN_TX_AXIS_XY,
    JOYSTICK_CAN_TX_AXIS_Z_RX,
    JOYSTICK_CAN_TX_AXIS_RY_RZ,
    JOYSTICK_CAN_TX_AXIS_SLIDER_DIAL,
    JOYSTICK_CAN_TX_BUTTONS_0_31,
    JOYSTICK_CAN_TX_BUTTONS_32_63,
    JOYSTICK_CAN_TX_STATUS
} joystick_can_tx_kind_t;

typedef struct {
    bool connected;
    bool joystick_present;
    bool has_sample;
    bool can_tx_backpressure_pending;
    bool hid_parse_error;
    uint32_t can_tx_backpressure_count;
    uint32_t last_report_ms;
    uint32_t last_parameter_tx_ms[JOYSTICK_CANAEROSPACE_PARAM_COUNT];
    uint32_t last_status_tx_ms;
    uint32_t frame_sequence;
    uint8_t report_age_bucket;
    uint8_t next_parameter_index;
    uint8_t message_codes[JOYSTICK_CANAEROSPACE_PARAM_COUNT];
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

/* Input quality is independent of transport diagnostics such as backpressure. */
uint8_t joystick_can_pipeline_hid_quality(const joystick_can_mapping_table_t *table,
                                          joystick_can_pipeline_state_t *state,
                                          uint32_t now_ms);

bool joystick_can_pipeline_is_failsafe(const joystick_can_mapping_table_t *table,
                                       joystick_can_pipeline_state_t *state,
                                       uint32_t now_ms);

/* Both queries observe elapsed time. A timed-out sample stays stale until replaced.
 * Preparing a frame leaves all transmit counters and deadlines unchanged. */
bool joystick_can_pipeline_build_next_frame(const joystick_can_mapping_table_t *table,
                                            joystick_can_pipeline_state_t *state,
                                            uint32_t now_ms,
                                            can_frame_t *out_frame,
                                            joystick_can_tx_kind_t *out_kind);

/* Bit 0 excludes the first parameter (X/Y), through bit 6 for status. */
bool joystick_can_pipeline_prepare_available(const joystick_can_mapping_table_t *table,
                                             joystick_can_pipeline_state_t *state,
                                             uint32_t now_ms,
                                             uint32_t unavailable_parameters,
                                             can_frame_t *out_frame,
                                             joystick_can_tx_kind_t *out_kind);

/* Call exactly once after the CAN driver accepts the prepared frame. */
void joystick_can_pipeline_commit_frame(joystick_can_pipeline_state_t *state,
                                        joystick_can_tx_kind_t kind,
                                        uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_CAN_MAPPING_H */
