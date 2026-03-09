#ifndef APP_CAN_H
#define APP_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_can_rx_callback_t)(const can_frame_t *frame, uint32_t timestamp_ms);

void app_can_init(app_can_rx_callback_t rx_callback);
bool app_can_start(void);

bool app_can_send_frame(const can_frame_t *frame);
bool app_can_set_bitrate(uint32_t bitrate_bps);
bool app_can_set_filter(uint32_t can_id, uint32_t mask, bool extended);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_H */
