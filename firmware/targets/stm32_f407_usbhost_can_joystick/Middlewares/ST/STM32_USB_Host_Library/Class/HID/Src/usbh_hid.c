/**
  ******************************************************************************
  * @file    usbh_hid.c
  * @author  MCD Application Team
  * @brief   This file is the HID Layer Handlers for USB Host HID class.
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  * @verbatim
  *
  *          ===================================================================
  *                                HID Class  Description
  *          ===================================================================
  *           This module manages the HID class V1.11 following the "Device Class Definition
  *           for Human Interface Devices (HID) Version 1.11 Jun 27, 2001".
  *           This driver implements the following aspects of the specification:
  *             - Nonboot joystick/gamepad report descriptors
  *             - Interrupt IN delivery using the actual transfer length
  *
  *  @endverbatim
  *
  ******************************************************************************
  */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
- "stm32xxxxx_{eval}{discovery}{adafruit}_lcd.c"
- "stm32xxxxx_{eval}{discovery}_sdram.c"
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbh_hid.h"
#include "usbh_hid_parser.h"


/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_CLASS
  * @{
  */

/** @addtogroup USBH_HID_CLASS
  * @{
  */

/** @defgroup USBH_HID_CORE
  * @brief    This file includes HID Layer Handlers for USB Host HID class.
  * @{
  */

/** @defgroup USBH_HID_CORE_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_HID_CORE_Private_Defines
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_HID_CORE_Private_Macros
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_HID_CORE_Private_Variables
  * @{
  */
/* This target is a full/low-speed, single-port USB Host. */
#define HID_FS_MAX_PACKET 64U
#define HID_IDLE_PERIOD_4MS 5U
static uint8_t hid_generic_rx_report_buf[HID_FS_MAX_PACKET];

/**
  * @}
  */


/** @defgroup USBH_HID_CORE_Private_FunctionPrototypes
  * @{
  */

static USBH_StatusTypeDef USBH_HID_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_SOFProcess(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_ParseHIDDesc(USBH_HandleTypeDef *phost, HID_DescTypeDef *desc);

USBH_ClassTypeDef  HID_Class =
{
  "HID",
  USB_HID_CLASS,
  USBH_HID_InterfaceInit,
  USBH_HID_InterfaceDeInit,
  USBH_HID_ClassRequest,
  USBH_HID_Process,
  USBH_HID_SOFProcess,
  NULL,
};
/**
  * @}
  */


/** @defgroup USBH_HID_CORE_Private_Functions
  * @{
  */


/**
  * @brief  USBH_HID_InterfaceInit
  *         The function init the HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_InterfaceInit(USBH_HandleTypeDef *phost)
{
  USBH_CfgDescTypeDef *cfg = &phost->device.CfgDesc;
  const USBH_EpDescTypeDef *input = NULL;
  uint8_t selected = 0xFFU;

  /* Select one unambiguous, default-alternate, nonboot HID input interface.
     A boot keyboard/mouse is never a translator control source. */
  if (cfg->bNumInterfaces > USBH_MAX_NUM_INTERFACES)
  {
    return USBH_FAIL;
  }
  for (uint8_t i = 0U; i < cfg->bNumInterfaces; ++i)
  {
    const USBH_InterfaceDescTypeDef *itf = &cfg->Itf_Desc[i];
    if (itf->bInterfaceClass != USB_HID_CLASS || itf->bInterfaceSubClass != 0U ||
        itf->bInterfaceProtocol != 0U || itf->bAlternateSetting != 0U)
    {
      continue;
    }
    for (uint8_t e = 0U; e < itf->bNumEndpoints && e < USBH_MAX_NUM_ENDPOINTS; ++e)
    {
      const USBH_EpDescTypeDef *ep = &itf->Ep_Desc[e];
      if ((ep->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_IN &&
          (ep->bmAttributes & 3U) == USB_EP_TYPE_INTR)
      {
        if (input != NULL || ep->wMaxPacketSize == 0U ||
            ep->wMaxPacketSize > HID_FS_MAX_PACKET || ep->bInterval == 0U ||
            (ep->bEndpointAddress & 0x0FU) == 0U)
        {
          return USBH_FAIL;
        }
        input = ep;
        selected = i;
      }
    }
  }
  if (input == NULL || USBH_SelectInterface(phost, selected) != USBH_OK)
  {
    return USBH_FAIL;
  }
  HID_HandleTypeDef *hid = (HID_HandleTypeDef *)USBH_malloc(sizeof(*hid));
  if (hid == NULL)
  {
    return USBH_FAIL;
  }
  USBH_memset(hid, 0, sizeof(*hid));
  hid->InEp = input->bEndpointAddress;
  hid->ep_addr = hid->InEp;
  hid->length = input->wMaxPacketSize;
  hid->poll = input->bInterval;
  hid->InPipe = USBH_AllocPipe(phost, hid->InEp);
  if (hid->InPipe == 0xFFU)
  {
    USBH_free(hid);
    return USBH_FAIL;
  }
  if (USBH_LL_OpenPipe(phost, hid->InPipe, hid->InEp, phost->device.address,
                    phost->device.speed, USB_EP_TYPE_INTR, hid->length) != USBH_OK)
  {
    USBH_FreePipe(phost, hid->InPipe);
    USBH_free(hid);
    return USBH_FAIL;
  }
  USBH_LL_SetToggle(phost, hid->InPipe, 0U);
  hid->pData = hid_generic_rx_report_buf;
  hid->state = USBH_HID_GET_DATA;
  hid->ctl_state = USBH_HID_REQ_INIT;
  phost->pActiveClass->pData = hid;
  return USBH_OK;
}

/**
  * @brief  USBH_HID_InterfaceDeInit
  *         The function DeInit the Pipes used for the HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if (HID_Handle == NULL)
  {
    return USBH_OK;
  }
  if (HID_Handle->InPipe < USBH_MAX_PIPES_NBR)
  {
    (void)USBH_ClosePipe(phost, HID_Handle->InPipe);
    (void)USBH_FreePipe(phost, HID_Handle->InPipe);
    HID_Handle->InPipe = 0U;     /* Reset the pipe as Free */
  }

  USBH_free(HID_Handle);
  phost->pActiveClass->pData = NULL;

  return USBH_OK;
}

/**
  * @brief  USBH_HID_ClassRequest
  *         The function is responsible for handling Standard requests
  *         for HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_ClassRequest(USBH_HandleTypeDef *phost)
{

  USBH_StatusTypeDef status         = USBH_BUSY;
  USBH_StatusTypeDef classReqStatus = USBH_BUSY;
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  /* Switch HID state machine */
  switch (HID_Handle->ctl_state)
  {
    case USBH_HID_REQ_INIT:
    case USBH_HID_REQ_GET_HID_DESC:

      if (USBH_HID_ParseHIDDesc(phost, &HID_Handle->HID_Desc) != USBH_OK)
      {
        return USBH_FAIL;
      }

      HID_Handle->ctl_state = USBH_HID_REQ_GET_REPORT_DESC;

      break;
    case USBH_HID_REQ_GET_REPORT_DESC:

      /* Get Report Desc */
      classReqStatus = USBH_HID_GetHIDReportDescriptor(phost, HID_Handle->HID_Desc.wItemLength);
      if (classReqStatus == USBH_OK)
      {
        /* The status stage used the OUT pipe. The IN pipe still holds
           the actual byte count from this completed descriptor data stage. */
        if (USBH_LL_GetLastXferSize(phost, phost->Control.pipe_in) != HID_Handle->HID_Desc.wItemLength)
        {
          return USBH_FAIL;
        }
        USBH_HID_ReportDescriptorCallback(phost, phost->device.Data, HID_Handle->HID_Desc.wItemLength);
        HID_Handle->ctl_state = USBH_HID_REQ_SET_IDLE;
      }
      else if (classReqStatus != USBH_BUSY)
      {
        return USBH_FAIL;
      }
      break;

    case USBH_HID_REQ_SET_IDLE:
      /* Request periodic unchanged reports where the device supports them.
         SET_PROTOCOL is a boot-subclass request and does not apply here. */
      classReqStatus = USBH_HID_SetIdle(phost, HID_IDLE_PERIOD_4MS, 0U);
      if (classReqStatus == USBH_OK || classReqStatus == USBH_NOT_SUPPORTED)
      {
        HID_Handle->ctl_state = USBH_HID_REQ_IDLE;
        phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
        status = USBH_OK;
      }
      else if (classReqStatus != USBH_BUSY)
      {
        status = USBH_FAIL;
      }
      break;

    case USBH_HID_REQ_IDLE:
    default:
      break;
  }

  return status;
}

/**
  * @brief  USBH_HID_Process
  *         The function is for managing state machine for HID data transfers
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_Process(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_OK;
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
  uint32_t XferSize;

  switch (HID_Handle->state)
  {
    case USBH_HID_IDLE:
      if ((phost->Timer - HID_Handle->timer) >= HID_Handle->poll)
      {
        HID_Handle->state = USBH_HID_GET_DATA;
      }
      break;

    case USBH_HID_GET_DATA:
      /* The ST convenience wrapper discards the controller's submission result. */
      status = USBH_LL_SubmitURB(phost, HID_Handle->InPipe, 1U, USBH_EP_INTERRUPT,
                                 USBH_PID_DATA, HID_Handle->pData, HID_Handle->length, 0U);
      HID_Handle->timer = phost->Timer;
      HID_Handle->DataReady = 0U;
      if (status == USBH_OK)
      {
        HID_Handle->state = USBH_HID_POLL;
      }
      else
      {
        HID_Handle->state = USBH_HID_IDLE;
        USBH_HID_InputReportCallback(phost, HID_Handle->pData, 0U);
      }
      break;

    case USBH_HID_POLL:
    {
      USBH_URBStateTypeDef urb = USBH_LL_GetURBState(phost, HID_Handle->InPipe);
      if (urb == USBH_URB_DONE && HID_Handle->DataReady == 0U)
      {
        XferSize = USBH_LL_GetLastXferSize(phost, HID_Handle->InPipe);
        HID_Handle->DataReady = 1U;
        /* A zero-length or inconsistent completion is an invalid sample.
           Never pass padding or residual bytes to the application. */
        uint16_t actual = (XferSize <= HID_Handle->length) ? (uint16_t)XferSize : 0U;
        USBH_HID_InputReportCallback(phost, HID_Handle->pData, actual);
      }
      if (urb == USBH_URB_STALL)
      {
        if (USBH_ClrFeature(phost, HID_Handle->InEp) == USBH_OK)
        {
          USBH_LL_SetToggle(phost, HID_Handle->InPipe, 0U);
          HID_Handle->state = USBH_HID_GET_DATA;
        }
      }
      else if (urb != USBH_URB_IDLE && (phost->Timer - HID_Handle->timer) >= HID_Handle->poll)
      {
        HID_Handle->state = USBH_HID_GET_DATA;
      }
      break;
    }

    default:
      break;
  }

  return status;
}

/**
  * @brief  USBH_HID_SOFProcess
  *         The function is for managing the SOF Process
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_SOFProcess(USBH_HandleTypeDef *phost)
{
  /* The foreground owns URB completion and resubmission. A SOF IRQ must
     not reschedule the pipe before the foreground consumes its data. */
  UNUSED(phost);
  return USBH_OK;
}

/**
  * @brief  Issue an interface-recipient HID descriptor request.
  * @param  phost: Host handle
  * @param  Length : HID Report Descriptor Length
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_GetInterfaceDescriptor(USBH_HandleTypeDef *phost,
                                                          uint16_t type, uint16_t length)
{
  if (length == 0U || length > sizeof(phost->device.Data))
  {
    return USBH_NOT_SUPPORTED;
  }
  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_STANDARD;
    phost->Control.setup.b.bRequest = USB_REQ_GET_DESCRIPTOR;
    phost->Control.setup.b.wValue.w = type;
    phost->Control.setup.b.wIndex.w = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceNumber;
    phost->Control.setup.b.wLength.w = length;
  }
  return USBH_CtlReq(phost, phost->device.Data, length);
}

USBH_StatusTypeDef USBH_HID_GetHIDReportDescriptor(USBH_HandleTypeDef *phost, uint16_t length)
{
  return USBH_HID_GetInterfaceDescriptor(phost, USB_DESC_HID_REPORT, length);
}

/**
  * @brief  USBH_Get_HID_Descriptor
  *         Issue HID Descriptor command to the device. Once the response
  *         received, parse the report descriptor and update the status.
  * @param  phost: Host handle
  * @param  Length : HID Descriptor Length
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_GetHIDDescriptor(USBH_HandleTypeDef *phost,
                                             uint16_t length)
{
  return USBH_HID_GetInterfaceDescriptor(phost, USB_DESC_HID, length);
}

/**
  * @brief  USBH_Set_Idle
  *         Set Idle State.
  * @param  phost: Host handle
  * @param  duration: Duration for HID Idle request
  * @param  reportId : Targeted report ID for Set Idle request
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_SetIdle(USBH_HandleTypeDef *phost,
                                    uint8_t duration,
                                    uint8_t reportId)
{

  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;


  phost->Control.setup.b.bRequest = USB_HID_SET_IDLE;
  phost->Control.setup.b.wValue.w = (uint16_t)(((uint32_t)duration << 8U) | (uint32_t)reportId);

  phost->Control.setup.b.wIndex.w = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceNumber;
  phost->Control.setup.b.wLength.w = 0U;

  return USBH_CtlReq(phost, NULL, 0U);
}


/**
  * @brief  USBH_HID_Set_Report
  *         Issues Set Report
  * @param  phost: Host handle
  * @param  reportType  : Report type to be sent
  * @param  reportId    : Targeted report ID for Set Report request
  * @param  reportBuff  : Report Buffer
  * @param  reportLen   : Length of data report to be send
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_SetReport(USBH_HandleTypeDef *phost,
                                      uint8_t reportType,
                                      uint8_t reportId,
                                      uint8_t *reportBuff,
                                      uint8_t reportLen)
{

  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;


  phost->Control.setup.b.bRequest = USB_HID_SET_REPORT;
  phost->Control.setup.b.wValue.w = (uint16_t)(((uint32_t)reportType << 8U) | (uint32_t)reportId);

  phost->Control.setup.b.wIndex.w = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceNumber;
  phost->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq(phost, reportBuff, (uint16_t)reportLen);
}


/**
  * @brief  USBH_HID_GetReport
  *         retrieve Set Report
  * @param  phost: Host handle
  * @param  reportType  : Report type to be sent
  * @param  reportId    : Targeted report ID for Set Report request
  * @param  reportBuff  : Report Buffer
  * @param  reportLen   : Length of data report to be send
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_GetReport(USBH_HandleTypeDef *phost,
                                      uint8_t reportType,
                                      uint8_t reportId,
                                      uint8_t *reportBuff,
                                      uint8_t reportLen)
{

  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | \
                                         USB_REQ_TYPE_CLASS;


  phost->Control.setup.b.bRequest = USB_HID_GET_REPORT;
  phost->Control.setup.b.wValue.w = (uint16_t)(((uint32_t)reportType << 8U) | (uint32_t)reportId);

  phost->Control.setup.b.wIndex.w = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceNumber;
  phost->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq(phost, reportBuff, (uint16_t)reportLen);
}

/**
  * @brief  USBH_Set_Protocol
  *         Set protocol State.
  * @param  phost: Host handle
  * @param  protocol : Set Protocol for HID : boot/report protocol
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_SetProtocol(USBH_HandleTypeDef *phost,
                                        uint8_t protocol)
{
  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE
                                         | USB_REQ_TYPE_CLASS;

  phost->Control.setup.b.bRequest = USB_HID_SET_PROTOCOL;
  if (protocol != 0U)
  {
    phost->Control.setup.b.wValue.w = 0U;
  }
  else
  {
    phost->Control.setup.b.wValue.w = 1U;
  }

  phost->Control.setup.b.wIndex.w = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceNumber;
  phost->Control.setup.b.wLength.w = 0U;

  return USBH_CtlReq(phost, NULL, 0U);

}

/**
  * @brief  Locate the selected interface's report descriptor metadata in the configuration.
  */
static USBH_StatusTypeDef USBH_HID_ParseHIDDesc(USBH_HandleTypeDef *phost, HID_DescTypeDef *desc)
{
  const uint8_t *raw = phost->device.CfgDesc_Raw;
  const uint16_t total = LE16(raw + 2U);
  const uint8_t number = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceNumber;
  uint8_t selected = 0U;

  if (total < USB_CONFIGURATION_DESC_SIZE || total > sizeof(phost->device.CfgDesc_Raw))
  {
    return USBH_FAIL;
  }
  for (uint16_t offset = 0U; offset < total; )
  {
    const uint8_t *item = &raw[offset];
    const uint16_t remaining = total - offset;
    if (remaining < 2U || item[0] < 2U || item[0] > remaining)
    {
      return USBH_FAIL;
    }
    if (item[1] == USB_DESC_TYPE_INTERFACE)
    {
      if (item[0] < USB_INTERFACE_DESC_SIZE)
      {
        return USBH_FAIL;
      }
      selected = (item[2] == number && item[3] == 0U && item[5] == USB_HID_CLASS);
    }
    if (selected && item[1] == USB_DESC_TYPE_HID)
    {
      if (item[0] < USB_HID_DESC_SIZE || (6U + item[5] * 3U) > item[0])
      {
        return USBH_FAIL;
      }
      for (uint16_t entry = 6U; entry < (6U + item[5] * 3U); entry += 3U)
      {
        if (item[entry] == USB_DESC_TYPE_HID_REPORT)
        {
          desc->bLength = item[0];
          desc->bDescriptorType = item[1];
          desc->bcdHID = LE16(item + 2U);
          desc->bCountryCode = item[4];
          desc->bNumDescriptors = item[5];
          desc->bReportDescriptorType = item[entry];
          desc->wItemLength = LE16(item + entry + 1U);
          return desc->wItemLength > 0U && desc->wItemLength <= sizeof(phost->device.Data)
                 ? USBH_OK : USBH_FAIL;
        }
      }
      return USBH_FAIL;
    }
    offset += item[0];
  }
  return USBH_FAIL;
}

/**
  * @brief  USBH_HID_GetDeviceType
  *         Return Device function.
  * @param  phost: Host handle
  * @retval HID function: HID_MOUSE / HID_KEYBOARD
  */
HID_TypeTypeDef USBH_HID_GetDeviceType(USBH_HandleTypeDef *phost)
{
  HID_TypeTypeDef   type = HID_UNKNOWN;
  uint8_t InterfaceProtocol;

  if (phost->gState == HOST_CLASS)
  {
    InterfaceProtocol = phost->device.CfgDesc.Itf_Desc[phost->device.current_interface].bInterfaceProtocol;
    if (InterfaceProtocol == HID_KEYBRD_BOOT_CODE)
    {
      type = HID_KEYBOARD;
    }
    else
    {
      if (InterfaceProtocol == HID_MOUSE_BOOT_CODE)
      {
        type = HID_MOUSE;
      }
    }
  }
  return type;
}


/**
  * @brief  USBH_HID_GetPollInterval
  *         Return HID device poll time
  * @param  phost: Host handle
  * @retval poll time (ms)
  */
uint8_t USBH_HID_GetPollInterval(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if ((phost->gState == HOST_CLASS_REQUEST) ||
      (phost->gState == HOST_INPUT) ||
      (phost->gState == HOST_SET_CONFIGURATION) ||
      (phost->gState == HOST_CHECK_CLASS) ||
      ((phost->gState == HOST_CLASS)))
  {
    return (uint8_t)(HID_Handle->poll);
  }
  else
  {
    return 0U;
  }
}
/**
  * @brief  USBH_HID_FifoInit
  *         Initialize FIFO.
  * @param  f: Fifo address
  * @param  buf: Fifo buffer
  * @param  size: Fifo Size
  * @retval none
  */
void USBH_HID_FifoInit(FIFO_TypeDef *f, uint8_t *buf, uint16_t size)
{
  f->head = 0U;
  f->tail = 0U;
  f->lock = 0U;
  f->size = size;
  f->buf = buf;
}

/**
  * @brief  USBH_HID_FifoRead
  *         Read from FIFO.
  * @param  f: Fifo address
  * @param  buf: read buffer
  * @param  nbytes: number of item to read
  * @retval number of read items
  */
uint16_t USBH_HID_FifoRead(FIFO_TypeDef *f, void *buf, uint16_t nbytes)
{
  uint16_t i;
  uint8_t *p;

  p = (uint8_t *) buf;

  if (f->lock == 0U)
  {
    f->lock = 1U;

    for (i = 0U; i < nbytes; i++)
    {
      if (f->tail != f->head)
      {
        *p++ = f->buf[f->tail];
        f->tail++;

        if (f->tail == f->size)
        {
          f->tail = 0U;
        }
      }
      else
      {
        f->lock = 0U;
        return i;
      }
    }
  }

  f->lock = 0U;

  return nbytes;
}

/**
  * @brief  USBH_HID_FifoWrite
  *         Write To FIFO.
  * @param  f: Fifo address
  * @param  buf: read buffer
  * @param  nbytes: number of item to write
  * @retval number of written items
  */
uint16_t USBH_HID_FifoWrite(FIFO_TypeDef *f, void *buf, uint16_t  nbytes)
{
  uint16_t i;
  uint8_t *p;

  p = (uint8_t *) buf;

  if (f->lock == 0U)
  {
    f->lock = 1U;

    for (i = 0U; i < nbytes; i++)
    {
      if (((f->head + 1U) == f->tail) ||
          (((f->head + 1U) == f->size) && (f->tail == 0U)))
      {
        f->lock = 0U;
        return i;
      }
      else
      {
        f->buf[f->head] = *p++;
        f->head++;

        if (f->head == f->size)
        {
          f->head = 0U;
        }
      }
    }
  }

  f->lock = 0U;

  return nbytes;
}

/**
  * @brief  The function is a callback about HID Data events
  *  @param  phost: Selected device
  * @retval None
  */
__weak void USBH_HID_ReportDescriptorCallback(USBH_HandleTypeDef *phost, const uint8_t *data, uint16_t length)
{
  UNUSED(phost);
  UNUSED(data);
  UNUSED(length);
}

__weak void USBH_HID_InputReportCallback(USBH_HandleTypeDef *phost, const uint8_t *data, uint16_t length)
{
  UNUSED(phost);
  UNUSED(data);
  UNUSED(length);
}

__weak void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(phost);
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


/**
  * @}
  */


/**
  * @}
  */
