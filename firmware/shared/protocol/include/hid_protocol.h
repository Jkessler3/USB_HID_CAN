#ifndef HID_PROTOCOL_H
#define HID_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HID_PROTOCOL_REPORT_SIZE            64U
#define HID_PROTOCOL_PAYLOAD_BYTES          63U

#define HID_PROTOCOL_REPORT_ID_HOST_CMD     0x01U
#define HID_PROTOCOL_REPORT_ID_DEVICE_EVT   0x81U

#define HID_PROTOCOL_FLAG_EXTENDED_ID       (1U << 0)
#define HID_PROTOCOL_FLAG_RTR               (1U << 1)

typedef enum {
    HID_PROTOCOL_CMD_CAN_TX = 0x01U,
    HID_PROTOCOL_CMD_OBD_REQUEST = 0x02U,
    HID_PROTOCOL_CMD_SET_BITRATE = 0x03U,
    HID_PROTOCOL_CMD_SET_FILTER = 0x04U
} hid_protocol_cmd_t;

typedef enum {
    HID_PROTOCOL_EVT_CAN_RX = 0x01U,
    HID_PROTOCOL_EVT_OBD_RESPONSE = 0x02U,
    HID_PROTOCOL_EVT_STATUS = 0x03U,
    HID_PROTOCOL_EVT_ERROR = 0x7FU
} hid_protocol_evt_t;

typedef enum {
    HID_PROTOCOL_STATUS_OK = 0x00000000U,
    HID_PROTOCOL_STATUS_UNSUPPORTED_CMD = 0x00000001U,
    HID_PROTOCOL_STATUS_BAD_PAYLOAD = 0x00000002U,
    HID_PROTOCOL_STATUS_CAN_TX_FAILED = 0x00000003U,
    HID_PROTOCOL_STATUS_QUEUE_OVERFLOW = 0x00000004U,
    HID_PROTOCOL_STATUS_CONFIG_FAILED = 0x00000005U
} hid_protocol_status_t;

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
} hid_protocol_report_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(hid_protocol_report_t) == HID_PROTOCOL_REPORT_SIZE, "HID report must stay 64 bytes");
#endif

bool hid_protocol_pack_report(const hid_protocol_report_t *report,
                              uint8_t out_raw[HID_PROTOCOL_REPORT_SIZE]);

bool hid_protocol_unpack_report(const uint8_t raw[HID_PROTOCOL_REPORT_SIZE],
                                hid_protocol_report_t *report);

void hid_protocol_from_can_frame(const can_frame_t *frame,
                                 uint8_t event_type,
                                 uint32_t seq,
                                 uint32_t value,
                                 hid_protocol_report_t *report);

void hid_protocol_to_can_frame(const hid_protocol_report_t *report, can_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* HID_PROTOCOL_H */
