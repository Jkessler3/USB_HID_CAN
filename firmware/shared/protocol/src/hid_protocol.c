#include "hid_protocol.h"

#include <string.h>

enum {
    HID_OFFSET_REPORT_ID = 0,
    HID_OFFSET_TYPE = 1,
    HID_OFFSET_FLAGS = 2,
    HID_OFFSET_DLC = 3,
    HID_OFFSET_CAN_ID = 4,
    HID_OFFSET_SEQ = 8,
    HID_OFFSET_VALUE = 12,
    HID_OFFSET_DATA = 16,
    HID_OFFSET_RESERVED = 24
};

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

bool hid_protocol_pack_report(const hid_protocol_report_t *report,
                              uint8_t out_raw[HID_PROTOCOL_REPORT_SIZE])
{
    if ((report == NULL) || (out_raw == NULL)) {
        return false;
    }

    memset(out_raw, 0, HID_PROTOCOL_REPORT_SIZE);
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

bool hid_protocol_unpack_report(const uint8_t raw[HID_PROTOCOL_REPORT_SIZE],
                                hid_protocol_report_t *report)
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

void hid_protocol_from_can_frame(const can_frame_t *frame,
                                 uint8_t event_type,
                                 uint32_t seq,
                                 uint32_t value,
                                 hid_protocol_report_t *report)
{
    if ((frame == NULL) || (report == NULL)) {
        return;
    }

    memset(report, 0, sizeof(*report));
    report->report_id = HID_PROTOCOL_REPORT_ID_DEVICE_EVT;
    report->type = event_type;
    report->flags = 0U;
    if (frame->extended) {
        report->flags |= HID_PROTOCOL_FLAG_EXTENDED_ID;
    }
    if (frame->rtr) {
        report->flags |= HID_PROTOCOL_FLAG_RTR;
    }
    report->dlc = frame->dlc;
    report->can_id = frame->id;
    report->seq = seq;
    report->value = value;
    memcpy(report->data, frame->data, frame->dlc);
}

void hid_protocol_to_can_frame(const hid_protocol_report_t *report, can_frame_t *frame)
{
    if ((report == NULL) || (frame == NULL)) {
        return;
    }

    can_frame_clear(frame);
    frame->id = report->can_id;
    frame->extended = ((report->flags & HID_PROTOCOL_FLAG_EXTENDED_ID) != 0U);
    frame->rtr = ((report->flags & HID_PROTOCOL_FLAG_RTR) != 0U);
    frame->dlc = (report->dlc <= CAN_FRAME_MAX_DATA_LEN) ? report->dlc : CAN_FRAME_MAX_DATA_LEN;
    memcpy(frame->data, report->data, frame->dlc);
}
