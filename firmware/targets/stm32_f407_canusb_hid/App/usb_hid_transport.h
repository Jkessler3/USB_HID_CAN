#ifndef USB_HID_TRANSPORT_H
#define USB_HID_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*hid_transport_can_tx_cb_t)(const can_frame_t *frame);
typedef bool (*hid_transport_obd_request_cb_t)(uint8_t mode, uint8_t pid, uint32_t seq);
typedef bool (*hid_transport_set_bitrate_cb_t)(uint32_t bitrate_bps);
typedef bool (*hid_transport_set_filter_cb_t)(uint32_t can_id, uint32_t mask, bool extended);
typedef uint8_t (*hid_transport_usb_tx_cb_t)(uint8_t *report, uint16_t len);

typedef struct {
    hid_transport_can_tx_cb_t can_tx;
    hid_transport_obd_request_cb_t obd_request;
    hid_transport_set_bitrate_cb_t set_bitrate;
    hid_transport_set_filter_cb_t set_filter;
} hid_transport_callbacks_t;

void hid_transport_init(const hid_transport_callbacks_t *callbacks);
void hid_transport_set_usb_tx_callback(hid_transport_usb_tx_cb_t usb_tx_cb);

bool hid_transport_on_usb_out_report_from_isr(const uint8_t *report, uint16_t len);
bool hid_transport_queue_can_rx_from_isr(const can_frame_t *frame, uint32_t timestamp_ms);

void hid_transport_process_host_reports(void);

bool hid_transport_send_frame(const can_frame_t *frame,
                              uint32_t timestamp_ms,
                              uint32_t seq,
                              uint8_t event_type);

bool hid_transport_send_obd_response(uint8_t pid,
                                     const uint8_t *payload,
                                     uint8_t payload_len,
                                     uint32_t seq,
                                     uint32_t timestamp_ms);

void hid_transport_poll_tx_queue(void);

uint32_t hid_transport_get_drop_count(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_HID_TRANSPORT_H */
