#ifndef JOYSTICK_TRANSLATOR_APP_H
#define JOYSTICK_TRANSLATOR_APP_H

#include <stdint.h>

#include "../Core/Inc/main.h"
#include "../Middlewares/ST/STM32_USB_Host_Library/Core/Inc/usbh_core.h"
#include "../USB_HOST/App/usb_host.h"

#ifdef __cplusplus
extern "C" {
#endif

void joystick_translator_app_bind_uart(UART_HandleTypeDef *huart);

void joystick_translator_app_init(CAN_HandleTypeDef *hcan,
                                  UART_HandleTypeDef *huart,
                                  USBH_HandleTypeDef *usb_host,
                                  ApplicationTypeDef initial_state,
                                  uint32_t now_ms);

void joystick_translator_app_run(uint32_t now_ms,
                                 ApplicationTypeDef appli_state);

void joystick_translator_app_on_hid_report(USBH_HandleTypeDef *phost);

void joystick_translator_app_on_can_rx_pending(CAN_HandleTypeDef *hcan);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_TRANSLATOR_APP_H */
