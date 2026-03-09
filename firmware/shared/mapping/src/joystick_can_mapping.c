#include "joystick_can_mapping.h"

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

static int16_t apply_deadzone(int16_t value, uint16_t deadzone)
{
    if ((value < (int16_t)deadzone) && (value > (int16_t)(-(int16_t)deadzone))) {
        return 0;
    }
    return value;
}

static bool period_elapsed(uint32_t now_ms, uint32_t last_ms, uint16_t period_ms)
{
    if (period_ms == 0U) {
        return true;
    }
    return ((now_ms - last_ms) >= period_ms);
}

static int16_t map_axis_value(const joystick_can_axis_map_t *map, int16_t raw_value)
{
    int32_t in_span;
    int32_t out_span;
    int32_t value;

    if (map == NULL) {
        return 0;
    }

    value = apply_deadzone(raw_value, map->deadzone);
    value = clamp_i16(value, map->input_min, map->input_max);

    in_span = (int32_t)map->input_max - (int32_t)map->input_min;
    out_span = (int32_t)map->output_max - (int32_t)map->output_min;

    if (in_span == 0) {
        value = map->output_min;
    } else {
        value = ((value - map->input_min) * out_span) / in_span + map->output_min;
    }

    if (map->invert) {
        value = -value;
    }

    return clamp_i16(value, map->output_min, map->output_max);
}

static void write_control_frame(const joystick_can_mapping_table_t *table,
                                joystick_can_pipeline_state_t *state,
                                bool failsafe,
                                can_frame_t *out_frame)
{
    int16_t signal_values[JOYSTICK_CAN_SIGNAL_COUNT];
    uint8_t i;
    uint8_t button_flags;

    memset(signal_values, 0, sizeof(signal_values));
    button_flags = 0U;

    if (!failsafe && state->has_sample && state->sample.valid) {
        uint8_t axis_map_count = table->axis_map_count;
        uint8_t button_map_count = table->button_map_count;

        if (axis_map_count > JOYSTICK_CAN_MAX_AXIS_MAPS) {
            axis_map_count = JOYSTICK_CAN_MAX_AXIS_MAPS;
        }
        if (button_map_count > JOYSTICK_CAN_MAX_BUTTON_MAPS) {
            button_map_count = JOYSTICK_CAN_MAX_BUTTON_MAPS;
        }

        for (i = 0U; i < axis_map_count; i++) {
            const joystick_can_axis_map_t *map = &table->axis_maps[i];
            int16_t source_value;
            int16_t mapped;

            if ((map->axis_index >= state->sample.axis_count) ||
                (map->signal_index >= JOYSTICK_CAN_SIGNAL_COUNT)) {
                continue;
            }

            source_value = state->sample.axes[map->axis_index];
            mapped = map_axis_value(map, source_value);
            signal_values[map->signal_index] = mapped;
        }

        for (i = 0U; i < button_map_count; i++) {
            const joystick_can_button_map_t *map = &table->button_maps[i];
            bool pressed;

            if ((map->button_index >= 32U) || (map->out_byte_index >= 8U)) {
                continue;
            }

            pressed = ((state->sample.buttons & (1UL << map->button_index)) != 0U);
            if (map->active_high) {
                if (pressed && (map->out_byte_index == 6U)) {
                    button_flags |= map->out_bit_mask;
                }
            } else {
                if (!pressed && (map->out_byte_index == 6U)) {
                    button_flags |= map->out_bit_mask;
                }
            }
        }
    }

    can_frame_clear(out_frame);
    out_frame->id = table->control_can_id;
    out_frame->extended = false;
    out_frame->rtr = false;
    out_frame->dlc = 8U;
    out_frame->data[0] = (uint8_t)(signal_values[JOYSTICK_CAN_SIGNAL_STEER] & 0xFF);
    out_frame->data[1] = (uint8_t)((signal_values[JOYSTICK_CAN_SIGNAL_STEER] >> 8) & 0xFF);
    out_frame->data[2] = (uint8_t)(signal_values[JOYSTICK_CAN_SIGNAL_THROTTLE] & 0xFF);
    out_frame->data[3] = (uint8_t)((signal_values[JOYSTICK_CAN_SIGNAL_THROTTLE] >> 8) & 0xFF);
    out_frame->data[4] = (uint8_t)signal_values[JOYSTICK_CAN_SIGNAL_AUX0];
    out_frame->data[5] = (uint8_t)signal_values[JOYSTICK_CAN_SIGNAL_AUX1];
    out_frame->data[6] = button_flags;
    out_frame->data[7] = (uint8_t)((state->connected ? 0x01U : 0x00U) |
                                   (failsafe ? 0x02U : 0x00U) |
                                   ((state->frame_sequence & 0x1FU) << 3U));
}

static void write_status_frame(const joystick_can_mapping_table_t *table,
                               joystick_can_pipeline_state_t *state,
                               bool failsafe,
                               uint32_t now_ms,
                               can_frame_t *out_frame)
{
    can_frame_clear(out_frame);
    out_frame->id = table->status_can_id;
    out_frame->extended = false;
    out_frame->rtr = false;
    out_frame->dlc = 8U;
    out_frame->data[0] = (uint8_t)((state->connected ? 0x01U : 0x00U) |
                                   (failsafe ? 0x02U : 0x00U));
    out_frame->data[1] = state->sample.axis_count;
    out_frame->data[2] = (uint8_t)(state->last_report_ms & 0xFFU);
    out_frame->data[3] = (uint8_t)((state->last_report_ms >> 8U) & 0xFFU);
    out_frame->data[4] = (uint8_t)((state->last_report_ms >> 16U) & 0xFFU);
    out_frame->data[5] = (uint8_t)((state->last_report_ms >> 24U) & 0xFFU);
    out_frame->data[6] = (uint8_t)(now_ms & 0xFFU);
    out_frame->data[7] = (uint8_t)((state->frame_sequence) & 0xFFU);
}

void joystick_can_mapping_table_init_default(joystick_can_mapping_table_t *table)
{
    if (table == NULL) {
        return;
    }

    memset(table, 0, sizeof(*table));
    table->control_can_id = JOYSTICK_CAN_CONTROL_ID;
    table->status_can_id = JOYSTICK_CAN_STATUS_ID;
    table->control_period_ms = 20U;
    table->status_period_ms = 100U;
    table->stale_timeout_ms = 250U;

    table->axis_map_count = 2U;
    table->axis_maps[0].axis_index = 0U;
    table->axis_maps[0].signal_index = JOYSTICK_CAN_SIGNAL_STEER;
    table->axis_maps[0].input_min = -32768;
    table->axis_maps[0].input_max = 32767;
    table->axis_maps[0].output_min = -1000;
    table->axis_maps[0].output_max = 1000;
    table->axis_maps[0].deadzone = 512U;
    table->axis_maps[0].invert = false;

    table->axis_maps[1].axis_index = 1U;
    table->axis_maps[1].signal_index = JOYSTICK_CAN_SIGNAL_THROTTLE;
    table->axis_maps[1].input_min = -32768;
    table->axis_maps[1].input_max = 32767;
    table->axis_maps[1].output_min = -1000;
    table->axis_maps[1].output_max = 1000;
    table->axis_maps[1].deadzone = 512U;
    table->axis_maps[1].invert = true;

    table->button_map_count = 4U;
    table->button_maps[0].button_index = 0U;
    table->button_maps[0].out_byte_index = 6U;
    table->button_maps[0].out_bit_mask = 0x01U;
    table->button_maps[0].active_high = true;
    table->button_maps[1].button_index = 1U;
    table->button_maps[1].out_byte_index = 6U;
    table->button_maps[1].out_bit_mask = 0x02U;
    table->button_maps[1].active_high = true;
    table->button_maps[2].button_index = 2U;
    table->button_maps[2].out_byte_index = 6U;
    table->button_maps[2].out_bit_mask = 0x04U;
    table->button_maps[2].active_high = true;
    table->button_maps[3].button_index = 3U;
    table->button_maps[3].out_byte_index = 6U;
    table->button_maps[3].out_bit_mask = 0x08U;
    table->button_maps[3].active_high = true;
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
    state->failsafe_active = false;
    state->last_report_ms = now_ms;
}

void joystick_can_pipeline_on_disconnect(joystick_can_pipeline_state_t *state,
                                         uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    state->connected = false;
    state->failsafe_active = true;
    state->has_sample = false;
    state->last_report_ms = now_ms;
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
    state->last_report_ms = sample->timestamp_ms;
}

bool joystick_can_pipeline_is_failsafe(const joystick_can_mapping_table_t *table,
                                       const joystick_can_pipeline_state_t *state,
                                       uint32_t now_ms)
{
    if ((table == NULL) || (state == NULL)) {
        return true;
    }

    if (!state->connected || !state->has_sample || !state->sample.valid || state->sample.stale) {
        return true;
    }

    if (table->stale_timeout_ms == 0U) {
        return false;
    }

    return ((now_ms - state->last_report_ms) > table->stale_timeout_ms);
}

bool joystick_can_pipeline_build_next_frame(const joystick_can_mapping_table_t *table,
                                            joystick_can_pipeline_state_t *state,
                                            uint32_t now_ms,
                                            can_frame_t *out_frame,
                                            joystick_can_tx_kind_t *out_kind)
{
    bool control_due;
    bool status_due;
    bool failsafe;

    if ((table == NULL) || (state == NULL) || (out_frame == NULL) || (out_kind == NULL)) {
        return false;
    }

    *out_kind = JOYSTICK_CAN_TX_NONE;
    control_due = period_elapsed(now_ms, state->last_control_tx_ms, table->control_period_ms);
    status_due = period_elapsed(now_ms, state->last_status_tx_ms, table->status_period_ms);
    if (!control_due && !status_due) {
        return false;
    }

    failsafe = joystick_can_pipeline_is_failsafe(table, state, now_ms);
    state->failsafe_active = failsafe;

    if (control_due) {
        write_control_frame(table, state, failsafe, out_frame);
        state->last_control_tx_ms = now_ms;
        state->frame_sequence += 1U;
        *out_kind = JOYSTICK_CAN_TX_CONTROL;
        return true;
    }

    write_status_frame(table, state, failsafe, now_ms, out_frame);
    state->last_status_tx_ms = now_ms;
    state->frame_sequence += 1U;
    *out_kind = JOYSTICK_CAN_TX_STATUS;
    return true;
}
