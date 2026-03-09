#include "obd_mode01.h"

#include <string.h>

bool obd_mode01_reply_id_is_valid(uint32_t can_id)
{
    return (can_id >= OBD_MODE01_REPLY_ID_MIN) && (can_id <= OBD_MODE01_REPLY_ID_MAX);
}

bool obd_mode01_build_request(uint8_t pid, can_frame_t *frame)
{
    if (frame == NULL) {
        return false;
    }

    can_frame_clear(frame);
    frame->id = OBD_MODE01_REQUEST_ID;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = CAN_FRAME_MAX_DATA_LEN;
    frame->data[0] = 0x02U;
    frame->data[1] = OBD_MODE01_CURRENT_DATA;
    frame->data[2] = pid;
    return true;
}

bool obd_mode01_extract_payload(const can_frame_t *frame,
                                uint8_t expected_pid,
                                uint8_t *payload,
                                size_t payload_capacity,
                                uint8_t *payload_len)
{
    uint8_t len = 0U;
    uint8_t expected_mode = (uint8_t)(OBD_MODE01_CURRENT_DATA + OBD_MODE01_RESPONSE_OFFSET);

    if ((frame == NULL) || (payload == NULL) || (payload_len == NULL)) {
        return false;
    }

    *payload_len = 0U;

    if (frame->extended || frame->rtr) {
        return false;
    }
    if (!obd_mode01_reply_id_is_valid(frame->id)) {
        return false;
    }
    if ((frame->dlc < 4U) || (frame->dlc > CAN_FRAME_MAX_DATA_LEN)) {
        return false;
    }
    if ((frame->data[1] != expected_mode) || (frame->data[2] != expected_pid)) {
        return false;
    }

    if (expected_pid == OBD_MODE01_PID_RPM) {
        len = 2U;
    } else if ((expected_pid == OBD_MODE01_PID_SPEED) || (expected_pid == OBD_MODE01_PID_COOLANT_TEMP)) {
        len = 1U;
    } else {
        len = 1U;
    }

    if ((frame->dlc < (uint8_t)(3U + len)) || (payload_capacity < len)) {
        return false;
    }

    memcpy(payload, &frame->data[3], len);
    *payload_len = len;
    return true;
}

bool obd_mode01_decode_rpm(const uint8_t *payload, size_t payload_len, uint16_t *rpm)
{
    if ((payload == NULL) || (rpm == NULL) || (payload_len < 2U)) {
        return false;
    }

    *rpm = (uint16_t)((((uint16_t)payload[0]) << 8U) | payload[1]);
    *rpm /= 4U;
    return true;
}

bool obd_mode01_decode_speed(const uint8_t *payload, size_t payload_len, uint8_t *speed_kph)
{
    if ((payload == NULL) || (speed_kph == NULL) || (payload_len < 1U)) {
        return false;
    }

    *speed_kph = payload[0];
    return true;
}

bool obd_mode01_decode_coolant_temp(const uint8_t *payload, size_t payload_len, int8_t *coolant_c)
{
    if ((payload == NULL) || (coolant_c == NULL) || (payload_len < 1U)) {
        return false;
    }

    *coolant_c = (int8_t)((int16_t)payload[0] - 40);
    return true;
}
