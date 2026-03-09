#ifndef USB_HID_TRANSPORT_H
#define USB_HID_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "can_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HID_REPORT_SIZE_BYTES          64U
#define HID_REPORT_PAYLOAD_BYTES       63U

#define HID_REPORT_ID_HOST_COMMAND     0x01U
#define HID_REPORT_ID_DEVICE_EVENT     0x81U

#define HID_FLAG_EXTENDED_ID           (1U << 0)
#define HID_FLAG_RTR                   (1U << 1)

typedef enum {
    HID_CMD_CAN_TX = 0x01U,
    HID_CMD_OBD_REQUEST = 0x02U,
    HID_CMD_SET_BITRATE = 0x03U,
    HID_CMD_SET_FILTER = 0x04U
} hid_transport_cmd_t;

typedef enum {
    HID_EVENT_CAN_RX = 0x01U,
    HID_EVENT_OBD_RESPONSE = 0x02U,
    HID_EVENT_STATUS = 0x03U,
    HID_EVENT_ERROR = 0x7FU
} hid_transport_event_t;

typedef enum {
    HID_STATUS_OK = 0x00000000U,
    HID_STATUS_UNSUPPORTED_CMD = 0x00000001U,
    HID_STATUS_BAD_PAYLOAD = 0x00000002U,
    HID_STATUS_CAN_TX_FAILED = 0x00000003U,
    HID_STATUS_QUEUE_OVERFLOW = 0x00000004U,
    HID_STATUS_CONFIG_FAILED = 0x00000005U
} hid_transport_status_t;

typedef struct {
    uint8_t report_id;
    uint8_t type;
    uint8_t flags;
    uint8_t dlc;
    uint32_t can_id;
    uint32_t seq;
    uint32_t value;
    uint8_t data[8];
    uint8_t reserved[40];
} hid_transport_report_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(hid_transport_report_t) == HID_REPORT_SIZE_BYTES, "HID report must stay 64 bytes");
#endif

typedef bool (*hid_transport_can_tx_cb_t)(const can_core_frame_t *frame);
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

bool hid_transport_pack_report(const hid_transport_report_t *report, uint8_t out_raw[HID_REPORT_SIZE_BYTES]);
bool hid_transport_unpack_report(const uint8_t raw[HID_REPORT_SIZE_BYTES], hid_transport_report_t *report);

bool hid_transport_on_usb_out_report_from_isr(const uint8_t *report, uint16_t len);
void hid_transport_process_host_reports(void);

bool hid_transport_queue_can_rx_from_isr(const can_core_frame_t *frame, uint32_t timestamp_ms);

bool hid_transport_send_frame(const can_core_frame_t *frame,
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
