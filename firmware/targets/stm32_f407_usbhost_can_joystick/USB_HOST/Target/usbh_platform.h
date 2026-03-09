/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbh_platform.h
  * @brief          : Header for usbh_platform.c file.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_PLATFORM_H__
#define __USBH_PLATFORM_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usb_host.h"

/* USER CODE BEGIN INCLUDE */
/* Set to 1 only when the board has a real GPIO-controlled USB VBUS switch. */
#ifndef USBH_HAS_VBUS_SWITCH
#define USBH_HAS_VBUS_SWITCH 0
#endif

/* USER CODE END INCLUDE */

void MX_DriverVbusFS(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* __USBH_PLATFORM_H__ */

