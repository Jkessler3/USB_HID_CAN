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
#define HID_LOG_PERIOD_MS        0U
#define HID_LOG_MAX_BYTES       HID_RAW_MAX_BYTES
#define HID_DECODE_LOG_PERIOD_MS 0U
#define UART_HEARTBEAT_PERIOD_MS 1000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
typedef enum {
  HID_PROFILE_UNKNOWN = 0,
  HID_PROFILE_KEYBOARD_BOOT = 1,
  HID_PROFILE_MOUSE_BOOT = 2,
  HID_PROFILE_JOYSTICK_GENERIC = 3
} hid_profile_t;

typedef struct {
  uint8_t raw[HID_RAW_MAX_BYTES];
  uint8_t raw_len;
  uint32_t sequence;
  uint32_t last_report_ms;
  uint8_t connected;
  uint8_t hid_ready;
  hid_profile_t profile;
  uint16_t vid;
  uint16_t pid;
  uint8_t interface_protocol;
  uint8_t report_offset;
  uint8_t report_id;
  uint8_t report_id_locked;
  uint8_t report_id_hit_count;
  int16_t axes[4];
  uint8_t axis_count;
  uint32_t buttons;
  uint8_t button_count;
  uint8_t button_offset;
  uint8_t button_bytes;
  int8_t hat;
  uint8_t has_hat;
  uint8_t key_modifiers;
  uint8_t keys[6];
  int8_t mouse_dx;
  int8_t mouse_dy;
  int8_t mouse_wheel;
} joystick_state_t;

extern ApplicationTypeDef Appli_state;
extern USBH_HandleTypeDef hUsbHostFS;

static joystick_state_t g_joystick_state;
static ApplicationTypeDef g_last_appli_state = APPLICATION_IDLE;
static uint32_t g_last_can_tx_ms = 0U;
static uint8_t g_can_tx_sequence = 0U;
static uint32_t g_last_hid_log_ms = 0U;
static uint32_t g_last_hid_decode_log_ms = 0U;
static uint32_t g_last_uart_heartbeat_ms = 0U;
static uint32_t g_uart_direct_tx_fail_count = 0U;
static uint32_t g_prev_buttons = 0U;
static uint8_t g_prev_button_count = 0U;
static hid_profile_t g_prev_profile = HID_PROFILE_UNKNOWN;
static uint8_t g_usb_prev_host_state = 0xFFU;
static uint8_t g_usb_prev_enum_state = 0xFFU;
static uint8_t g_usb_prev_req_state = 0xFFU;
static uint8_t g_usb_prev_conn = 0xFFU;
static uint8_t g_usb_prev_disc = 0xFFU;
static uint8_t g_usb_prev_port = 0xFFU;
static uint8_t g_usb_prev_speed = 0xFFU;
static uint8_t g_usb_prev_addr = 0xFFU;
static uint8_t g_usb_prev_iface = 0xFFU;
static uint16_t g_usb_prev_vid = 0xFFFFU;
static uint16_t g_usb_prev_pid = 0xFFFFU;

typedef struct {
  volatile uint32_t id;
  volatile uint8_t ide;
  volatile uint8_t dlc;
  volatile uint8_t data[8];
  volatile uint32_t count;
  volatile uint8_t pending;
} can_rx_log_t;

static can_rx_log_t g_can_rx_log = {0};

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
static void app_decode_hid_report(USBH_HandleTypeDef *phost,
                                  const uint8_t *report,
                                  uint16_t report_len);
static void app_parse_keyboard_boot(joystick_state_t *state,
                                    const uint8_t *report,
                                    uint16_t report_len);
static void app_parse_mouse_boot(joystick_state_t *state,
                                 const uint8_t *report,
                                 uint16_t report_len);
static void app_parse_joystick_generic(joystick_state_t *state,
                                       const uint8_t *report,
                                       uint16_t report_len);
static uint8_t app_detect_report_offset(joystick_state_t *state,
                                        const uint8_t *report,
                                        uint16_t report_len);
static int16_t app_scale_u8_axis(uint8_t raw);
static void app_log_button_transitions(void);
static void app_log_usb_core_transitions(void);
static const char *app_usb_host_state_name(uint8_t s);
static const char *app_usb_enum_state_name(uint8_t s);
static const char *app_usb_req_state_name(uint8_t s);
static void app_log_hid_decoded(void);
static void app_flush_can_rx_log(void);
static void app_uart_heartbeat(uint32_t now_ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void app_init(void)
{
  const char *msg = "UART OK\r\n";

  memset(&g_joystick_state, 0, sizeof(g_joystick_state));
  g_last_appli_state = Appli_state;
  g_last_can_tx_ms = HAL_GetTick();
  g_last_uart_heartbeat_ms = HAL_GetTick();
  if (HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 100U) != HAL_OK) {
    g_uart_direct_tx_fail_count += 1U;
  }
  printf("APP: init complete\r\n");
  printf("USB UART verbose mode enabled\r\n");
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
    g_joystick_state.axis_count = 0U;
    g_joystick_state.buttons = 0U;
    g_joystick_state.button_count = 0U;
    g_joystick_state.has_hat = 0U;
    g_joystick_state.profile = HID_PROFILE_UNKNOWN;
    g_joystick_state.report_offset = 0U;
    g_joystick_state.report_id = 0U;
    g_joystick_state.report_id_locked = 0U;
    g_joystick_state.report_id_hit_count = 0U;
    g_joystick_state.button_offset = 0U;
    g_joystick_state.button_bytes = 0U;
    g_prev_profile = HID_PROFILE_UNKNOWN;
    g_prev_buttons = 0U;
    g_prev_button_count = 0U;
  }
}

static void app_send_can_test_frame(uint32_t now_ms)
{
  CAN_TxHeaderTypeDef tx = {0};
  uint8_t data[8];
  uint32_t mailbox;
  int16_t axis0;
  int16_t axis1;

  if ((now_ms - g_last_can_tx_ms) < CAN_TEST_TX_PERIOD_MS) {
    return;
  }
  g_last_can_tx_ms = now_ms;

  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U) {
    return;
  }

  axis0 = (g_joystick_state.axis_count > 0U) ? g_joystick_state.axes[0] : 0;
  axis1 = (g_joystick_state.axis_count > 1U) ? g_joystick_state.axes[1] : 0;

  data[0] = (uint8_t)(axis0 & 0xFF);
  data[1] = (uint8_t)((axis0 >> 8) & 0xFF);
  data[2] = (uint8_t)(axis1 & 0xFF);
  data[3] = (uint8_t)((axis1 >> 8) & 0xFF);
  data[4] = (uint8_t)(g_joystick_state.buttons & 0xFFU);
  data[5] = (uint8_t)((g_joystick_state.buttons >> 8U) & 0xFFU);
  data[6] = (uint8_t)g_joystick_state.sequence;
  data[7] = g_can_tx_sequence++;

  tx.StdId = CAN_TEST_STD_ID;
  tx.ExtId = 0U;
  tx.IDE = CAN_ID_STD;
  tx.RTR = CAN_RTR_DATA;
  tx.DLC = 8U;

  if (HAL_CAN_AddTxMessage(&hcan1, &tx, data, &mailbox) == HAL_OK) {
    printf("CAN TX test id=0x%03lX seq=%u ax0=%d ax1=%d btn=%04X\r\n",
           tx.StdId,
           data[7],
           axis0,
           axis1,
           (unsigned)(g_joystick_state.buttons & 0xFFFFU));
  }
}

static void app_loop(void)
{
  uint32_t now_ms = HAL_GetTick();
  app_update_usb_state();
  app_log_usb_core_transitions();
  app_send_can_test_frame(now_ms);
  app_flush_can_rx_log();
  app_uart_heartbeat(now_ms);
  app_log_hid_decoded();
}

static void app_dump_raw_hid(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  uint16_t limit = len;
  uint32_t now_ms = HAL_GetTick();

  if ((HID_LOG_PERIOD_MS > 0U) && ((now_ms - g_last_hid_log_ms) < HID_LOG_PERIOD_MS)) {
    return;
  }
  g_last_hid_log_ms = now_ms;

  if (limit > HID_LOG_MAX_BYTES) {
    limit = HID_LOG_MAX_BYTES;
  }

  printf("HID RAW len=%u seq=%lu:", len, g_joystick_state.sequence);
  for (i = 0U; i < limit; i++) {
    printf(" %02X", data[i]);
  }
  if (limit < len) {
    printf(" ...");
  }
  printf("\r\n");
}

static int16_t app_scale_u8_axis(uint8_t raw)
{
  int32_t centered = (int32_t)raw - 128;
  return (int16_t)(centered * 8);
}

static uint8_t app_detect_report_offset(joystick_state_t *state,
                                        const uint8_t *report,
                                        uint16_t report_len)
{
  uint8_t candidate;

  if ((state == NULL) || (report == NULL) || (report_len < 2U)) {
    return 0U;
  }

  if (state->report_id_locked != 0U) {
    return state->report_offset;
  }

  candidate = report[0];
  if ((candidate >= 1U) && (candidate <= 8U)) {
    if (state->report_id == candidate) {
      if (state->report_id_hit_count < 255U) {
        state->report_id_hit_count += 1U;
      }
    } else {
      state->report_id = candidate;
      state->report_id_hit_count = 1U;
    }
  } else {
    state->report_id = 0U;
    state->report_id_hit_count = 0U;
  }

  if ((state->report_id_hit_count >= 4U) && (report_len >= 6U)) {
    state->report_id_locked = 1U;
    state->report_offset = 1U;
  }

  return state->report_offset;
}

static void app_parse_keyboard_boot(joystick_state_t *state,
                                    const uint8_t *report,
                                    uint16_t report_len)
{
  uint8_t i;
  uint32_t buttons = 0U;

  if ((state == NULL) || (report == NULL) || (report_len < 8U)) {
    return;
  }

  state->profile = HID_PROFILE_KEYBOARD_BOOT;
  state->mouse_dx = 0;
  state->mouse_dy = 0;
  state->mouse_wheel = 0;
  state->axis_count = 0U;
  state->has_hat = 0U;
  state->hat = -1;
  state->button_offset = 0U;
  state->button_bytes = 4U;
  state->key_modifiers = report[0];
  memcpy(state->keys, &report[2], sizeof(state->keys));

  buttons = state->key_modifiers;
  for (i = 0U; i < 6U; i++) {
    uint8_t key = state->keys[i];
    if ((key > 0U) && (key < 24U)) {
      buttons |= (1UL << (8U + key));
    }
  }

  state->buttons = buttons;
  state->button_count = 32U;
}

static void app_parse_mouse_boot(joystick_state_t *state,
                                 const uint8_t *report,
                                 uint16_t report_len)
{
  if ((state == NULL) || (report == NULL) || (report_len < 3U)) {
    return;
  }

  state->profile = HID_PROFILE_MOUSE_BOOT;
  state->key_modifiers = 0U;
  memset(state->keys, 0, sizeof(state->keys));
  state->buttons = (uint32_t)(report[0] & 0x1FU);
  state->button_count = 5U;
  state->button_offset = 0U;
  state->button_bytes = 1U;
  state->mouse_dx = (int8_t)report[1];
  state->mouse_dy = (int8_t)report[2];
  state->mouse_wheel = (report_len > 3U) ? (int8_t)report[3] : 0;
  state->axes[0] = (int16_t)((int16_t)state->mouse_dx * 256);
  state->axes[1] = (int16_t)((int16_t)state->mouse_dy * 256);
  state->axes[2] = (int16_t)((int16_t)state->mouse_wheel * 256);
  state->axis_count = 3U;
  state->has_hat = 0U;
  state->hat = -1;
}

static void app_parse_joystick_generic(joystick_state_t *state,
                                       const uint8_t *report,
                                       uint16_t report_len)
{
  const uint8_t *payload = report;
  uint16_t payload_len = report_len;
  uint8_t axis_count;
  uint8_t i;
  uint8_t offset;
  uint8_t button_offset;
  uint8_t button_bytes;
  uint32_t buttons = 0U;

  if ((state == NULL) || (report == NULL) || (report_len == 0U)) {
    return;
  }

  state->profile = HID_PROFILE_JOYSTICK_GENERIC;
  state->mouse_dx = 0;
  state->mouse_dy = 0;
  state->mouse_wheel = 0;
  state->has_hat = 0U;
  state->hat = -1;
  state->button_count = 0U;
  state->button_offset = 0U;
  state->button_bytes = 0U;
  state->buttons = 0U;

  offset = app_detect_report_offset(state, report, report_len);
  if ((offset > 0U) && (report_len > 1U)) {
    payload = &report[offset];
    payload_len = (uint16_t)(report_len - offset);
  }

  axis_count = (payload_len > 4U) ? 4U : (uint8_t)payload_len;

  if ((state->vid == 0x044FU) && (payload_len >= 7U)) {
    /* Thrustmaster-friendly heuristic: first 4 bytes are primary axes. */
    uint8_t tail_bytes;
    uint8_t tail_start;

    axis_count = 4U;
    for (i = 0U; i < axis_count; i++) {
      state->axes[i] = app_scale_u8_axis(payload[i]);
    }

    if ((payload[4] & 0x0FU) <= 8U) {
      state->has_hat = 1U;
      state->hat = (int8_t)(payload[4] & 0x0FU);
    }

    tail_bytes = (uint8_t)(payload_len - 4U);
    if (tail_bytes > 4U) {
      tail_bytes = 4U;
    }
    tail_start = (uint8_t)(payload_len - tail_bytes);
    for (i = 0U; i < tail_bytes; i++) {
      buttons |= ((uint32_t)payload[tail_start + i]) << (8U * i);
    }
    state->buttons = buttons;
    state->button_count = (uint8_t)(tail_bytes * 8U);
    state->button_offset = (uint8_t)(offset + tail_start);
    state->button_bytes = tail_bytes;
    state->axis_count = axis_count;
    return;
  }

  for (i = 0U; i < axis_count; i++) {
    state->axes[i] = app_scale_u8_axis(payload[i]);
  }
  state->axis_count = axis_count;

  button_offset = axis_count;
  if (payload_len > button_offset) {
    uint8_t hat_candidate = payload[button_offset] & 0x0FU;
    if (hat_candidate <= 8U) {
      state->has_hat = 1U;
      state->hat = (int8_t)hat_candidate;
      button_offset += 1U;
    }
  }

  if (payload_len > button_offset) {
    button_bytes = (uint8_t)(payload_len - button_offset);
    if (button_bytes > 4U) {
      button_bytes = 4U;
    }
    for (i = 0U; i < button_bytes; i++) {
      buttons |= ((uint32_t)payload[button_offset + i]) << (8U * i);
    }
    state->buttons = buttons;
    state->button_count = (uint8_t)(button_bytes * 8U);
    state->button_offset = (uint8_t)(offset + button_offset);
    state->button_bytes = button_bytes;
  } else if (payload_len > 0U) {
    /* Fallback: sample tail bytes as button area for unknown report layouts. */
    uint8_t tail_bytes = (payload_len > 4U) ? 4U : (uint8_t)payload_len;
    uint8_t tail_start = (uint8_t)(payload_len - tail_bytes);
    for (i = 0U; i < tail_bytes; i++) {
      buttons |= ((uint32_t)payload[tail_start + i]) << (8U * i);
    }
    state->buttons = buttons;
    state->button_count = (uint8_t)(tail_bytes * 8U);
    state->button_offset = (uint8_t)(offset + tail_start);
    state->button_bytes = tail_bytes;
  }
}

static void app_decode_hid_report(USBH_HandleTypeDef *phost,
                                  const uint8_t *report,
                                  uint16_t report_len)
{
  uint8_t iface;
  uint8_t protocol = 0xFFU;

  if ((phost == NULL) || (report == NULL) || (report_len == 0U)) {
    return;
  }

  g_joystick_state.vid = phost->device.DevDesc.idVendor;
  g_joystick_state.pid = phost->device.DevDesc.idProduct;

  iface = phost->device.current_interface;
  if (iface < phost->device.CfgDesc.bNumInterfaces) {
    protocol = phost->device.CfgDesc.Itf_Desc[iface].bInterfaceProtocol;
  }
  g_joystick_state.interface_protocol = protocol;

  if (protocol == HID_KEYBRD_BOOT_CODE) {
    app_parse_keyboard_boot(&g_joystick_state, report, report_len);
  } else if (protocol == HID_MOUSE_BOOT_CODE) {
    app_parse_mouse_boot(&g_joystick_state, report, report_len);
  } else {
    app_parse_joystick_generic(&g_joystick_state, report, report_len);
  }
}

static const char *app_usb_host_state_name(uint8_t s)
{
  switch (s) {
  case HOST_IDLE: return "HOST_IDLE";
  case HOST_DEV_WAIT_FOR_ATTACHMENT: return "HOST_WAIT_ATTACH";
  case HOST_DEV_ATTACHED: return "HOST_ATTACHED";
  case HOST_DEV_DISCONNECTED: return "HOST_DISCONNECTED";
  case HOST_DETECT_DEVICE_SPEED: return "HOST_DETECT_SPEED";
  case HOST_ENUMERATION: return "HOST_ENUM";
  case HOST_CLASS_REQUEST: return "HOST_CLASS_REQ";
  case HOST_INPUT: return "HOST_INPUT";
  case HOST_SET_CONFIGURATION: return "HOST_SET_CFG";
  case HOST_CHECK_CLASS: return "HOST_CHECK_CLASS";
  case HOST_CLASS: return "HOST_CLASS";
  case HOST_ABORT_STATE: return "HOST_ABORT";
  default: return "HOST_UNKNOWN";
  }
}

static const char *app_usb_enum_state_name(uint8_t s)
{
  switch (s) {
  case ENUM_IDLE: return "ENUM_IDLE";
  case ENUM_GET_FULL_DEV_DESC: return "ENUM_GET_DEV_DESC";
  case ENUM_SET_ADDR: return "ENUM_SET_ADDR";
  case ENUM_GET_CFG_DESC: return "ENUM_GET_CFG_DESC";
  case ENUM_GET_FULL_CFG_DESC: return "ENUM_GET_FULL_CFG";
  case ENUM_GET_MFC_STRING_DESC: return "ENUM_GET_MFC";
  case ENUM_GET_PRODUCT_STRING_DESC: return "ENUM_GET_PRODUCT";
  case ENUM_GET_SERIALNUM_STRING_DESC: return "ENUM_GET_SERIAL";
  default: return "ENUM_UNKNOWN";
  }
}

static const char *app_usb_req_state_name(uint8_t s)
{
  switch (s) {
  case CMD_IDLE: return "CMD_IDLE";
  case CMD_SEND: return "CMD_SEND";
  case CMD_WAIT: return "CMD_WAIT";
  default: return "CMD_UNKNOWN";
  }
}

static void app_log_usb_core_transitions(void)
{
  uint8_t g;
  uint8_t e;
  uint8_t r;
  uint8_t c;
  uint8_t d;
  uint8_t p;
  uint8_t s;
  uint8_t a;
  uint8_t i;
  uint16_t vid;
  uint16_t pid;

  g = (uint8_t)hUsbHostFS.gState;
  e = (uint8_t)hUsbHostFS.EnumState;
  r = (uint8_t)hUsbHostFS.RequestState;
  c = (uint8_t)hUsbHostFS.device.is_connected;
  d = (uint8_t)hUsbHostFS.device.is_disconnected;
  p = (uint8_t)hUsbHostFS.device.PortEnabled;
  s = (uint8_t)hUsbHostFS.device.speed;
  a = (uint8_t)hUsbHostFS.device.address;
  i = (uint8_t)hUsbHostFS.device.current_interface;
  vid = hUsbHostFS.device.DevDesc.idVendor;
  pid = hUsbHostFS.device.DevDesc.idProduct;

  if ((g == g_usb_prev_host_state) &&
      (e == g_usb_prev_enum_state) &&
      (r == g_usb_prev_req_state) &&
      (c == g_usb_prev_conn) &&
      (d == g_usb_prev_disc) &&
      (p == g_usb_prev_port) &&
      (s == g_usb_prev_speed) &&
      (a == g_usb_prev_addr) &&
      (i == g_usb_prev_iface) &&
      (vid == g_usb_prev_vid) &&
      (pid == g_usb_prev_pid)) {
    return;
  }

  if ((g_usb_prev_conn != 0xFFU) && (g_usb_prev_conn != c)) {
    if (c != 0U) {
      printf("USB: connect(edge) t=%lu\r\n", HAL_GetTick());
    } else {
      printf("USB: disconnect(edge) t=%lu\r\n", HAL_GetTick());
    }
  }

  g_usb_prev_host_state = g;
  g_usb_prev_enum_state = e;
  g_usb_prev_req_state = r;
  g_usb_prev_conn = c;
  g_usb_prev_disc = d;
  g_usb_prev_port = p;
  g_usb_prev_speed = s;
  g_usb_prev_addr = a;
  g_usb_prev_iface = i;
  g_usb_prev_vid = vid;
  g_usb_prev_pid = pid;

  printf("USB CORE t=%lu g=%s(%u) e=%s(%u) r=%s(%u) c=%u d=%u p=%u spd=%u addr=%u if=%u vid=%04X pid=%04X\r\n",
         HAL_GetTick(),
         app_usb_host_state_name(g), g,
         app_usb_enum_state_name(e), e,
         app_usb_req_state_name(r), r,
         c, d, p, s, a, i,
         vid, pid);
}

static void app_log_button_transitions(void)
{
  uint32_t changed;
  uint8_t max_bits;
  uint8_t bit;

  if (!g_joystick_state.hid_ready) {
    return;
  }

  if (g_prev_profile != g_joystick_state.profile) {
    g_prev_profile = g_joystick_state.profile;
    g_prev_buttons = g_joystick_state.buttons;
    g_prev_button_count = g_joystick_state.button_count;
    return;
  }

  changed = g_prev_buttons ^ g_joystick_state.buttons;
  if (changed == 0U) {
    return;
  }

  max_bits = g_joystick_state.button_count;
  if (max_bits == 0U) {
    max_bits = 32U;
  }
  if (max_bits > 32U) {
    max_bits = 32U;
  }

  for (bit = 0U; bit < max_bits; bit++) {
    uint32_t mask = (1UL << bit);
    if ((changed & mask) == 0U) {
      continue;
    }
    printf("HID BTN bit%u %s (btn=%08lX off=%u bytes=%u)\r\n",
           (unsigned)bit,
           ((g_joystick_state.buttons & mask) != 0U) ? "pressed" : "released",
           g_joystick_state.buttons,
           g_joystick_state.button_offset,
           g_joystick_state.button_bytes);
  }

  g_prev_buttons = g_joystick_state.buttons;
  g_prev_button_count = g_joystick_state.button_count;
}

static void app_log_hid_decoded(void)
{
  uint32_t now_ms = HAL_GetTick();

  if ((HID_DECODE_LOG_PERIOD_MS > 0U) &&
      ((now_ms - g_last_hid_decode_log_ms) < HID_DECODE_LOG_PERIOD_MS)) {
    return;
  }
  g_last_hid_decode_log_ms = now_ms;

  if (!g_joystick_state.hid_ready) {
    return;
  }

  if (g_joystick_state.profile == HID_PROFILE_KEYBOARD_BOOT) {
    printf("HID KBD mod=%02X k=%02X %02X %02X %02X %02X %02X\r\n",
           g_joystick_state.key_modifiers,
           g_joystick_state.keys[0],
           g_joystick_state.keys[1],
           g_joystick_state.keys[2],
           g_joystick_state.keys[3],
           g_joystick_state.keys[4],
           g_joystick_state.keys[5]);
    app_log_button_transitions();
    return;
  }

  if (g_joystick_state.profile == HID_PROFILE_MOUSE_BOOT) {
    printf("HID MOUSE btn=%02lX dx=%d dy=%d wheel=%d\r\n",
           g_joystick_state.buttons & 0x1FU,
           (int)g_joystick_state.mouse_dx,
           (int)g_joystick_state.mouse_dy,
           (int)g_joystick_state.mouse_wheel);
    app_log_button_transitions();
    return;
  }

  printf("HID JOY vid=%04X pid=%04X proto=%u rid=%u off=%u ax=[%d,%d,%d,%d] btn=%08lX bc=%u hat=%d\r\n",
         g_joystick_state.vid,
         g_joystick_state.pid,
         g_joystick_state.interface_protocol,
         g_joystick_state.report_id,
         g_joystick_state.report_offset,
         (int)g_joystick_state.axes[0],
         (int)g_joystick_state.axes[1],
         (int)g_joystick_state.axes[2],
         (int)g_joystick_state.axes[3],
         g_joystick_state.buttons,
         g_joystick_state.button_count,
         (int)g_joystick_state.hat);
  app_log_button_transitions();
}

static void app_flush_can_rx_log(void)
{
  uint32_t id;
  uint32_t count;
  uint8_t ide;
  uint8_t dlc;
  uint8_t d0;
  uint8_t d1;
  uint8_t pending;

  __disable_irq();
  pending = g_can_rx_log.pending;
  id = g_can_rx_log.id;
  count = g_can_rx_log.count;
  ide = g_can_rx_log.ide;
  dlc = g_can_rx_log.dlc;
  d0 = g_can_rx_log.data[0];
  d1 = g_can_rx_log.data[1];
  g_can_rx_log.pending = 0U;
  __enable_irq();

  if (pending == 0U) {
    return;
  }

  if (ide == CAN_ID_STD) {
    printf("CAN RX cnt=%lu STD id=0x%03lX dlc=%u d0=%02X d1=%02X\r\n",
           count,
           id,
           dlc,
           d0,
           d1);
  } else {
    printf("CAN RX cnt=%lu EXT id=0x%08lX dlc=%u d0=%02X d1=%02X\r\n",
           count,
           id,
           dlc,
           d0,
           d1);
  }
}

static void app_uart_heartbeat(uint32_t now_ms)
{
  char buf[64];
  int n;

  if ((now_ms - g_last_uart_heartbeat_ms) < UART_HEARTBEAT_PERIOD_MS) {
    return;
  }
  g_last_uart_heartbeat_ms = now_ms;

  n = snprintf(buf, sizeof(buf), "UART HB t=%lu fail=%lu\r\n",
               now_ms, g_uart_direct_tx_fail_count);
  if (n <= 0) {
    return;
  }
  if (HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)n, 100U) != HAL_OK) {
    g_uart_direct_tx_fail_count += 1U;
  }
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
  app_decode_hid_report(phost, local, read_len);
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
  /* Bring up UART before USB host init so early USB events are visible. */
  MX_USART2_UART_Init();
  HAL_UART_Transmit(&huart2, (uint8_t *)"UART pre-init\r\n", 15U, 100U);

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

  if ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0U) {
    return len;
  }

  if (HAL_UART_Transmit(&huart2, (uint8_t*)ptr, (uint16_t)len, 20U) != HAL_OK) {
    return 0;
  }

  return len;
}


void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef rx;
  uint8_t d[8];
  uint8_t i;

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, d) != HAL_OK) return;

  g_can_rx_log.ide = (uint8_t)rx.IDE;
  g_can_rx_log.id = (rx.IDE == CAN_ID_STD) ? rx.StdId : rx.ExtId;
  g_can_rx_log.dlc = (uint8_t)rx.DLC;
  for (i = 0U; i < 8U; i++) {
    g_can_rx_log.data[i] = d[i];
  }
  g_can_rx_log.count += 1U;
  g_can_rx_log.pending = 1U;
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
