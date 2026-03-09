#include "usb_hid_transport.h"

#include <string.h>

#include "hid_protocol.h"
#include "obd_mode01.h"
#include "spsc_ring.h"

#ifndef USBD_OK
#define USBD_OK 0U
#endif

#define HID_RX_REPORT_QUEUE_DEPTH      16U
#define HID_TX_REPORT_QUEUE_DEPTH      32U
#define HID_CAN_RX_QUEUE_DEPTH         32U

typedef struct {
    uint8_t raw[HID_PROTOCOL_REPORT_SIZE];
} hid_raw_report_t;

typedef struct {
    can_frame_t frame;
    uint32_t timestamp_ms;
} hid_can_rx_item_t;

static hid_transport_callbacks_t s_callbacks;
static hid_transport_usb_tx_cb_t s_usb_tx_cb;

static spsc_ring_t s_host_rx_ring;
static spsc_ring_t s_usb_tx_ring;
static spsc_ring_t s_can_rx_ring;

static uint8_t s_host_rx_storage[HID_RX_REPORT_QUEUE_DEPTH * sizeof(hid_raw_report_t)];
static uint8_t s_usb_tx_storage[HID_TX_REPORT_QUEUE_DEPTH * sizeof(hid_raw_report_t)];
static uint8_t s_can_rx_storage[HID_CAN_RX_QUEUE_DEPTH * sizeof(hid_can_rx_item_t)];

static volatile uint32_t s_drop_count;
static uint32_t s_next_seq = 1U;

static uint32_t next_event_seq(void)
{
    uint32_t seq = s_next_seq;
    s_next_seq++;
    if (s_next_seq == 0U) {
        s_next_seq = 1U;
    }
    return seq;
}

static bool tx_queue_push_raw(const uint8_t raw[HID_PROTOCOL_REPORT_SIZE])
{
    hid_raw_report_t item;
    memcpy(item.raw, raw, sizeof(item.raw));
    return spsc_ring_push(&s_usb_tx_ring, &item);
}

static bool tx_queue_peek_raw(uint8_t raw[HID_PROTOCOL_REPORT_SIZE])
{
    hid_raw_report_t item;
    if (!spsc_ring_peek(&s_usb_tx_ring, &item)) {
        return false;
    }
    memcpy(raw, item.raw, HID_PROTOCOL_REPORT_SIZE);
    return true;
}

static bool tx_queue_pop_raw(void)
{
    hid_raw_report_t item;
    return spsc_ring_pop(&s_usb_tx_ring, &item);
}

static bool queue_status_event(uint8_t event_type, uint32_t seq, uint32_t status_code)
{
    hid_protocol_report_t report;
    uint8_t raw[HID_PROTOCOL_REPORT_SIZE];

    memset(&report, 0, sizeof(report));
    report.report_id = HID_PROTOCOL_REPORT_ID_DEVICE_EVT;
    report.type = event_type;
    report.seq = seq;
    report.value = status_code;

    if (!hid_protocol_pack_report(&report, raw)) {
        return false;
    }
    if (!tx_queue_push_raw(raw)) {
        s_drop_count++;
        return false;
    }
    return true;
}

static bool fallback_obd_mode01_request(uint8_t mode, uint8_t pid)
{
    can_frame_t request;
    if (mode != OBD_MODE01_CURRENT_DATA) {
        return false;
    }
    if (!obd_mode01_build_request(pid, &request)) {
        return false;
    }
    if (s_callbacks.can_tx == NULL) {
        return false;
    }
    return s_callbacks.can_tx(&request);
}

static bool handle_can_tx_cmd(const hid_protocol_report_t *cmd)
{
    can_frame_t frame;

    if (cmd->dlc > CAN_FRAME_MAX_DATA_LEN) {
        queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_BAD_PAYLOAD);
        return false;
    }

    hid_protocol_to_can_frame(cmd, &frame);

    if ((s_callbacks.can_tx == NULL) || (!s_callbacks.can_tx(&frame))) {
        queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_CAN_TX_FAILED);
        return false;
    }

    return queue_status_event(HID_PROTOCOL_EVT_STATUS, cmd->seq, HID_PROTOCOL_STATUS_OK);
}

static bool handle_obd_request_cmd(const hid_protocol_report_t *cmd)
{
    uint8_t mode = cmd->data[0];
    uint8_t pid = cmd->data[1];
    bool ok;

    if (s_callbacks.obd_request != NULL) {
        ok = s_callbacks.obd_request(mode, pid, cmd->seq);
    } else {
        ok = fallback_obd_mode01_request(mode, pid);
    }

    if (!ok) {
        queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_CAN_TX_FAILED);
        return false;
    }

    return queue_status_event(HID_PROTOCOL_EVT_STATUS, cmd->seq, HID_PROTOCOL_STATUS_OK);
}

static bool handle_set_bitrate_cmd(const hid_protocol_report_t *cmd)
{
    if ((s_callbacks.set_bitrate == NULL) || (!s_callbacks.set_bitrate(cmd->value))) {
        queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_CONFIG_FAILED);
        return false;
    }

    return queue_status_event(HID_PROTOCOL_EVT_STATUS, cmd->seq, HID_PROTOCOL_STATUS_OK);
}

static bool handle_set_filter_cmd(const hid_protocol_report_t *cmd)
{
    bool extended = ((cmd->flags & HID_PROTOCOL_FLAG_EXTENDED_ID) != 0U);

    if ((s_callbacks.set_filter == NULL) || (!s_callbacks.set_filter(cmd->can_id, cmd->value, extended))) {
        queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_CONFIG_FAILED);
        return false;
    }

    return queue_status_event(HID_PROTOCOL_EVT_STATUS, cmd->seq, HID_PROTOCOL_STATUS_OK);
}

static void process_one_host_report(const hid_protocol_report_t *cmd)
{
    if (cmd->report_id != HID_PROTOCOL_REPORT_ID_HOST_CMD) {
        queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_BAD_PAYLOAD);
        return;
    }

    switch (cmd->type) {
    case HID_PROTOCOL_CMD_CAN_TX:
        (void)handle_can_tx_cmd(cmd);
        break;

    case HID_PROTOCOL_CMD_OBD_REQUEST:
        (void)handle_obd_request_cmd(cmd);
        break;

    case HID_PROTOCOL_CMD_SET_BITRATE:
        (void)handle_set_bitrate_cmd(cmd);
        break;

    case HID_PROTOCOL_CMD_SET_FILTER:
        (void)handle_set_filter_cmd(cmd);
        break;

    default:
        (void)queue_status_event(HID_PROTOCOL_EVT_ERROR, cmd->seq, HID_PROTOCOL_STATUS_UNSUPPORTED_CMD);
        break;
    }
}

static void mirror_obd_response_if_present(const hid_can_rx_item_t *item)
{
    uint8_t payload_len;
    uint8_t payload[5];
    uint8_t pid;

    if (item->frame.extended || item->frame.rtr) {
        return;
    }
    if (!obd_mode01_reply_id_is_valid(item->frame.id)) {
        return;
    }
    if ((item->frame.dlc < 4U) || (item->frame.data[1] != (OBD_MODE01_CURRENT_DATA + OBD_MODE01_RESPONSE_OFFSET))) {
        return;
    }

    pid = item->frame.data[2];
    payload_len = (uint8_t)(item->frame.dlc - 3U);
    if (payload_len > sizeof(payload)) {
        payload_len = sizeof(payload);
    }
    memcpy(payload, &item->frame.data[3], payload_len);

    (void)hid_transport_send_obd_response(pid, payload, payload_len, next_event_seq(), item->timestamp_ms);
}

static void drain_can_rx_to_usb_tx_queue(void)
{
    hid_can_rx_item_t item;
    while (spsc_ring_pop(&s_can_rx_ring, &item)) {
        if (!hid_transport_send_frame(&item.frame,
                                      item.timestamp_ms,
                                      next_event_seq(),
                                      HID_PROTOCOL_EVT_CAN_RX)) {
            break;
        }
        mirror_obd_response_if_present(&item);
    }
}

void hid_transport_init(const hid_transport_callbacks_t *callbacks)
{
    memset(&s_callbacks, 0, sizeof(s_callbacks));
    if (callbacks != NULL) {
        s_callbacks = *callbacks;
    }
    s_usb_tx_cb = NULL;
    s_drop_count = 0U;
    s_next_seq = 1U;

    spsc_ring_init(&s_host_rx_ring,
                   s_host_rx_storage,
                   (uint16_t)sizeof(hid_raw_report_t),
                   HID_RX_REPORT_QUEUE_DEPTH);
    spsc_ring_init(&s_usb_tx_ring,
                   s_usb_tx_storage,
                   (uint16_t)sizeof(hid_raw_report_t),
                   HID_TX_REPORT_QUEUE_DEPTH);
    spsc_ring_init(&s_can_rx_ring,
                   s_can_rx_storage,
                   (uint16_t)sizeof(hid_can_rx_item_t),
                   HID_CAN_RX_QUEUE_DEPTH);
}

void hid_transport_set_usb_tx_callback(hid_transport_usb_tx_cb_t usb_tx_cb)
{
    s_usb_tx_cb = usb_tx_cb;
}

bool hid_transport_on_usb_out_report_from_isr(const uint8_t *report, uint16_t len)
{
    hid_raw_report_t item;

    if ((report == NULL) || (len != HID_PROTOCOL_REPORT_SIZE)) {
        s_drop_count++;
        return false;
    }

    memcpy(item.raw, report, sizeof(item.raw));
    if (!spsc_ring_push(&s_host_rx_ring, &item)) {
        s_drop_count++;
        return false;
    }
    return true;
}

bool hid_transport_queue_can_rx_from_isr(const can_frame_t *frame, uint32_t timestamp_ms)
{
    hid_can_rx_item_t item;

    if (!can_frame_is_valid(frame)) {
        return false;
    }

    item.frame = *frame;
    item.timestamp_ms = timestamp_ms;

    if (!spsc_ring_push(&s_can_rx_ring, &item)) {
        s_drop_count++;
        return false;
    }
    return true;
}

void hid_transport_process_host_reports(void)
{
    hid_raw_report_t raw;
    hid_protocol_report_t decoded;

    while (spsc_ring_pop(&s_host_rx_ring, &raw)) {
        if (!hid_protocol_unpack_report(raw.raw, &decoded)) {
            s_drop_count++;
            continue;
        }
        process_one_host_report(&decoded);
    }
}

bool hid_transport_send_frame(const can_frame_t *frame,
                              uint32_t timestamp_ms,
                              uint32_t seq,
                              uint8_t event_type)
{
    hid_protocol_report_t report;
    uint8_t raw[HID_PROTOCOL_REPORT_SIZE];

    if (!can_frame_is_valid(frame)) {
        return false;
    }

    hid_protocol_from_can_frame(frame, event_type, seq, timestamp_ms, &report);
    if (!hid_protocol_pack_report(&report, raw)) {
        return false;
    }
    if (!tx_queue_push_raw(raw)) {
        s_drop_count++;
        return false;
    }
    return true;
}

bool hid_transport_send_obd_response(uint8_t pid,
                                     const uint8_t *payload,
                                     uint8_t payload_len,
                                     uint32_t seq,
                                     uint32_t timestamp_ms)
{
    hid_protocol_report_t report;
    uint8_t raw[HID_PROTOCOL_REPORT_SIZE];
    uint8_t copy_len;

    if ((payload == NULL) && (payload_len != 0U)) {
        return false;
    }

    copy_len = payload_len;
    if (copy_len > 6U) {
        copy_len = 6U;
    }

    memset(&report, 0, sizeof(report));
    report.report_id = HID_PROTOCOL_REPORT_ID_DEVICE_EVT;
    report.type = HID_PROTOCOL_EVT_OBD_RESPONSE;
    report.dlc = (uint8_t)(copy_len + 2U);
    report.seq = seq;
    report.value = timestamp_ms;
    report.data[0] = (uint8_t)(OBD_MODE01_CURRENT_DATA + OBD_MODE01_RESPONSE_OFFSET);
    report.data[1] = pid;
    if (copy_len > 0U) {
        memcpy(&report.data[2], payload, copy_len);
    }

    if (!hid_protocol_pack_report(&report, raw)) {
        return false;
    }
    if (!tx_queue_push_raw(raw)) {
        s_drop_count++;
        return false;
    }
    return true;
}

void hid_transport_poll_tx_queue(void)
{
    uint8_t report[HID_PROTOCOL_REPORT_SIZE];
    uint8_t usb_status;

    drain_can_rx_to_usb_tx_queue();

    if (s_usb_tx_cb == NULL) {
        return;
    }
    if (!tx_queue_peek_raw(report)) {
        return;
    }

    usb_status = s_usb_tx_cb(report, HID_PROTOCOL_REPORT_SIZE);
    if (usb_status == USBD_OK) {
        (void)tx_queue_pop_raw();
    }
}

uint32_t hid_transport_get_drop_count(void)
{
    return s_drop_count;
}
