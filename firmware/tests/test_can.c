/* Host regression vectors for the firmware's public CAN mapping API. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "joystick_can_mapping.h"

static void faults_and_recovery(void)
{
    joystick_can_mapping_table_t table;
    joystick_can_pipeline_state_t state;
    usb_host_hid_joystick_state_t sample = {
        .connected = true, .valid = true, .axis_count = 2, .button_count = 32,
        .axes = { -32768, 32767 }, .buttons = 0x80000001U, .timestamp_ms = 100
    };
    can_frame_t first, retry;
    joystick_can_tx_kind_t kind, retry_kind;
    joystick_can_mapping_table_init_default(&table);
    joystick_can_pipeline_reset(&state);
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 50, &first, &kind));
    assert(first.data[2] == 0x13);
    joystick_can_pipeline_on_connect(&state, 50);
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 50, &first, &kind));
    assert(first.data[2] == 0x11);
    joystick_can_pipeline_submit_sample(&state, &sample);

    joystick_can_pipeline_note_hid_parse_error(&state);
    assert(joystick_can_pipeline_is_failsafe(&table, &state, 101));
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 101, &first, &kind));
    assert(first.id == 1500 && first.data[2] == 0x04);
    assert(memcmp(first.data + 4, (uint8_t[]){0x80, 0, 0x80, 0}, 4) == 0);
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 101, &retry, &retry_kind));
    assert(retry_kind == kind && !memcmp(first.data, retry.data, 8));
    assert(state.frame_sequence == 0 && state.last_parameter_tx_ms[0] == 0);

    joystick_can_pipeline_submit_sample(&state, &sample);
    assert(!joystick_can_pipeline_is_failsafe(&table, &state, 350));
    assert(joystick_can_pipeline_is_failsafe(&table, &state, 351));
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 351, &first, &kind));
    assert(first.data[2] == 0x01);
    sample.timestamp_ms = 352;
    joystick_can_pipeline_submit_sample(&state, &sample);
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 352, &first, &kind));
    assert(first.data[2] == 0 && memcmp(first.data + 4, (uint8_t[]){0,0,255,255}, 4) == 0);
    joystick_can_pipeline_note_hid_parse_error(&state);
    joystick_can_pipeline_on_disconnect(&state, 355);
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 355, &first, &kind));
    assert(first.data[2] == 0x13);
    joystick_can_pipeline_on_connect(&state, 360);
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 360, &first, &kind));
    assert(first.data[2] == 0x11 && first.data[4] == 0x80);
    sample.timestamp_ms = UINT32_MAX - 30U;
    joystick_can_pipeline_submit_sample(&state, &sample);
    assert(!joystick_can_pipeline_is_failsafe(&table, &state, 219));
    assert(joystick_can_pipeline_is_failsafe(&table, &state, 220));
}

static void silence_for_a_full_tick_epoch(void)
{
    joystick_can_mapping_table_t table;
    joystick_can_pipeline_state_t state;
    usb_host_hid_joystick_state_t sample = {
        .connected = true, .valid = true, .axis_count = 1, .axes = {32767}, .timestamp_ms = 1000
    };
    can_frame_t frame;
    joystick_can_tx_kind_t kind;
    joystick_can_mapping_table_init_default(&table);
    joystick_can_pipeline_reset(&state);
    joystick_can_pipeline_submit_sample(&state, &sample);
    assert(joystick_can_pipeline_is_failsafe(&table, &state, 1251));
    assert(joystick_can_pipeline_build_next_frame(&table, &state, UINT32_MAX, &frame, &kind));
    /* After another 1001 ticks the 32-bit clock matches the old sample timestamp. */
    assert(joystick_can_pipeline_is_failsafe(&table, &state, 1000));
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 1000, &frame, &kind));
    assert(frame.data[2] == 0x01 && !memcmp(frame.data + 4, (uint8_t[]){128,0,128,0}, 4));
    state.next_parameter_index = JOYSTICK_CAN_TX_STATUS - JOYSTICK_CAN_TX_AXIS_XY;
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 1000, &frame, &kind));
    assert(frame.id == 1506 && frame.data[7] == 255);
    sample.timestamp_ms = 1001;
    joystick_can_pipeline_submit_sample(&state, &sample);
    assert(!joystick_can_pipeline_is_failsafe(&table, &state, 1001));
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 1001, &frame, &kind));
    assert(frame.data[2] == 0 && frame.data[7] == 0);
    assert(!joystick_can_pipeline_prepare_available(&table, &state, 1001, 0x7fU, &frame, &kind));
    assert(kind == JOYSTICK_CAN_TX_NONE && state.frame_sequence == 0);
    assert(joystick_can_pipeline_prepare_available(&table, &state, 1001, 0x5fU, &frame, &kind));
    assert(frame.id == 1505 && kind == JOYSTICK_CAN_TX_BUTTONS_32_63);
}

static void all_identifiers_and_wrap(void)
{
    joystick_can_mapping_table_t table;
    joystick_can_pipeline_state_t state;
    usb_host_hid_joystick_state_t sample = {
        .connected = true, .valid = true, .axis_count = 8, .button_count = 64,
        .axes = {-32768, 32767, -1, 1, 2, -2, 255, -256},
        .buttons = UINT64_C(0x8100040280000001), .sequence = 7, .timestamp_ms = 90
    };
    const uint8_t data[][4] = {
        {0,0,255,255}, {127,255,128,1}, {128,2,127,254}, {128,255,127,0},
        {128,0,0,1}, {129,0,4,2}
    };
    joystick_can_mapping_table_init_default(&table);
    joystick_can_pipeline_reset(&state);
    joystick_can_pipeline_submit_sample(&state, &sample);
    table.parameter_period_ms = table.status_period_ms = 0;
    for (unsigned cycle = 0; cycle <= 256; ++cycle) {
        for (unsigned channel = 0; channel < 7; ++channel) {
            can_frame_t f;
            joystick_can_tx_kind_t kind;
            assert(joystick_can_pipeline_build_next_frame(&table, &state, 100, &f, &kind));
            assert(f.id == 1500U + channel && !f.extended && !f.rtr && f.dlc == 8U);
            assert(f.data[0] == 1 && f.data[2] == 0 && f.data[3] == (uint8_t)cycle);
            if (channel < 6) {
                assert(f.data[1] == (channel < 4 ? 0x0D : 0x05));
                assert(memcmp(f.data + 4, data[channel], 4) == 0);
            } else {
                assert(f.data[1] == 0x10 && f.data[4] == 0 && f.data[5] == 7 &&
                       f.data[6] == (uint8_t)state.frame_sequence && f.data[7] == 1);
            }
            joystick_can_pipeline_commit_frame(&state, kind, 100);
        }
    }
    /* Undefined upper bits from a shorter report must never become buttons. */
    sample.button_count = 33;
    sample.buttons = UINT64_MAX;
    joystick_can_pipeline_submit_sample(&state, &sample);
    state.next_parameter_index = JOYSTICK_CAN_TX_BUTTONS_32_63 - JOYSTICK_CAN_TX_AXIS_XY;
    can_frame_t f;
    joystick_can_tx_kind_t kind;
    assert(joystick_can_pipeline_build_next_frame(&table, &state, 100, &f, &kind));
    assert(f.id == 1505 && !memcmp(f.data + 4, (uint8_t[]){0,0,0,1}, 4));
}

int main(void)
{
    faults_and_recovery();
    silence_for_a_full_tick_epoch();
    all_identifiers_and_wrap();
    puts("CAN presence, faults, full-tick-epoch silence, recovery, all seven IDs and counter wrap passed");
    return 0;
}
