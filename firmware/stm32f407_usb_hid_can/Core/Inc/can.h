#ifndef __CAN_H
#define __CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "can_core.h"
#include "stm32f4xx_hal.h"

extern CAN_HandleTypeDef hcan1;

void MX_CAN1_Init(void);

bool can1_send_core_frame(const can_core_frame_t *frame);
bool can1_apply_bitrate(uint32_t bitrate_bps);
bool can1_apply_filter(uint32_t can_id, uint32_t mask, bool extended);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H */
