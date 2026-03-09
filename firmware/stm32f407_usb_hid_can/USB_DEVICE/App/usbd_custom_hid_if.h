#ifndef __USBD_CUSTOM_HID_IF_H__
#define __USBD_CUSTOM_HID_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_customhid.h"

#include <stdint.h>

#define CUSTOM_HID_REPORT_DESC_SIZE    38U

extern USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops_FS;

uint8_t CUSTOM_HID_SendReport_FS(uint8_t *report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CUSTOM_HID_IF_H__ */
