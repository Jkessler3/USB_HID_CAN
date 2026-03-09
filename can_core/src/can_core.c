#include "can_core.h"

#include <string.h>

bool can_obd_reply_id_is_valid(uint32_t can_id)
{
    return (can_id >= CAN_OBD_REPLY_ID_MIN) && (can_id <= CAN_OBD_REPLY_ID_MAX);
}

bool can_obd_build_mode01_request(uint8_t pid, can_core_frame_t *frame)
{
    if (frame == NULL) {
        return false;
    }

    memset(frame, 0, sizeof(*frame));
    frame->id = CAN_OBD_REQUEST_ID;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = CAN_CORE_MAX_DATA_LEN;
    frame->data[0] = 0x02U;
    frame->data[1] = CAN_OBD_MODE_CURRENT_DATA;
    frame->data[2] = pid;
    return true;
}

bool can_obd_extract_mode01_payload(const can_core_frame_t *frame,
                                    uint8_t expected_pid,
                                    uint8_t *payload,
                                    size_t payload_capacity,
                                    uint8_t *payload_len)
{
    if ((frame == NULL) || (payload == NULL) || (payload_len == NULL)) {
        return false;
    }

    *payload_len = 0U;

    if (frame->extended || frame->rtr) {
        return false;
    }
    if (!can_obd_reply_id_is_valid(frame->id)) {
        return false;
    }
    if (frame->dlc < 4U || frame->dlc > CAN_CORE_MAX_DATA_LEN) {
        return false;
    }

    const uint8_t expected_mode = (uint8_t)(CAN_OBD_MODE_CURRENT_DATA + CAN_OBD_MODE_RESPONSE_OFFSET);
    if ((frame->data[1] != expected_mode) || (frame->data[2] != expected_pid)) {
        return false;
    }

    uint8_t len = 0U;
    if (expected_pid == CAN_OBD_PID_RPM) {
        len = 2U;
    } else if ((expected_pid == CAN_OBD_PID_SPEED) || (expected_pid == CAN_OBD_PID_COOLANT_TEMP)) {
        len = 1U;
    } else {
        // Caller can still parse unknown PIDs as single-byte payload by default.
        len = 1U;
    }

    if (frame->dlc < (uint8_t)(3U + len) || payload_capacity < len) {
        return false;
    }

    memcpy(payload, &frame->data[3], len);
    *payload_len = len;
    return true;
}

bool can_obd_decode_rpm(const uint8_t *payload, size_t payload_len, uint16_t *rpm)
{
    if ((payload == NULL) || (rpm == NULL) || (payload_len < 2U)) {
        return false;
    }
    *rpm = (uint16_t)((((uint16_t)payload[0]) << 8U) | payload[1]);
    *rpm /= 4U;
    return true;
}

bool can_obd_decode_speed(const uint8_t *payload, size_t payload_len, uint8_t *speed_kph)
{
    if ((payload == NULL) || (speed_kph == NULL) || (payload_len < 1U)) {
        return false;
    }
    *speed_kph = payload[0];
    return true;
}

bool can_obd_decode_coolant_temp(const uint8_t *payload, size_t payload_len, int8_t *coolant_c)
{
    if ((payload == NULL) || (coolant_c == NULL) || (payload_len < 1U)) {
        return false;
    }
    // SAE J1979 PID 0x05: A - 40
    *coolant_c = (int8_t)((int16_t)payload[0] - 40);
    return true;
}
