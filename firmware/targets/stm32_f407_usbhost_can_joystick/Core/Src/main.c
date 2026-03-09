/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "main.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include "usbh_hid.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HID_RAW_MAX_BYTES        64U
#define CAN_TEST_TX_PERIOD_MS   100U
#define CAN_TEST_STD_ID       0x321U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
typedef struct {
  uint8_t raw[HID_RAW_MAX_BYTES];
  uint8_t raw_len;
  uint32_t sequence;
  uint32_t last_report_ms;
  uint8_t connected;
  uint8_t hid_ready;
} joystick_state_t;

extern ApplicationTypeDef Appli_state;

static joystick_state_t g_joystick_state;
static ApplicationTypeDef g_last_appli_state = APPLICATION_IDLE;
static uint32_t g_last_can_tx_ms = 0U;
static uint8_t g_can_tx_sequence = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN1_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */
static void app_init(void);
static void app_loop(void);
static void app_update_usb_state(void);
static void app_send_can_test_frame(uint32_t now_ms);
static void app_dump_raw_hid(const uint8_t *data, uint16_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void app_init(void)
{
  memset(&g_joystick_state, 0, sizeof(g_joystick_state));
  g_last_appli_state = Appli_state;
  g_last_can_tx_ms = HAL_GetTick();
  printf("APP: init complete\r\n");
}

static void app_update_usb_state(void)
{
  if (Appli_state != g_last_appli_state) {
    printf("USB APP state %u -> %u\r\n", (unsigned)g_last_appli_state, (unsigned)Appli_state);
    g_last_appli_state = Appli_state;
  }

  if (Appli_state == APPLICATION_READY) {
    g_joystick_state.connected = 1U;
  } else if ((Appli_state == APPLICATION_DISCONNECT) || (Appli_state == APPLICATION_IDLE)) {
    g_joystick_state.connected = 0U;
    g_joystick_state.hid_ready = 0U;
    g_joystick_state.raw_len = 0U;
  }
}

static void app_send_can_test_frame(uint32_t now_ms)
{
  CAN_TxHeaderTypeDef tx = {0};
  uint8_t data[8];
  uint32_t mailbox;

  if ((now_ms - g_last_can_tx_ms) < CAN_TEST_TX_PERIOD_MS) {
    return;
  }
  g_last_can_tx_ms = now_ms;

  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U) {
    return;
  }

  data[0] = 0xA5U;
  data[1] = 0x5AU;
  data[2] = g_joystick_state.connected;
  data[3] = g_joystick_state.raw_len;
  data[4] = (g_joystick_state.raw_len > 0U) ? g_joystick_state.raw[0] : 0U;
  data[5] = (g_joystick_state.raw_len > 1U) ? g_joystick_state.raw[1] : 0U;
  data[6] = (uint8_t)g_joystick_state.sequence;
  data[7] = g_can_tx_sequence++;

  tx.StdId = CAN_TEST_STD_ID;
  tx.ExtId = 0U;
  tx.IDE = CAN_ID_STD;
  tx.RTR = CAN_RTR_DATA;
  tx.DLC = 8U;

  if (HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox) == HAL_OK) {
    printf("CAN TX test id=0x%03lX seq=%u len=%u b0=%02X b1=%02X\r\n",
           tx.StdId,
           data[7],
           data[3],
           data[4],
           data[5]);
  }
}

static void app_loop(void)
{
  uint32_t now_ms = HAL_GetTick();
  app_update_usb_state();
  app_send_can_test_frame(now_ms);
}

static void app_dump_raw_hid(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  printf("HID RAW len=%u seq=%lu:", len, g_joystick_state.sequence);
  for (i = 0U; i < len; i++) {
    printf(" %02X", data[i]);
  }
  printf("\r\n");
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *hid;
  uint8_t local[HID_RAW_MAX_BYTES];
  uint16_t to_read;
  uint16_t read_len;

  if ((phost == NULL) || (phost->pActiveClass == NULL) || (phost->pActiveClass->pData == NULL)) {
    return;
  }

  hid = (HID_HandleTypeDef *)phost->pActiveClass->pData;
  to_read = hid->length;
  if (to_read > HID_RAW_MAX_BYTES) {
    to_read = HID_RAW_MAX_BYTES;
  }
  if (to_read == 0U) {
    return;
  }

  read_len = USBH_HID_FifoRead(&hid->fifo, local, to_read);
  if (read_len == 0U) {
    return;
  }

  memset(g_joystick_state.raw, 0, sizeof(g_joystick_state.raw));
  memcpy(g_joystick_state.raw, local, read_len);
  g_joystick_state.raw_len = (uint8_t)read_len;
  g_joystick_state.sequence += 1U;
  g_joystick_state.last_report_ms = HAL_GetTick();
  g_joystick_state.connected = 1U;
  g_joystick_state.hid_ready = 1U;

  app_dump_raw_hid(local, read_len);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_HOST_Init();
  MX_USART2_UART_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  // --- Configure RX filter: accept everything into FIFO0 ---
  CAN_FilterTypeDef f = {0};
  f.FilterBank = 0;
  f.FilterMode = CAN_FILTERMODE_IDMASK;
  f.FilterScale = CAN_FILTERSCALE_32BIT;
  f.FilterIdHigh = 0x0000;
  f.FilterIdLow  = 0x0000;
  f.FilterMaskIdHigh = 0x0000;
  f.FilterMaskIdLow  = 0x0000;
  f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  f.FilterActivation = ENABLE;
  f.SlaveStartFilterBank = 14;   // safe on F4 (even if only CAN1)
  HAL_CAN_ConfigFilter(&hcan1, &f);

  // --- Start CAN ---
   if (HAL_CAN_Start(&hcan1) != HAL_OK)
   {
     Error_Handler();
   }
   // --- Enable RX interrupt notifications ---
   if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
   {
     Error_Handler();
   }
   app_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */
    app_loop();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = ENABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len) {
  (void)file;
  HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
  return len;
}


void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef rx;
  uint8_t d[8];
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, d) != HAL_OK) return;

  if (rx.IDE == CAN_ID_STD) {
    printf("CAN RX STD ID=0x%03lX DLC=%lu Data:", rx.StdId, rx.DLC);
  } else {
    printf("CAN RX EXT ID=0x%08lX DLC=%lu Data:", rx.ExtId, rx.DLC);
  }
  for (uint8_t i = 0; i < rx.DLC; i++) printf(" %02X", d[i]);
  printf("\r\n");
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
