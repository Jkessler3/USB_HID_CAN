/* Exercise the target's actual foreground CAN sender. Only hardware I/O is simulated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "joystick_translator_app.h"

/* CMSIS interrupt instructions cannot execute on a native host. */
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#undef SCB
static SCB_Type simulated_scb;
#define SCB (&simulated_scb)
#define _write test_uart_write
#include "joystick_translator_app.c"

static CAN_HandleTypeDef can;
static unsigned occupied, aborted, accepted, fail_next, fail_abort;
static unsigned defer_abort, abort_requests;
static uint8_t payload[8];
static uint32_t last_id, tick, uart_writes;

uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *h, const CAN_FilterTypeDef *filter)
{ (void)h; (void)filter; return HAL_OK; }
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *h)
{ (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *h, uint32_t mask)
{ (void)h; (void)mask; return HAL_OK; }
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *h, uint32_t fifo,
                                      CAN_RxHeaderTypeDef *header, uint8_t data[])
{ (void)h; (void)fifo; (void)header; (void)data; return HAL_ERROR; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *data,
                                  uint16_t n, uint32_t timeout)
{ (void)h; (void)data; (void)n; (void)timeout; ++uart_writes; return HAL_OK; }
uint16_t USBH_HID_FifoRead(FIFO_TypeDef *fifo, void *buffer, uint16_t size)
{ (void)fifo; (void)buffer; (void)size; return 0; }
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *handle)
{
    (void)handle;
    return 3U - !!(occupied & 1U) - !!(occupied & 2U) - !!(occupied & 4U);
}
uint32_t HAL_CAN_IsTxMessagePending(const CAN_HandleTypeDef *handle, uint32_t mask)
{
    (void)handle;
    return !!(occupied & mask);
}
HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *handle, uint32_t mask)
{
    (void)handle;
    ++abort_requests;
    if (fail_abort) { fail_abort = 0; return HAL_ERROR; }
    aborted |= occupied & mask;
    if (!defer_abort) { occupied &= ~mask; }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *handle,
                                      const CAN_TxHeaderTypeDef *header,
                                      const uint8_t data[], uint32_t *mailbox)
{
    (void)handle;
    if (fail_next) { fail_next = 0; return HAL_ERROR; }
    for (unsigned mask = 1; mask <= 4; mask <<= 1) {
        if (!(occupied & mask)) {
            *mailbox = mask;
            occupied |= mask;
            accepted++;
            memcpy(payload, data, sizeof(payload));
            last_id = header->StdId;
            return HAL_OK;
        }
    }
    return HAL_BUSY;
}

static void start(void)
{
    memset(&s_app, 0, sizeof(s_app));
    occupied = aborted = accepted = fail_next = fail_abort = 0;
    defer_abort = abort_requests = 0;
    s_app.hcan = &can;
    s_app.can_ready = 1;
    s_app.last_appli_state = APPLICATION_READY;
    joystick_can_mapping_table_init_default(&s_app.mapping);
    joystick_can_pipeline_on_connect(&s_app.pipeline, 0);
}
static void sample(uint32_t now)
{
    usb_host_hid_joystick_state_t s = {
        .connected = true, .valid = true, .axis_count = 2, .axes = {32767,-32768},
        .button_count = 1, .buttons = 1, .timestamp_ms = now
    };
    joystick_can_pipeline_submit_sample(&s_app.pipeline, &s);
}
static void send_at(uint32_t now)
{
    tick = now;
    app_send_mapped_can_frames(now);
}
static void retry_after_congestion(void)
{
    start();
    sample(10);
    occupied = 7; /* Other mailboxes are full when nothing is due. */
    send_at(11);
    assert(s_app.pipeline.can_tx_backpressure_count == 0);
    send_at(20);
    send_at(21);
    assert(accepted == 0 && s_app.pipeline.frame_sequence == 0);
    assert(s_app.pipeline.can_tx_backpressure_count == 0); /* Normal short arbitration wait. */
    send_at(40);
    send_at(41);
    assert(s_app.pipeline.can_tx_backpressure_count == 1);
    occupied = 0;
    fail_next = 1;
    send_at(42);
    assert(s_app.pipeline.last_parameter_tx_ms[0] == 0 && s_app.pipeline.message_codes[0] == 0);
    send_at(43);
    assert(last_id == 1500 && payload[2] == 0x08 && payload[3] == 0);
    assert(s_app.pipeline.last_parameter_tx_ms[0] == 43 && s_app.pipeline.frame_sequence == 1);
    send_at(44);
    assert(last_id == 1501 && payload[2] == 0 && payload[3] == 0);
}

static void cancel_obsolete_mailboxes(void)
{
    start();
    sample(100);
    send_at(100);
    send_at(101);
    assert(occupied == 3 && payload[2] == 0);
    joystick_can_pipeline_note_hid_parse_error(&s_app.pipeline);
    send_at(102);
    assert(aborted == 3);
    assert(payload[2] == 0x04 && memcmp(payload + 4, (uint8_t[]){128,0,128,0}, 4) == 0);
    start();
    sample(100);
    send_at(100); /* Outstanding sample A. */
    sample(300);  /* Fresh sample B does not extend A's queued lifetime. */
    send_at(350);
    assert(aborted == 0);
    send_at(351);
    assert(aborted == 1 && payload[2] == 0);

    start();
    sample(100);
    send_at(100);
    fail_abort = 1;
    joystick_can_pipeline_note_hid_parse_error(&s_app.pipeline);
    send_at(101);
    assert(!aborted);
    sample(102);
    send_at(102);
    assert(aborted == 3); /* Retire A and the now-obsolete fault frame on recovery. */
}

static void generation_ordering(void)
{
    start();
    sample(100);
    s_app.mapping.stale_timeout_ms = 0;
    s_app.pipeline.next_parameter_index = JOYSTICK_CAN_TX_BUTTONS_32_63 - JOYSTICK_CAN_TX_AXIS_XY;
    send_at(100);
    assert(last_id == 1505 && occupied == 1 && payload[3] == 0);
    unsigned other_ids = 0;
    for (unsigned now = 121; now < 165; ++now) {
        sample(now); /* A new sample alone must not repeatedly cancel a pending frame. */
        unsigned before = accepted;
        send_at(now);
        if (accepted != before) {
            assert(last_id != 1505);
            other_ids |= 1U << (last_id - 1500);
        }
        occupied &= 1U; /* Other IDs complete. Keep the first 1505 pending. */
    }
    assert(other_ids == 0x5fU && abort_requests == 0);
    occupied = 0;
    for (unsigned now = 165; now < 185 && last_id != 1505; ++now) {
        send_at(now);
        occupied = 0;
    }
    assert(last_id == 1505 && payload[3] == 1);
}

static void obsolete_status_during_recovery(void)
{
    start();
    joystick_can_pipeline_on_disconnect(&s_app.pipeline, 90);
    s_app.mapping.status_period_ms = 0;
    s_app.pipeline.next_parameter_index = JOYSTICK_CAN_TX_STATUS - JOYSTICK_CAN_TX_AXIS_XY;
    occupied = 3;
    send_at(100);
    assert(last_id == 1506 && payload[2] == 0x13 && occupied == 7);
    occupied = 4; /* The disconnected status still occupies the third mailbox. */
    defer_abort = 1;
    sample(101);
    for (unsigned now = 101; now < 130; ++now) {
        unsigned before = accepted;
        send_at(now);
        if (accepted != before) { assert(last_id != 1506 && payload[2] == 0); }
        occupied &= 4U; /* Hardware has accepted the abort but has not completed it. */
    }
    assert(aborted == 4 && abort_requests == 1);
    occupied = 0;
    for (unsigned now = 130; now < 150 && last_id != 1506; ++now) {
        send_at(now);
        occupied = 0;
    }
    assert(last_id == 1506 && payload[2] == 0 && payload[3] == 1);
}

static void fault_mailbox_lifetime(void)
{
    start();
    joystick_can_pipeline_on_disconnect(&s_app.pipeline, 90);
    s_app.pipeline.next_parameter_index = JOYSTICK_CAN_TX_STATUS - JOYSTICK_CAN_TX_AXIS_XY;
    send_at(100);
    assert(last_id == 1506 && payload[2] == 0x13);
    joystick_can_pipeline_on_connect(&s_app.pipeline, 101);
    send_at(101);
    assert(aborted == 1 && payload[2] == 0x11); /* Attached awaiting first report. */

    start();
    joystick_can_pipeline_on_disconnect(&s_app.pipeline, 0);
    s_app.pipeline.next_parameter_index = JOYSTICK_CAN_TX_STATUS - JOYSTICK_CAN_TX_AXIS_XY;
    send_at(400);
    assert(last_id == 1506 && payload[2] == 0x13);
    send_at(650);
    assert(aborted == 0); /* Age a fault frame from its enqueue, independent of the last report. */
    send_at(651);
    assert(aborted == 1);
}

static void capture_controls(uint32_t now, uint8_t frames[7][8])
{
    unsigned mask = 0;
    tick = now;
    for (unsigned i = 0; i < 7; ++i) {
        unsigned before = accepted;
        occupied = 0; /* Simulate previous queue entries completing. */
        joystick_translator_app_run(tick, APPLICATION_READY);
        if (accepted != before) {
            assert(last_id >= 1500 && last_id <= 1506);
            unsigned channel = last_id - 1500;
            memcpy(frames[channel], payload, 8);
            mask |= 1U << channel;
        }
    }
    assert((mask & 0x3fU) == 0x3fU);
}

static void hid_callback_to_wire(void)
{
    /* Joystick, X/Y, four constant padding bytes. No report ID or buttons. */
    const uint8_t descriptor[] = {
        0x05,1, 0x09,4, 0xA1,1, 0x15,0, 0x26,255,0, 0x75,8,
        0x95,2, 0x09,0x30, 0x09,0x31, 0x81,2, 0x95,4, 0x81,1, 0xC0
    };
    USBH_HandleTypeDef host = {0};
    uint8_t report[] = {0,255,255,255,255,255};
    uint8_t frames[7][8] = {0};
    start();
    tick = 0;
    UART_HandleTypeDef uart = {0};
    joystick_translator_app_init(&can, &uart, &host, APPLICATION_IDLE, tick);
    joystick_translator_app_run(++tick, APPLICATION_START);
    USBH_HID_ReportDescriptorCallback(&host, descriptor, sizeof(descriptor));
    joystick_translator_app_run(++tick, APPLICATION_READY);
    for (uint8_t x = 1; x <= 8; ++x) {
        report[0] = x;
        for (unsigned repeat = 0; repeat < 5; ++repeat) {
            ++tick;
            USBH_HID_InputReportCallback(&host, report, sizeof(report));
        }
        capture_controls(tick + 20, frames);
        assert(frames[0][2] == 0 && frames[0][4] == x && frames[0][5] == 0);
        assert(frames[0][6] == 0xff && frames[0][7] == 0xff);
        assert(!memcmp(frames[4] + 4, (uint8_t[]){0,0,0,0}, 4));
        assert(!memcmp(frames[5] + 4, (uint8_t[]){0,0,0,0}, 4));
    }
    USBH_HID_InputReportCallback(&host, report, 3); /* Short transfer; padded buffer still exists. */
    capture_controls(tick + 20, frames);
    assert(frames[0][2] == 0x04 && !memcmp(frames[0]+4, (uint8_t[]){128,0,128,0}, 4));
    report[0] = 255;
    report[1] = 0;
    USBH_HID_InputReportCallback(&host, report, sizeof(report));
    capture_controls(tick + 20, frames);
    assert(frames[0][2] == 0 && !memcmp(frames[0]+4, (uint8_t[]){255,255,0,0}, 4));
    tick += 1000U;
    USBH_HID_InputReportCallback(&host, report, sizeof(report));
    joystick_translator_app_run(tick, APPLICATION_READY);
    assert((uart_writes != 0U) == EXPECT_UART_TRACE);
    unsigned before_irq = uart_writes;
    simulated_scb.ICSR = 1U;
    assert(test_uart_write(1, "irq", 3) == 3 && uart_writes == before_irq);
    simulated_scb.ICSR = 0U;
    puts("Descriptor and actual HID transfer length replayed through the target app to CAN");
}

int main(void)
{
    generation_ordering();
    obsolete_status_during_recovery();
    fault_mailbox_lifetime();
    retry_after_congestion();
    cancel_obsolete_mailboxes();
    start();
    sample(10);
    s_app.hid_device.connected = true;
    s_app.hid_device.class_active = true;
    s_app.hid_device.last_report_ms = 10;
    tick = 261;
    joystick_translator_app_run(tick, APPLICATION_READY);
    assert(s_app.pipeline.connected && s_app.pipeline.joystick_present);
    assert(last_id == 1500 && payload[2] == 0x01);
    hid_callback_to_wire();
    puts("Target CAN enqueue retry, backpressure recovery and bxCAN cancellation passed");
    return 0;
}
