/* Native tests at the HID class boundary, with USB transactions simulated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usbh_hid.h"

static uint16_t descriptor_in, packet_in, received_size;
static unsigned descriptor_calls, packet_calls, set_protocol_calls, active_calls;
static USBH_StatusTypeDef submit_status;
static uint8_t selected_ep, interface_number, packet[64];

static void user_event(USBH_HandleTypeDef *host, uint8_t event)
{
  (void)host;
  if (event == HOST_USER_CLASS_ACTIVE) ++active_calls;
}
void USBH_HID_ReportDescriptorCallback(USBH_HandleTypeDef *host, const uint8_t *data, uint16_t size)
{
  assert(host && data == host->device.Data && size == 6);
  ++descriptor_calls;
}
void USBH_HID_InputReportCallback(USBH_HandleTypeDef *host, const uint8_t *data, uint16_t size)
{
  (void)host;
  assert(size <= sizeof(packet));
  memcpy(packet, data, size);
  received_size = size;
  ++packet_calls;
}

USBH_StatusTypeDef USBH_SelectInterface(USBH_HandleTypeDef *host, uint8_t ix)
{ host->device.current_interface = ix; return USBH_OK; }
uint8_t USBH_AllocPipe(USBH_HandleTypeDef *host, uint8_t ep)
{ (void)host; selected_ep=ep; return 3; }
USBH_StatusTypeDef USBH_LL_OpenPipe(USBH_HandleTypeDef *host, uint8_t pipe, uint8_t ep, uint8_t addr, uint8_t speed, uint8_t type, uint16_t mps)
{ (void)host;(void)pipe;(void)ep;(void)addr;(void)speed;(void)type;(void)mps;return USBH_OK; }
USBH_StatusTypeDef USBH_ClosePipe(USBH_HandleTypeDef *host, uint8_t pipe)
{(void)host;(void)pipe;return USBH_OK;}
USBH_StatusTypeDef USBH_FreePipe(USBH_HandleTypeDef *host, uint8_t pipe)
{(void)host;(void)pipe;return USBH_OK;}
USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef *host, uint8_t pipe, uint8_t toggle)
{(void)host;(void)pipe;(void)toggle;return USBH_OK;}
USBH_StatusTypeDef USBH_CtlReq(USBH_HandleTypeDef *host, uint8_t *buf,uint16_t len)
{
  (void)buf;(void)len;
  interface_number=(uint8_t)host->Control.setup.b.wIndex.w;
  assert(interface_number==5);
  if (host->Control.setup.b.bRequest==USB_REQ_GET_DESCRIPTOR) {
    assert(host->Control.setup.b.wValue.w==USB_DESC_HID_REPORT && len==6);
  }
  if (host->Control.setup.b.bRequest==USB_HID_SET_IDLE) {
    assert(host->Control.setup.b.wValue.w == 0x0500);
  }
  if (host->Control.setup.b.bRequest==USB_HID_SET_PROTOCOL) ++set_protocol_calls;
  return host->Control.setup.b.bRequest==USB_HID_SET_IDLE ? USBH_NOT_SUPPORTED : USBH_OK;
}
USBH_StatusTypeDef USBH_ClrFeature(USBH_HandleTypeDef *host, uint8_t ep)
{(void)host;(void)ep;return USBH_OK;}
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *host, uint8_t pipe)
{(void)host;(void)pipe;return USBH_URB_DONE;}
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *host, uint8_t pipe)
{ return pipe==host->Control.pipe_in ? descriptor_in : packet_in; }
USBH_StatusTypeDef USBH_LL_SubmitURB(USBH_HandleTypeDef *host, uint8_t pipe, uint8_t dir, uint8_t ep, uint8_t token,
                                     uint8_t *buff, uint16_t size, uint8_t ping)
{
  (void)host;(void)ping;
  assert(pipe==3 && dir==1 && ep==USBH_EP_INTERRUPT && token==USBH_PID_DATA && buff && size==64);
  return submit_status;
}

static const uint8_t raw[] = {
  9,2,59,0,2,1,0,0x80,50,
  9,4,0,0,0,3,1,1,0, 9,0x21,0x11,1,0,1,0x22,30,0,
  9,4,5,0,2,3,0,0,0, 9,0x21,0x11,1,0,1,0x22,6,0,
  7,5,2,3,8,0,31, 7,5,0x83,3,64,0,4
};

static void reset(USBH_HandleTypeDef *host)
{
  memset(host,0,sizeof(*host));
  descriptor_in=6; packet_in=3; descriptor_calls=packet_calls=set_protocol_calls=active_calls=0;
  host->RequestState=CMD_SEND; host->pActiveClass=&HID_Class; host->pUser=user_event;
  host->device.speed=USBH_SPEED_FULL; host->device.CfgDesc.bNumInterfaces=2;
  host->device.CfgDesc.wTotalLength=sizeof(raw);
  memcpy(host->device.CfgDesc_Raw,raw,sizeof(raw));
  assert(raw[2] == sizeof(raw));
  USBH_InterfaceDescTypeDef *it=host->device.CfgDesc.Itf_Desc;
  it[0].bInterfaceClass=3; it[0].bInterfaceSubClass=1; it[0].bInterfaceProtocol=1;
  it[1].bInterfaceClass=3; it[1].bInterfaceNumber=5; it[1].bNumEndpoints=2;
  it[1].Ep_Desc[0]=(USBH_EpDescTypeDef){7,5,2,3,8,31};
  it[1].Ep_Desc[1]=(USBH_EpDescTypeDef){7,5,0x83,3,64,4};
}
static USBH_StatusTypeDef requests(USBH_HandleTypeDef *h)
{
  USBH_StatusTypeDef s=USBH_BUSY;
  for (unsigned i=0;i<12 && s==USBH_BUSY;++i) s=HID_Class.Requests(h);
  return s;
}
int main(void)
{
  USBH_HandleTypeDef h;
  reset(&h);
  assert(HID_Class.Init(&h)==USBH_OK);
  assert(h.device.current_interface==1 && selected_ep==0x83);
  HID_HandleTypeDef *hid=h.pActiveClass->pData;
  assert(hid->length==64 && hid->poll==4);
  assert(requests(&h)==USBH_OK && active_calls==1 && descriptor_calls==1);
  assert(interface_number==5 && set_protocol_calls==0);
  HID_Class.BgndProcess(&h);
  hid->state=USBH_HID_POLL;hid->timer=0;h.Timer=100;
  memset(hid->pData,0x55,64);hid->pData[0]=9;hid->DataReady=0;
  HID_Class.SOFProcess(&h);
  HID_Class.BgndProcess(&h);
  assert(packet_calls==1 && received_size==3 && packet[0]==9 && packet[3]==0);
  hid->state=USBH_HID_GET_DATA;
  submit_status=USBH_FAIL;
  HID_Class.BgndProcess(&h);
  assert(packet_calls==2 && received_size==0);
  submit_status=USBH_OK;
  h.Timer+=hid->poll;
  HID_Class.BgndProcess(&h);
  HID_Class.BgndProcess(&h);
  packet_in=65;
  HID_Class.BgndProcess(&h);
  assert(packet_calls==3 && received_size==0);
  HID_Class.DeInit(&h);
  reset(&h);
  descriptor_in=4;
  assert(HID_Class.Init(&h)==USBH_OK && requests(&h)==USBH_FAIL && descriptor_calls==0);
  HID_Class.DeInit(&h);
  reset(&h);
  h.device.CfgDesc.bNumInterfaces=1;
  assert(HID_Class.Init(&h)!=USBH_OK);
  reset(&h);
  h.device.CfgDesc.Itf_Desc[1].Ep_Desc[1].bmAttributes=2;
  assert(HID_Class.Init(&h)!=USBH_OK);
  puts("PASS HID interface selection, scoped descriptor request, short report, short descriptor and boot/endpoint rejection");
}
