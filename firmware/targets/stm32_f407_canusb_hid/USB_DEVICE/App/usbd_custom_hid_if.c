#include "usbd_custom_hid_if.h"

#include "hid_protocol.h"
#include "usb_device.h"
#include "usb_hid_transport.h"

static int8_t CUSTOM_HID_Init_FS(void);
static int8_t CUSTOM_HID_DeInit_FS(void);
static int8_t CUSTOM_HID_OutEvent_FS(uint8_t event_idx, uint8_t state);

static uint8_t CUSTOM_HID_ReportDesc_FS[CUSTOM_HID_REPORT_DESC_SIZE] = {
    0x06, 0x00, 0xFF,            /* USAGE_PAGE (Vendor Defined) */
    0x09, 0x01,                  /* USAGE (Vendor Usage 1) */
    0xA1, 0x01,                  /* COLLECTION (Application) */

    0x85, HID_PROTOCOL_REPORT_ID_HOST_CMD,
    0x09, 0x01,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_PROTOCOL_PAYLOAD_BYTES, /* 63 payload bytes + 1-byte report ID */
    0x91, 0x02,                  /* OUTPUT (Data,Var,Abs) */

    0x85, HID_PROTOCOL_REPORT_ID_DEVICE_EVT,
    0x09, 0x01,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_PROTOCOL_PAYLOAD_BYTES, /* 63 payload bytes + 1-byte report ID */
    0x81, 0x02,                  /* INPUT (Data,Var,Abs) */

    0xC0                         /* END_COLLECTION */
};

USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops_FS = {
    CUSTOM_HID_ReportDesc_FS,
    CUSTOM_HID_Init_FS,
    CUSTOM_HID_DeInit_FS,
    CUSTOM_HID_OutEvent_FS
};

static int8_t CUSTOM_HID_Init_FS(void)
{
    return (int8_t)USBD_OK;
}

static int8_t CUSTOM_HID_DeInit_FS(void)
{
    return (int8_t)USBD_OK;
}

static int8_t CUSTOM_HID_OutEvent_FS(uint8_t event_idx, uint8_t state)
{
    USBD_CUSTOM_HID_HandleTypeDef *hhid;

    (void)event_idx;
    (void)state;

    hhid = (USBD_CUSTOM_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hhid == NULL) {
        return (int8_t)USBD_FAIL;
    }

    (void)hid_transport_on_usb_out_report_from_isr(hhid->Report_buf, HID_PROTOCOL_REPORT_SIZE);
    return (int8_t)USBD_OK;
}

uint8_t CUSTOM_HID_SendReport_FS(uint8_t *report, uint16_t len)
{
    if (report == NULL) {
        return USBD_FAIL;
    }
    return USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, len);
}
