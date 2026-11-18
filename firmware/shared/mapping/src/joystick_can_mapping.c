#include "joystick_can_mapping.h"

#include <string.h>

typedef enum {
    CA_PARAM_AXIS_XY = 0,
    CA_PARAM_AXIS_Z_RX,
    CA_PARAM_AXIS_RY_RZ,
    CA_PARAM_AXIS_SLIDER_DIAL,
    CA_PARAM_BUTTONS_0_31,
    CA_PARAM_BUTTONS_32_63,
    CA_PARAM_STATUS
} ca_param_index_t;

static bool period_elapsed(uint32_t now_ms, uint32_t last_ms, uint16_t period_ms)
{
    if (period_ms == 0U) {
        return true;
    }
    return ((now_ms - last_ms) >= period_ms);
}

static uint16_t axis_to_u16(const joystick_can_pipeline_state_t *state, uint8_t axis_index, bool failsafe)
{
    int32_t value;

    if ((state == NULL) || failsafe || !state->has_sample || !state->sample.valid ||
        (axis_index >= state->sample.axis_count)) {
        return 32768U;
    }

    value = (int32_t)state->sample.axes[axis_index] + 32768L;
    if (value < 0L) {
        value = 0L;
    } else if (value > 65535L) {
        value = 65535L;
    }
    return (uint16_t)value;
}

static uint32_t buttons_to_blong(const joystick_can_pipeline_state_t *state,
                                 uint8_t first_button,
                                 bool failsafe)
{
    if (failsafe || first_button >= state->sample.button_count) {
        return 0UL;
    }

    uint32_t bank = (uint32_t)((uint64_t)state->sample.buttons >> first_button);
    uint8_t count = state->sample.button_count - first_button;
    if (count < 32U) {
        bank &= (UINT32_C(1) << count) - 1U;
    }
    return bank;
}

static bool update_sample_age(const joystick_can_mapping_table_t *table,
                              joystick_can_pipeline_state_t *state,
                              uint32_t now_ms)
{
    uint32_t age_ms = now_ms - state->last_report_ms;
    uint8_t bucket = age_ms >= 2550U ? 255U : (uint8_t)(age_ms / 10U);
    if (bucket > state->report_age_bucket) {
        state->report_age_bucket = bucket;
    }
    /* A continuously attached silent device can outlive the 32-bit tick epoch. */
    if (table->stale_timeout_ms != 0U && age_ms > table->stale_timeout_ms) {
        state->sample.stale = true;
    }
    return !state->connected || !state->has_sample || !state->sample.valid || state->sample.stale;
}

uint8_t joystick_can_pipeline_hid_quality(const joystick_can_mapping_table_t *table,
                                          joystick_can_pipeline_state_t *state,
                                          uint32_t now_ms)
{
    uint8_t service = JOYSTICK_CANAEROSPACE_SERVICE_VALID;

    if (table == NULL || state == NULL) {
        return JOYSTICK_CANAEROSPACE_SERVICE_USB_DISCONNECTED |
               JOYSTICK_CANAEROSPACE_SERVICE_JOYSTICK_NOT_PRESENT |
               JOYSTICK_CANAEROSPACE_SERVICE_DATA_STALE;
    }
    if (!state->connected) {
        service |= JOYSTICK_CANAEROSPACE_SERVICE_USB_DISCONNECTED;
    }
    if (!state->joystick_present) {
        service |= JOYSTICK_CANAEROSPACE_SERVICE_JOYSTICK_NOT_PRESENT;
    }

    if (update_sample_age(table, state, now_ms)) {
        service |= JOYSTICK_CANAEROSPACE_SERVICE_DATA_STALE;
    }

    if (state->hid_parse_error) {
        service |= JOYSTICK_CANAEROSPACE_SERVICE_HID_PARSE_ERROR;
    }

    return service;
}

static void write_header(const joystick_can_mapping_table_t *table,
                         const joystick_can_pipeline_state_t *state,
                         ca_param_index_t param,
                         uint8_t data_type,
                         uint8_t service,
                         can_frame_t *out_frame)
{
    can_frame_clear(out_frame);
    out_frame->id = table->parameter_can_ids[param];
    out_frame->extended = false;
    out_frame->rtr = false;
    out_frame->dlc = 8U;
    out_frame->data[0] = table->node_id;
    out_frame->data[1] = data_type;
    out_frame->data[2] = service;
    out_frame->data[3] = state->message_codes[param];
}

static void write_u16_be(uint8_t *data, uint8_t offset, uint16_t value)
{
    data[offset] = (uint8_t)((value >> 8U) & 0xFFU);
    data[offset + 1U] = (uint8_t)(value & 0xFFU);
}

static void write_u32_be(uint8_t *data, uint8_t offset, uint32_t value)
{
    data[offset] = (uint8_t)((value >> 24U) & 0xFFU);
    data[offset + 1U] = (uint8_t)((value >> 16U) & 0xFFU);
    data[offset + 2U] = (uint8_t)((value >> 8U) & 0xFFU);
    data[offset + 3U] = (uint8_t)(value & 0xFFU);
}

static void write_axis_pair(const joystick_can_mapping_table_t *table,
                            const joystick_can_pipeline_state_t *state,
                            ca_param_index_t param,
                            uint8_t axis_a,
                            uint8_t axis_b,
                            bool failsafe,
                            uint8_t service,
                            can_frame_t *out_frame)
{
    write_header(table,
                 state,
                 param,
                 JOYSTICK_CANAEROSPACE_TYPE_USHORT2,
                 service,
                 out_frame);
    write_u16_be(out_frame->data, 4U, axis_to_u16(state, axis_a, failsafe));
    write_u16_be(out_frame->data, 6U, axis_to_u16(state, axis_b, failsafe));
}

static void write_buttons(const joystick_can_mapping_table_t *table,
                          const joystick_can_pipeline_state_t *state,
                          ca_param_index_t param,
                          uint8_t first_button,
                          bool failsafe,
                          uint8_t service,
                          can_frame_t *out_frame)
{
    write_header(table,
                 state,
                 param,
                 JOYSTICK_CANAEROSPACE_TYPE_BLONG,
                 service,
                 out_frame);
    write_u32_be(out_frame->data, 4U, buttons_to_blong(state, first_button, failsafe));
}

static void write_status(const joystick_can_mapping_table_t *table,
                         const joystick_can_pipeline_state_t *state,
                         uint8_t service,
                         can_frame_t *out_frame)
{
    write_header(table,
                 state,
                 CA_PARAM_STATUS,
                 JOYSTICK_CANAEROSPACE_TYPE_UCHAR4,
                 service,
                 out_frame);
    out_frame->data[4] = service;
    out_frame->data[5] = (uint8_t)(state->sample.sequence & 0xFFU);
    out_frame->data[6] = (uint8_t)(state->frame_sequence & 0xFFU);
    out_frame->data[7] = state->report_age_bucket;
}

void joystick_can_mapping_table_init_default(joystick_can_mapping_table_t *table)
{
    if (table == NULL) {
        return;
    }

    memset(table, 0, sizeof(*table));
    table->node_id = JOYSTICK_CANAEROSPACE_NODE_ID_DEFAULT;
    table->parameter_can_ids[CA_PARAM_AXIS_XY] = JOYSTICK_CANAEROSPACE_AXIS_XY_ID;
    table->parameter_can_ids[CA_PARAM_AXIS_Z_RX] = JOYSTICK_CANAEROSPACE_AXIS_Z_RX_ID;
    table->parameter_can_ids[CA_PARAM_AXIS_RY_RZ] = JOYSTICK_CANAEROSPACE_AXIS_RY_RZ_ID;
    table->parameter_can_ids[CA_PARAM_AXIS_SLIDER_DIAL] = JOYSTICK_CANAEROSPACE_AXIS_SLIDER_DIAL_ID;
    table->parameter_can_ids[CA_PARAM_BUTTONS_0_31] = JOYSTICK_CANAEROSPACE_BUTTONS_0_31_ID;
    table->parameter_can_ids[CA_PARAM_BUTTONS_32_63] = JOYSTICK_CANAEROSPACE_BUTTONS_32_63_ID;
    table->parameter_can_ids[CA_PARAM_STATUS] = JOYSTICK_CANAEROSPACE_STATUS_ID;
    table->parameter_period_ms = 20U;
    table->status_period_ms = 100U;
    table->stale_timeout_ms = 250U;

    table->axis_map_count = 0U;
    table->button_map_count = 0U;
}

void joystick_can_pipeline_reset(joystick_can_pipeline_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

void joystick_can_pipeline_on_connect(joystick_can_pipeline_state_t *state,
                                      uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    state->connected = true;
    state->joystick_present = false;
    state->has_sample = false;
    state->hid_parse_error = false;
    usb_host_hid_joystick_state_clear(&state->sample);
    state->last_report_ms = now_ms;
    state->report_age_bucket = 0U;
}

void joystick_can_pipeline_on_disconnect(joystick_can_pipeline_state_t *state,
                                         uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    state->connected = false;
    state->joystick_present = false;
    state->hid_parse_error = false;
    state->has_sample = false;
    state->last_report_ms = now_ms;
    state->report_age_bucket = 0U;
    usb_host_hid_joystick_mark_disconnected(&state->sample, now_ms);
}

void joystick_can_pipeline_submit_sample(joystick_can_pipeline_state_t *state,
                                         const usb_host_hid_joystick_state_t *sample)
{
    if ((state == NULL) || (sample == NULL)) {
        return;
    }

    state->sample = *sample;
    state->has_sample = sample->valid;
    state->connected = sample->connected;
    if (!sample->connected) {
        state->joystick_present = false;
    } else if (sample->valid && !sample->stale) {
        state->joystick_present = true;
        state->hid_parse_error = false;
    }
    state->last_report_ms = sample->timestamp_ms;
    state->report_age_bucket = 0U;
}

void joystick_can_pipeline_note_tx_backpressure(joystick_can_pipeline_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (!state->can_tx_backpressure_pending) {
        state->can_tx_backpressure_count += 1U;
        state->can_tx_backpressure_pending = true;
    }
}

void joystick_can_pipeline_note_hid_parse_error(joystick_can_pipeline_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->hid_parse_error = true;
}

bool joystick_can_pipeline_is_failsafe(const joystick_can_mapping_table_t *table,
                                       joystick_can_pipeline_state_t *state,
                                       uint32_t now_ms)
{
    return joystick_can_pipeline_hid_quality(table, state, now_ms) != JOYSTICK_CANAEROSPACE_SERVICE_VALID;
}

bool joystick_can_pipeline_build_next_frame(const joystick_can_mapping_table_t *table,
                                            joystick_can_pipeline_state_t *state,
                                            uint32_t now_ms,
                                            can_frame_t *out_frame,
                                            joystick_can_tx_kind_t *out_kind)
{
    return joystick_can_pipeline_prepare_available(table, state, now_ms, 0U, out_frame, out_kind);
}

bool joystick_can_pipeline_prepare_available(const joystick_can_mapping_table_t *table,
                                             joystick_can_pipeline_state_t *state,
                                             uint32_t now_ms,
                                             uint32_t unavailable_parameters,
                                             can_frame_t *out_frame,
                                             joystick_can_tx_kind_t *out_kind)
{
    uint8_t attempts;
    bool failsafe;
    uint8_t service;

    if ((table == NULL) || (state == NULL) || (out_frame == NULL) || (out_kind == NULL)) {
        return false;
    }

    *out_kind = JOYSTICK_CAN_TX_NONE;
    service = joystick_can_pipeline_hid_quality(table, state, now_ms);
    failsafe = service != JOYSTICK_CANAEROSPACE_SERVICE_VALID;
    if (state->can_tx_backpressure_pending) {
        service |= JOYSTICK_CANAEROSPACE_SERVICE_TX_BACKPRESSURE_SEEN;
    }

    for (attempts = 0U; attempts < JOYSTICK_CANAEROSPACE_PARAM_COUNT; attempts++) {
        ca_param_index_t param = (ca_param_index_t)((state->next_parameter_index + attempts) %
                                                    JOYSTICK_CANAEROSPACE_PARAM_COUNT);
        uint16_t period = (param == CA_PARAM_STATUS) ? table->status_period_ms : table->parameter_period_ms;

        if ((unavailable_parameters & (1UL << param)) == 0U &&
            period_elapsed(now_ms, state->last_parameter_tx_ms[param], period)) {
            switch (param) {
            case CA_PARAM_AXIS_XY:
                write_axis_pair(table, state, param, 0U, 1U, failsafe, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_AXIS_XY;
                break;
            case CA_PARAM_AXIS_Z_RX:
                write_axis_pair(table, state, param, 2U, 3U, failsafe, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_AXIS_Z_RX;
                break;
            case CA_PARAM_AXIS_RY_RZ:
                write_axis_pair(table, state, param, 4U, 5U, failsafe, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_AXIS_RY_RZ;
                break;
            case CA_PARAM_AXIS_SLIDER_DIAL:
                write_axis_pair(table, state, param, 6U, 7U, failsafe, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_AXIS_SLIDER_DIAL;
                break;
            case CA_PARAM_BUTTONS_0_31:
                write_buttons(table, state, param, 0U, failsafe, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_BUTTONS_0_31;
                break;
            case CA_PARAM_BUTTONS_32_63:
                write_buttons(table, state, param, 32U, failsafe, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_BUTTONS_32_63;
                break;
            case CA_PARAM_STATUS:
            default:
                write_status(table, state, service, out_frame);
                *out_kind = JOYSTICK_CAN_TX_STATUS;
                break;
            }

            return true;
        }
    }

    return false;
}

void joystick_can_pipeline_commit_frame(joystick_can_pipeline_state_t *state,
                                        joystick_can_tx_kind_t kind,
                                        uint32_t now_ms)
{
    if (state == NULL || kind < JOYSTICK_CAN_TX_AXIS_XY || kind > JOYSTICK_CAN_TX_STATUS) {
        return;
    }
    /* The public transmit kinds follow the parameter order, after TX_NONE. */
    uint8_t param = (uint8_t)(kind - JOYSTICK_CAN_TX_AXIS_XY);
    state->message_codes[param]++;
    state->last_parameter_tx_ms[param] = now_ms;
    state->last_status_tx_ms = state->last_parameter_tx_ms[CA_PARAM_STATUS];
    state->next_parameter_index = (uint8_t)((param + 1U) % JOYSTICK_CANAEROSPACE_PARAM_COUNT);
    state->frame_sequence++;
    state->can_tx_backpressure_pending = false;
}
