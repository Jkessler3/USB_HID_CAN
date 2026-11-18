/* Exercise the public configuration descriptor path through the ST control FSM. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usbh_core.h"

static const uint8_t config[] = {
  9,2,43,0,2,1,0,0x80,50,
  9,4,0,0,0,0xff,0,0,0,
  9,4,1,0,1,3,0,0,0,
  9,0x21,0x11,1,0,1,0x22,32,0,
  7,5,0x81,3,64,0,4
};
static uint8_t response[sizeof(config)];
static uint16_t input_length;

USBH_StatusTypeDef USBH_CtlSendSetup(USBH_HandleTypeDef *h, uint8_t *p, uint8_t pipe)
{ (void)h; (void)p; (void)pipe; return USBH_OK; }
USBH_StatusTypeDef USBH_CtlReceiveData(USBH_HandleTypeDef *h, uint8_t *p, uint16_t n, uint8_t pipe)
{ (void)h; (void)pipe; assert(n>=input_length); memcpy(p,response,input_length); return USBH_OK; }
USBH_StatusTypeDef USBH_CtlSendData(USBH_HandleTypeDef *h, uint8_t *p, uint16_t n, uint8_t pipe, uint8_t ping)
{ (void)h; (void)p; (void)n; (void)pipe; (void)ping; return USBH_OK; }
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *h, uint8_t pipe)
{ (void)h; (void)pipe; return USBH_URB_DONE; }
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *h,uint8_t pipe)
{ assert(pipe==h->Control.pipe_in); return input_length; }
USBH_StatusTypeDef USBH_FreePipe(USBH_HandleTypeDef *h, uint8_t pipe)
{ (void)h; (void)pipe; return USBH_OK; }

static USBH_StatusTypeDef fetch(USBH_HandleTypeDef *host, uint16_t requested, uint16_t actual)
{
  USBH_StatusTypeDef status=USBH_BUSY;
  host->device.speed=USBH_SPEED_FULL;
  host->RequestState=CMD_SEND;
  input_length=actual;
  for (unsigned i=0;i<24 && status==USBH_BUSY;++i) status=USBH_Get_CfgDesc(host,requested);
  return status;
}
int main(void)
{
  USBH_HandleTypeDef host={0};
  memcpy(response,config,sizeof(config));
  assert(fetch(&host,9,9)==USBH_OK);
  assert(fetch(&host,sizeof(config),sizeof(config))==USBH_OK);
  assert(host.device.CfgDesc.Itf_Desc[1].Ep_Desc[0].bEndpointAddress==0x81);
  /* A new short reply must never borrow an endpoint byte from the previous full reply. */
  assert(fetch(&host,sizeof(config),sizeof(config)-1)==USBH_NOT_SUPPORTED);
  response[2]=sizeof(config)+1;
  assert(fetch(&host,sizeof(config),sizeof(config))==USBH_NOT_SUPPORTED);
  puts("PASS descriptor header, full composite configuration, and short transfer rejection");
}
