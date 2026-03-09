/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

#include "can.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "app_main.h"
/* USER CODE END Includes */

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_CAN1_Init();
    MX_USB_DEVICE_Init();

    /* USER CODE BEGIN 2 */
    app_init();
    /* USER CODE END 2 */

    while (1) {
        /* USER CODE BEGIN WHILE */
        app_loop();
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        /* USER CODE END 3 */
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
