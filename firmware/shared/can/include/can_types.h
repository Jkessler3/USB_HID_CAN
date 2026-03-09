#ifndef CAN_TYPES_H
#define CAN_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_FRAME_MAX_DATA_LEN 8U

typedef struct {
    uint32_t id;
    bool extended;
    bool rtr;
    uint8_t dlc;
    uint8_t data[CAN_FRAME_MAX_DATA_LEN];
} can_frame_t;

bool can_frame_is_valid(const can_frame_t *frame);
void can_frame_clear(can_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* CAN_TYPES_H */
