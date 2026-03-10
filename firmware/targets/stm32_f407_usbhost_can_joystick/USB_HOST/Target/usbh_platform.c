/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbh_platform.c

  * @brief          : This file implements the USB platform
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbh_platform.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

/**
  * @brief  Drive VBUS.
  * @param  state : VBUS state
  *          This parameter can be one of the these values:
  *           - 1 : VBUS Active
  *           - 0 : VBUS Inactive
  */
void MX_DriverVbusFS(uint8_t state)
{
  uint8_t data = state;
  /* USER CODE BEGIN PREPARE_GPIO_DATA_VBUS_FS */
#if (USBH_HAS_VBUS_SWITCH == 0)
  /* No board-side VBUS switch is present: keep this function as a no-op. */
  (void)state;
  return;
#else
  if(state == 0U)
  {
#if (USBH_VBUS_SWITCH_ACTIVE_HIGH == 1)
    data = GPIO_PIN_RESET;
#else
    data = GPIO_PIN_SET;
#endif
  }
  else
  {
#if (USBH_VBUS_SWITCH_ACTIVE_HIGH == 1)
    data = GPIO_PIN_SET;
#else
    data = GPIO_PIN_RESET;
#endif
  }
#endif
  /* USER CODE END PREPARE_GPIO_DATA_VBUS_FS */
  HAL_GPIO_WritePin(GPIOC,GPIO_PIN_9,(GPIO_PinState)data);
}

