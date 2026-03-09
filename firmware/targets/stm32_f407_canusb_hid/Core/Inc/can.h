#ifndef __CAN_H
#define __CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

extern CAN_HandleTypeDef hcan1;

void MX_CAN1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H */
