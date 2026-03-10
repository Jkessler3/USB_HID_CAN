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
volatile usb_host_debug_status_t g_usb_host_debug_status = {
  .last_app_state = APPLICATION_IDLE,
  .last_core_host_state = 0U,
  .last_enum_step = 0U,
  .last_error_code = USB_HOST_DBG_FAIL_NONE,
  .last_vid = 0U,
  .last_pid = 0U,
  .last_update_ms = 0U
};

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
static const char *USBH_UserEventName(uint8_t id);
static const char *USBH_SpeedName(uint8_t speed);
static void USBH_DebugUpdate(USBH_HandleTypeDef *phost, ApplicationTypeDef app_state, usb_host_dbg_fail_t fail);
static void USBH_LogSnapshot(USBH_HandleTypeDef *phost, const char *tag);
static void USBH_LogEnumDetails(USBH_HandleTypeDef *phost, const char *stage);

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */
static const char *USBH_UserEventName(uint8_t id)
{
  switch (id) {
  case HOST_USER_SELECT_CONFIGURATION: return "HOST_USER_SET_CONFIGURATION";
  case HOST_USER_CLASS_ACTIVE: return "HOST_USER_CLASS_ACTIVE";
  case HOST_USER_CLASS_SELECTED: return "HOST_USER_CLASS_SELECTED";
  case HOST_USER_CONNECTION: return "HOST_USER_CONNECTION";
  case HOST_USER_DISCONNECTION: return "HOST_USER_DISCONNECTION";
  case HOST_USER_UNRECOVERED_ERROR: return "HOST_USER_UNRECOVERED_ERROR";
  default: return "unknown";
  }
}

static const char *USBH_SpeedName(uint8_t speed)
{
  switch (speed) {
  case USBH_SPEED_HIGH: return "HIGH";
  case USBH_SPEED_FULL: return "FULL";
  case USBH_SPEED_LOW: return "LOW";
  default: return "UNKNOWN";
  }
}

static void USBH_DebugUpdate(USBH_HandleTypeDef *phost, ApplicationTypeDef app_state, usb_host_dbg_fail_t fail)
{
  g_usb_host_debug_status.last_app_state = (uint8_t)app_state;
  g_usb_host_debug_status.last_update_ms = HAL_GetTick();

  if (phost != NULL) {
    g_usb_host_debug_status.last_core_host_state = (uint8_t)phost->gState;
    g_usb_host_debug_status.last_enum_step = (uint8_t)phost->EnumState;
    g_usb_host_debug_status.last_vid = phost->device.DevDesc.idVendor;
    g_usb_host_debug_status.last_pid = phost->device.DevDesc.idProduct;
  }

  g_usb_host_debug_status.last_error_code = (uint8_t)fail;
}

static void USBH_LogSnapshot(USBH_HandleTypeDef *phost, const char *tag)
{
  if (phost == NULL) {
    printf("USB DBG %s t=%lu null host\r\n", tag, HAL_GetTick());
    return;
  }

  printf("USB DBG %s t=%lu app=%u g=%u enum=%u err=%u vid=%04X pid=%04X req=%u conn=%u disc=%u\r\n",
         tag,
         HAL_GetTick(),
         (unsigned)g_usb_host_debug_status.last_app_state,
         (unsigned)phost->gState,
         (unsigned)phost->EnumState,
         (unsigned)g_usb_host_debug_status.last_error_code,
         (unsigned)phost->device.DevDesc.idVendor,
         (unsigned)phost->device.DevDesc.idProduct,
         (unsigned)phost->RequestState,
         (unsigned)phost->device.is_connected,
         (unsigned)phost->device.is_disconnected);
}

static void USBH_LogEnumDetails(USBH_HandleTypeDef *phost, const char *stage)
{
  const USBH_DevDescTypeDef *dd;
  const USBH_InterfaceDescTypeDef *itf = NULL;
  const HID_HandleTypeDef *hid = NULL;
  uint8_t iface_idx;
  uint8_t iface_class = 0U;
  uint8_t iface_sub = 0U;
  uint8_t iface_proto = 0U;
  uint8_t ep_count = 0U;
  uint16_t hid_report_len = 0U;
  uint8_t hid_poll_ms = 0U;

  if (phost == NULL) {
    return;
  }

  dd = &phost->device.DevDesc;
  iface_idx = phost->device.current_interface;
  if (iface_idx < phost->device.CfgDesc.bNumInterfaces) {
    itf = &phost->device.CfgDesc.Itf_Desc[iface_idx];
    iface_class = itf->bInterfaceClass;
    iface_sub = itf->bInterfaceSubClass;
    iface_proto = itf->bInterfaceProtocol;
    ep_count = itf->bNumEndpoints;
  }

  if ((phost->pActiveClass != NULL) && (phost->pActiveClass->pData != NULL)) {
    hid = (HID_HandleTypeDef *)phost->pActiveClass->pData;
    hid_report_len = hid->HID_Desc.wItemLength;
    hid_poll_ms = (uint8_t)hid->poll;
  }

  printf("USB ENUM %s spd=%u(%s) vid=%04X pid=%04X dev=%02X/%02X/%02X if=%02X/%02X/%02X ep=%u hid_rep=%u poll=%u\r\n",
         stage,
         (unsigned)phost->device.speed,
         USBH_SpeedName(phost->device.speed),
         (unsigned)dd->idVendor,
         (unsigned)dd->idProduct,
         (unsigned)dd->bDeviceClass,
         (unsigned)dd->bDeviceSubClass,
         (unsigned)dd->bDeviceProtocol,
         (unsigned)iface_class,
         (unsigned)iface_sub,
         (unsigned)iface_proto,
         (unsigned)ep_count,
         (unsigned)hid_report_len,
         (unsigned)hid_poll_ms);
}

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
  printf("USB EVT %s id=%u t=%lu\r\n", USBH_UserEventName(id), (unsigned)id, HAL_GetTick());

  switch(id)
  {
  case HOST_USER_DISCONNECTION:
    Appli_state = APPLICATION_DISCONNECT;
    USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_DISCONNECT);
    printf("USB: disconnect\r\n");
    USBH_LogSnapshot(phost, "disconnect");
    break;

  case HOST_USER_CONNECTION:
    Appli_state = APPLICATION_START;
    USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_NONE);
    printf("USB: connect\r\n");
    USBH_LogSnapshot(phost, "connect");
    break;

  case HOST_USER_SELECT_CONFIGURATION:
    USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_NONE);
    printf("USB: set configuration\r\n");
    USBH_LogSnapshot(phost, "set_config");
    USBH_LogEnumDetails(phost, "set_config");
    break;

  case HOST_USER_CLASS_SELECTED:
    USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_NONE);
    printf("USB: class selected\r\n");
    USBH_LogSnapshot(phost, "class_selected");
    USBH_LogEnumDetails(phost, "class_selected");
    break;

  case HOST_USER_CLASS_ACTIVE:
    {
      Appli_state = APPLICATION_READY;
      USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_NONE);
      printf("USB: class active\r\n");
      USBH_LogSnapshot(phost, "class_active");
      USBH_LogEnumDetails(phost, "class_active");
      break;
    }

  case HOST_USER_UNRECOVERED_ERROR:
    Appli_state = APPLICATION_DISCONNECT;
    USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_UNRECOVERED);
    printf("USB: unrecovered error\r\n");
    USBH_LogSnapshot(phost, "unrec_error");
    break;

  default:
    USBH_DebugUpdate(phost, Appli_state, USB_HOST_DBG_FAIL_OTHER);
    USBH_LogSnapshot(phost, "other_event");
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

