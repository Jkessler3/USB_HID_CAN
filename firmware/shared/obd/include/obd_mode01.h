#ifndef OBD_MODE01_H
#define OBD_MODE01_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OBD_MODE01_REQUEST_ID           0x7DFU
#define OBD_MODE01_REPLY_ID_MIN         0x7E8U
#define OBD_MODE01_REPLY_ID_MAX         0x7EFU

#define OBD_MODE01_CURRENT_DATA         0x01U
#define OBD_MODE01_RESPONSE_OFFSET      0x40U

#define OBD_MODE01_PID_RPM              0x0CU
#define OBD_MODE01_PID_SPEED            0x0DU
#define OBD_MODE01_PID_COOLANT_TEMP     0x05U

bool obd_mode01_reply_id_is_valid(uint32_t can_id);

bool obd_mode01_build_request(uint8_t pid, can_frame_t *frame);

bool obd_mode01_extract_payload(const can_frame_t *frame,
                                uint8_t expected_pid,
                                uint8_t *payload,
                                size_t payload_capacity,
                                uint8_t *payload_len);

bool obd_mode01_decode_rpm(const uint8_t *payload, size_t payload_len, uint16_t *rpm);
bool obd_mode01_decode_speed(const uint8_t *payload, size_t payload_len, uint8_t *speed_kph);
bool obd_mode01_decode_coolant_temp(const uint8_t *payload, size_t payload_len, int8_t *coolant_c);

#ifdef __cplusplus
}
#endif

#endif /* OBD_MODE01_H */
