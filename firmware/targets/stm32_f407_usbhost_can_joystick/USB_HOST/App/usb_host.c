/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file            : usb_host.c
  * @version         : v1.0_Cube
  * @brief           : This file implements the USB Host
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

#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>

/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * user callback declaration
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Init USB host library, add supported class and start the library
  * @retval None
  */
void MX_USB_HOST_Init(void)
{
  /* USER CODE BEGIN USB_HOST_Init_PreTreatment */

  /* USER CODE END USB_HOST_Init_PreTreatment */

  /* Init host Library, add supported class and start the library. */
  if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_RegisterClass(&hUsbHostFS, USBH_HID_CLASS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_Start(&hUsbHostFS) != USBH_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_HOST_Init_PostTreatment */

  /* USER CODE END USB_HOST_Init_PostTreatment */
}

/*
 * Background task
 */
void MX_USB_HOST_Process(void)
{
  /* USB Host Background task */
  USBH_Process(&hUsbHostFS);
}
/*
 * user callback definition
 */
static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id)
{
  /* USER CODE BEGIN CALL_BACK_1 */
  switch(id)
  {
  case HOST_USER_CLASS_SELECTED:             // a class was chosen (e.g., HID)
      {
        printf("USB: class selected\r\n");
        // Helpful introspection:
        USBH_DevDescTypeDef *dd = &phost->device.DevDesc;
        printf("VID=%04X PID=%04X speed=%u ifaces=%d\r\n",
               dd->idVendor, dd->idProduct, phost->device.speed,
               phost->device.CfgDesc.bNumInterfaces);
        for (uint8_t i = 0; i < phost->device.CfgDesc.bNumInterfaces; i++) {
          const USBH_InterfaceDescTypeDef *itf = &phost->device.CfgDesc.Itf_Desc[i];
          printf("IF[%u]: class=0x%02X sub=0x%02X proto=0x%02X\r\n",
                 i, itf->bInterfaceClass, itf->bInterfaceSubClass, itf->bInterfaceProtocol);
        }
        break;
      }

  case HOST_USER_DISCONNECTION:
  Appli_state = APPLICATION_DISCONNECT;
  	  printf("USB: disconnect\r\n");
  break;

  case HOST_USER_CLASS_ACTIVE:
    {
      HID_HandleTypeDef *hid = NULL;
      Appli_state = APPLICATION_READY;
      printf("USB: class active\r\n");

      if ((phost->pActiveClass != NULL) && (phost->pActiveClass->pData != NULL)) {
        hid = (HID_HandleTypeDef *)phost->pActiveClass->pData;
        printf("USB HID ready: in_ep=0x%02X out_ep=0x%02X report_len=%u poll_ms=%u\r\n",
               hid->InEp,
               hid->OutEp,
               hid->length,
               USBH_HID_GetPollInterval(phost));
      } else {
        printf("USB HID ready: handle unavailable\r\n");
      }
      break;
    }

  case HOST_USER_CONNECTION:
  Appli_state = APPLICATION_START;
  	  printf("USB: connect\r\n");
  break;

  default:
  break;
  }
  /* USER CODE END CALL_BACK_1 */
}

/**
  * @}
  */

/**
  * @}
  */

