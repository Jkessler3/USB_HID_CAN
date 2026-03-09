#include "main.h"

#include <stdbool.h>

#include "can.h"
#include "host_interface_compat.h"
#include "usb_device.h"
#include "usb_hid_transport.h"
#include "usbd_custom_hid_if.h"

static void SystemClock_Config(void);

static bool app_can_tx(const can_core_frame_t *frame)
{
    return can1_send_core_frame(frame);
}

static bool app_obd_request(uint8_t mode, uint8_t pid, uint32_t seq)
{
    can_core_frame_t request;

    (void)seq;

    if (mode != CAN_OBD_MODE_CURRENT_DATA) {
        return false;
    }
    if (!can_obd_build_mode01_request(pid, &request)) {
        return false;
    }
    return can1_send_core_frame(&request);
}

static bool app_set_bitrate(uint32_t bitrate_bps)
{
    return can1_apply_bitrate(bitrate_bps);
}

static bool app_set_filter(uint32_t can_id, uint32_t mask, bool extended)
{
    return can1_apply_filter(can_id, mask, extended);
}

int main(void)
{
    hid_transport_callbacks_t callbacks;

    HAL_Init();
    SystemClock_Config();

    MX_CAN1_Init();
    MX_USB_DEVICE_Init();

    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        Error_Handler();
    }

    callbacks.can_tx = app_can_tx;
    callbacks.obd_request = app_obd_request;
    callbacks.set_bitrate = app_set_bitrate;
    callbacks.set_filter = app_set_filter;

    hid_transport_init(&callbacks);
    hid_transport_set_usb_tx_callback(CUSTOM_HID_SendReport_FS);
    host_interface_compat_init();

    for (;;) {
        hid_transport_process_host_reports();
        hid_transport_poll_tx_queue();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init = {0};
    RCC_ClkInitTypeDef clk_init = {0};
    RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc_init.HSEState = RCC_HSE_ON;
    osc_init.PLL.PLLState = RCC_PLL_ON;
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc_init.PLL.PLLM = 8;
    osc_init.PLL.PLLN = 336;
    osc_init.PLL.PLLP = RCC_PLLP_DIV2;
    osc_init.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&osc_init) != HAL_OK) {
        Error_Handler();
    }

    clk_init.ClockType = RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2;
    clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk_init.APB1CLKDivider = RCC_HCLK_DIV4;
    clk_init.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    periph_clk_init.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLQ;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
