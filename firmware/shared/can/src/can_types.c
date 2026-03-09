#include "can_types.h"

#include <string.h>

bool can_frame_is_valid(const can_frame_t *frame)
{
    if (frame == NULL) {
        return false;
    }
    return (frame->dlc <= CAN_FRAME_MAX_DATA_LEN);
}

void can_frame_clear(can_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
}
