#include "usb_hid_transport.h"

#include <string.h>

#include "main.h"

#ifndef USBD_OK
#define USBD_OK 0U
#endif

#define HID_RX_REPORT_QUEUE_DEPTH      16U
#define HID_TX_REPORT_QUEUE_DEPTH      32U
#define HID_CAN_RX_QUEUE_DEPTH         32U

#define HID_OFFSET_REPORT_ID           0U
#define HID_OFFSET_TYPE                1U
#define HID_OFFSET_FLAGS               2U
#define HID_OFFSET_DLC                 3U
#define HID_OFFSET_CAN_ID              4U
#define HID_OFFSET_SEQ                 8U
#define HID_OFFSET_VALUE               12U
#define HID_OFFSET_DATA                16U
#define HID_OFFSET_RESERVED            24U

typedef struct {
    uint8_t raw[HID_REPORT_SIZE_BYTES];
} hid_raw_report_t;

typedef struct {
    can_core_frame_t frame;
    uint32_t timestamp_ms;
} hid_can_rx_item_t;

typedef struct {
    hid_raw_report_t items[HID_RX_REPORT_QUEUE_DEPTH];
    volatile uint8_t head;
    volatile uint8_t tail;
} hid_raw_rx_queue_t;

typedef struct {
    hid_raw_report_t items[HID_TX_REPORT_QUEUE_DEPTH];
    volatile uint8_t head;
    volatile uint8_t tail;
} hid_raw_tx_queue_t;

typedef struct {
    hid_can_rx_item_t items[HID_CAN_RX_QUEUE_DEPTH];
    volatile uint8_t head;
    volatile uint8_t tail;
} hid_can_rx_queue_t;

static hid_transport_callbacks_t s_callbacks;
static hid_transport_usb_tx_cb_t s_usb_tx_cb;

static hid_raw_rx_queue_t s_host_rx_queue;
static hid_raw_tx_queue_t s_usb_tx_queue;
static hid_can_rx_queue_t s_can_rx_queue;

static volatile uint32_t s_drop_count;
static uint32_t s_next_seq = 1U;

static uint32_t read_u32_le(const uint8_t *src)
{
    return ((uint32_t)src[0]) |
           (((uint32_t)src[1]) << 8U) |
           (((uint32_t)src[2]) << 16U) |
           (((uint32_t)src[3]) << 24U);
}

static void write_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16U) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static uint8_t queue_next(uint8_t index, uint8_t depth)
{
    return (uint8_t)((index + 1U) % depth);
}

static bool raw_queue_push(hid_raw_report_t *items,
                           volatile uint8_t *head,
                           volatile uint8_t *tail,
                           uint8_t depth,
                           const uint8_t *raw)
{
    uint8_t local_head = *head;
    uint8_t next_head = queue_next(local_head, depth);
    if (next_head == *tail) {
        return false;
    }

    memcpy(items[local_head].raw, raw, HID_REPORT_SIZE_BYTES);
    *head = next_head;
    return true;
}

static bool raw_queue_pop(hid_raw_report_t *items,
                          volatile uint8_t *head,
                          volatile uint8_t *tail,
                          uint8_t depth,
                          hid_raw_report_t *out)
{
    uint8_t local_tail = *tail;
    if (local_tail == *head) {
        return false;
    }

    memcpy(out->raw, items[local_tail].raw, HID_REPORT_SIZE_BYTES);
    *tail = queue_next(local_tail, depth);
    return true;
}

static bool can_rx_queue_push(const hid_can_rx_item_t *item)
{
    uint8_t local_head = s_can_rx_queue.head;
    uint8_t next_head = queue_next(local_head, HID_CAN_RX_QUEUE_DEPTH);
    if (next_head == s_can_rx_queue.tail) {
        return false;
    }

    s_can_rx_queue.items[local_head] = *item;
    s_can_rx_queue.head = next_head;
    return true;
}

static bool can_rx_queue_pop(hid_can_rx_item_t *item)
{
    uint8_t local_tail = s_can_rx_queue.tail;
    if (local_tail == s_can_rx_queue.head) {
        return false;
    }

    *item = s_can_rx_queue.items[local_tail];
    s_can_rx_queue.tail = queue_next(local_tail, HID_CAN_RX_QUEUE_DEPTH);
    return true;
}

static bool tx_queue_push_raw(const uint8_t raw[HID_REPORT_SIZE_BYTES])
{
    return raw_queue_push(s_usb_tx_queue.items,
                          &s_usb_tx_queue.head,
                          &s_usb_tx_queue.tail,
                          HID_TX_REPORT_QUEUE_DEPTH,
                          raw);
}

static bool tx_queue_pop_raw(hid_raw_report_t *raw)
{
    return raw_queue_pop(s_usb_tx_queue.items,
                         &s_usb_tx_queue.head,
                         &s_usb_tx_queue.tail,
                         HID_TX_REPORT_QUEUE_DEPTH,
                         raw);
}

static bool tx_queue_peek_raw(hid_raw_report_t *raw)
{
    uint8_t local_tail = s_usb_tx_queue.tail;
    if (local_tail == s_usb_tx_queue.head) {
        return false;
    }

    memcpy(raw->raw, s_usb_tx_queue.items[local_tail].raw, HID_REPORT_SIZE_BYTES);
    return true;
}

static uint32_t next_event_seq(void)
{
    uint32_t seq = s_next_seq;
    s_next_seq++;
    if (s_next_seq == 0U) {
        s_next_seq = 1U;
    }
    return seq;
}

static bool queue_status_event(uint8_t event_type, uint32_t seq, uint32_t status_code)
{
    hid_transport_report_t report;
    uint8_t raw[HID_REPORT_SIZE_BYTES];

    memset(&report, 0, sizeof(report));
    report.report_id = HID_REPORT_ID_DEVICE_EVENT;
    report.type = event_type;
    report.seq = seq;
    report.value = status_code;

    if (!hid_transport_pack_report(&report, raw)) {
        return false;
    }
    if (!tx_queue_push_raw(raw)) {
        s_drop_count++;
        return false;
    }
    return true;
}

static bool handle_can_tx_cmd(const hid_transport_report_t *cmd)
{
    can_core_frame_t frame;

    if (cmd->dlc > CAN_CORE_MAX_DATA_LEN) {
        queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_BAD_PAYLOAD);
        return false;
    }

    memset(&frame, 0, sizeof(frame));
    frame.id = cmd->can_id;
    frame.extended = ((cmd->flags & HID_FLAG_EXTENDED_ID) != 0U);
    frame.rtr = ((cmd->flags & HID_FLAG_RTR) != 0U);
    frame.dlc = cmd->dlc;
    memcpy(frame.data, cmd->data, cmd->dlc);

    if ((s_callbacks.can_tx == NULL) || (!s_callbacks.can_tx(&frame))) {
        queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_CAN_TX_FAILED);
        return false;
    }

    return queue_status_event(HID_EVENT_STATUS, cmd->seq, HID_STATUS_OK);
}

static bool fallback_obd_mode01_request(uint8_t mode, uint8_t pid)
{
    can_core_frame_t request;
    if (mode != CAN_OBD_MODE_CURRENT_DATA) {
        return false;
    }
    if (!can_obd_build_mode01_request(pid, &request)) {
        return false;
    }
    if (s_callbacks.can_tx == NULL) {
        return false;
    }
    return s_callbacks.can_tx(&request);
}

static bool handle_obd_request_cmd(const hid_transport_report_t *cmd)
{
    uint8_t mode = cmd->data[0];
    uint8_t pid = cmd->data[1];
    bool ok = false;

    if (s_callbacks.obd_request != NULL) {
        ok = s_callbacks.obd_request(mode, pid, cmd->seq);
    } else {
        ok = fallback_obd_mode01_request(mode, pid);
    }

    if (!ok) {
        queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_CAN_TX_FAILED);
        return false;
    }

    return queue_status_event(HID_EVENT_STATUS, cmd->seq, HID_STATUS_OK);
}

static bool handle_set_bitrate_cmd(const hid_transport_report_t *cmd)
{
    if ((s_callbacks.set_bitrate == NULL) || (!s_callbacks.set_bitrate(cmd->value))) {
        queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_CONFIG_FAILED);
        return false;
    }

    return queue_status_event(HID_EVENT_STATUS, cmd->seq, HID_STATUS_OK);
}

static bool handle_set_filter_cmd(const hid_transport_report_t *cmd)
{
    bool extended = ((cmd->flags & HID_FLAG_EXTENDED_ID) != 0U);
    uint32_t mask = cmd->value;

    if ((s_callbacks.set_filter == NULL) || (!s_callbacks.set_filter(cmd->can_id, mask, extended))) {
        queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_CONFIG_FAILED);
        return false;
    }

    return queue_status_event(HID_EVENT_STATUS, cmd->seq, HID_STATUS_OK);
}

static void process_one_host_report(const hid_transport_report_t *cmd)
{
    if (cmd->report_id != HID_REPORT_ID_HOST_COMMAND) {
        queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_BAD_PAYLOAD);
        return;
    }

    switch (cmd->type) {
    case HID_CMD_CAN_TX:
        (void)handle_can_tx_cmd(cmd);
        break;

    case HID_CMD_OBD_REQUEST:
        (void)handle_obd_request_cmd(cmd);
        break;

    case HID_CMD_SET_BITRATE:
        (void)handle_set_bitrate_cmd(cmd);
        break;

    case HID_CMD_SET_FILTER:
        (void)handle_set_filter_cmd(cmd);
        break;

    default:
        (void)queue_status_event(HID_EVENT_ERROR, cmd->seq, HID_STATUS_UNSUPPORTED_CMD);
        break;
    }
}

static void mirror_obd_response_if_present(const hid_can_rx_item_t *item)
{
    uint8_t payload[5];
    uint8_t payload_len = 0U;
    uint8_t pid;

    if (item->frame.extended || item->frame.rtr) {
        return;
    }
    if (!can_obd_reply_id_is_valid(item->frame.id)) {
        return;
    }
    if ((item->frame.dlc < 4U) || (item->frame.data[1] != (CAN_OBD_MODE_CURRENT_DATA + CAN_OBD_MODE_RESPONSE_OFFSET))) {
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
    while (can_rx_queue_pop(&item)) {
        if (!hid_transport_send_frame(&item.frame, item.timestamp_ms, next_event_seq(), HID_EVENT_CAN_RX)) {
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
    memset(&s_host_rx_queue, 0, sizeof(s_host_rx_queue));
    memset(&s_usb_tx_queue, 0, sizeof(s_usb_tx_queue));
    memset(&s_can_rx_queue, 0, sizeof(s_can_rx_queue));
    s_drop_count = 0U;
    s_next_seq = 1U;
}

void hid_transport_set_usb_tx_callback(hid_transport_usb_tx_cb_t usb_tx_cb)
{
    s_usb_tx_cb = usb_tx_cb;
}

bool hid_transport_pack_report(const hid_transport_report_t *report, uint8_t out_raw[HID_REPORT_SIZE_BYTES])
{
    if ((report == NULL) || (out_raw == NULL)) {
        return false;
    }

    memset(out_raw, 0, HID_REPORT_SIZE_BYTES);
    out_raw[HID_OFFSET_REPORT_ID] = report->report_id;
    out_raw[HID_OFFSET_TYPE] = report->type;
    out_raw[HID_OFFSET_FLAGS] = report->flags;
    out_raw[HID_OFFSET_DLC] = report->dlc;
    write_u32_le(&out_raw[HID_OFFSET_CAN_ID], report->can_id);
    write_u32_le(&out_raw[HID_OFFSET_SEQ], report->seq);
    write_u32_le(&out_raw[HID_OFFSET_VALUE], report->value);
    memcpy(&out_raw[HID_OFFSET_DATA], report->data, sizeof(report->data));
    memcpy(&out_raw[HID_OFFSET_RESERVED], report->reserved, sizeof(report->reserved));
    return true;
}

bool hid_transport_unpack_report(const uint8_t raw[HID_REPORT_SIZE_BYTES], hid_transport_report_t *report)
{
    if ((raw == NULL) || (report == NULL)) {
        return false;
    }

    memset(report, 0, sizeof(*report));
    report->report_id = raw[HID_OFFSET_REPORT_ID];
    report->type = raw[HID_OFFSET_TYPE];
    report->flags = raw[HID_OFFSET_FLAGS];
    report->dlc = raw[HID_OFFSET_DLC];
    report->can_id = read_u32_le(&raw[HID_OFFSET_CAN_ID]);
    report->seq = read_u32_le(&raw[HID_OFFSET_SEQ]);
    report->value = read_u32_le(&raw[HID_OFFSET_VALUE]);
    memcpy(report->data, &raw[HID_OFFSET_DATA], sizeof(report->data));
    memcpy(report->reserved, &raw[HID_OFFSET_RESERVED], sizeof(report->reserved));
    return true;
}

bool hid_transport_on_usb_out_report_from_isr(const uint8_t *report, uint16_t len)
{
    if ((report == NULL) || (len != HID_REPORT_SIZE_BYTES)) {
        s_drop_count++;
        return false;
    }

    if (!raw_queue_push(s_host_rx_queue.items,
                        &s_host_rx_queue.head,
                        &s_host_rx_queue.tail,
                        HID_RX_REPORT_QUEUE_DEPTH,
                        report)) {
        s_drop_count++;
        return false;
    }
    return true;
}

void hid_transport_process_host_reports(void)
{
    hid_raw_report_t raw;
    hid_transport_report_t decoded;

    while (raw_queue_pop(s_host_rx_queue.items,
                         &s_host_rx_queue.head,
                         &s_host_rx_queue.tail,
                         HID_RX_REPORT_QUEUE_DEPTH,
                         &raw)) {
        if (!hid_transport_unpack_report(raw.raw, &decoded)) {
            s_drop_count++;
            continue;
        }
        process_one_host_report(&decoded);
    }
}

bool hid_transport_queue_can_rx_from_isr(const can_core_frame_t *frame, uint32_t timestamp_ms)
{
    hid_can_rx_item_t item;

    if (frame == NULL) {
        return false;
    }

    item.frame = *frame;
    item.timestamp_ms = timestamp_ms;

    if (!can_rx_queue_push(&item)) {
        s_drop_count++;
        return false;
    }
    return true;
}

bool hid_transport_send_frame(const can_core_frame_t *frame,
                              uint32_t timestamp_ms,
                              uint32_t seq,
                              uint8_t event_type)
{
    hid_transport_report_t report;
    uint8_t raw[HID_REPORT_SIZE_BYTES];

    if ((frame == NULL) || (frame->dlc > CAN_CORE_MAX_DATA_LEN)) {
        return false;
    }

    memset(&report, 0, sizeof(report));
    report.report_id = HID_REPORT_ID_DEVICE_EVENT;
    report.type = event_type;
    report.flags = 0U;
    if (frame->extended) {
        report.flags |= HID_FLAG_EXTENDED_ID;
    }
    if (frame->rtr) {
        report.flags |= HID_FLAG_RTR;
    }
    report.dlc = frame->dlc;
    report.can_id = frame->id;
    report.seq = seq;
    report.value = timestamp_ms;
    memcpy(report.data, frame->data, frame->dlc);

    if (!hid_transport_pack_report(&report, raw)) {
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
    hid_transport_report_t report;
    uint8_t raw[HID_REPORT_SIZE_BYTES];
    uint8_t copy_len;

    if ((payload == NULL) && (payload_len != 0U)) {
        return false;
    }

    copy_len = payload_len;
    if (copy_len > 6U) {
        copy_len = 6U;
    }

    memset(&report, 0, sizeof(report));
    report.report_id = HID_REPORT_ID_DEVICE_EVENT;
    report.type = HID_EVENT_OBD_RESPONSE;
    report.dlc = (uint8_t)(copy_len + 2U);
    report.seq = seq;
    report.value = timestamp_ms;
    report.data[0] = (uint8_t)(CAN_OBD_MODE_CURRENT_DATA + CAN_OBD_MODE_RESPONSE_OFFSET);
    report.data[1] = pid;
    if (copy_len > 0U) {
        memcpy(&report.data[2], payload, copy_len);
    }

    if (!hid_transport_pack_report(&report, raw)) {
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
    hid_raw_report_t report;
    uint8_t usb_status;

    drain_can_rx_to_usb_tx_queue();

    if (s_usb_tx_cb == NULL) {
        return;
    }

    if (!tx_queue_peek_raw(&report)) {
        return;
    }

    usb_status = s_usb_tx_cb(report.raw, HID_REPORT_SIZE_BYTES);
    if (usb_status == USBD_OK) {
        (void)tx_queue_pop_raw(&report);
    }
}

uint32_t hid_transport_get_drop_count(void)
{
    return s_drop_count;
}
