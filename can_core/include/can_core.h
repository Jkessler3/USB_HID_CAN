#ifndef CAN_CORE_H
#define CAN_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_CORE_MAX_DATA_LEN           8U

#define CAN_OBD_REQUEST_ID              0x7DFU
#define CAN_OBD_REPLY_ID_MIN            0x7E8U
#define CAN_OBD_REPLY_ID_MAX            0x7EFU

#define CAN_OBD_MODE_CURRENT_DATA       0x01U
#define CAN_OBD_MODE_RESPONSE_OFFSET    0x40U

#define CAN_OBD_PID_RPM                 0x0CU
#define CAN_OBD_PID_SPEED               0x0DU
#define CAN_OBD_PID_COOLANT_TEMP        0x05U

typedef struct {
    uint32_t id;
    bool extended;
    bool rtr;
    uint8_t dlc;
    uint8_t data[CAN_CORE_MAX_DATA_LEN];
} can_core_frame_t;

bool can_obd_reply_id_is_valid(uint32_t can_id);
bool can_obd_build_mode01_request(uint8_t pid, can_core_frame_t *frame);

bool can_obd_extract_mode01_payload(const can_core_frame_t *frame,
                                    uint8_t expected_pid,
                                    uint8_t *payload,
                                    size_t payload_capacity,
                                    uint8_t *payload_len);

bool can_obd_decode_rpm(const uint8_t *payload, size_t payload_len, uint16_t *rpm);
bool can_obd_decode_speed(const uint8_t *payload, size_t payload_len, uint8_t *speed_kph);
bool can_obd_decode_coolant_temp(const uint8_t *payload, size_t payload_len, int8_t *coolant_c);

#endif
