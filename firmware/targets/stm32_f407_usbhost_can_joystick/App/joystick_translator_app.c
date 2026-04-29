#include "joystick_translator_app.h"

#include <stdio.h>
#include <string.h>

#include "../Middlewares/ST/STM32_USB_Host_Library/Class/HID/Inc/usbh_hid.h"
#include "../../../shared/can/include/can_types.h"
#include "../../../shared/mapping/include/joystick_can_mapping.h"
#include "../../../shared/usb_host_hid/include/usb_host_hid.h"

#define HID_RAW_MAX_BYTES         64U
#define HID_LOG_PERIOD_MS        100U
#define HID_LOG_MAX_BYTES        HID_RAW_MAX_BYTES
#define HID_DECODE_LOG_PERIOD_MS 250U
#define UART_HEARTBEAT_PERIOD_MS 1000U
#define CAN_TX_LOG_PERIOD_MS     250U

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

typedef struct {
    volatile uint32_t id;
    volatile uint8_t ide;
    volatile uint8_t dlc;
    volatile uint8_t data[8];
    volatile uint32_t count;
    volatile uint8_t pending;
} can_rx_log_t;

typedef struct {
    CAN_HandleTypeDef *hcan;
    UART_HandleTypeDef *huart;
    USBH_HandleTypeDef *usb_host;
    joystick_state_t joystick_state;
    can_rx_log_t can_rx_log;
    ApplicationTypeDef last_appli_state;
    uint32_t last_can_tx_ms;
    uint8_t can_tx_sequence;
    uint32_t last_hid_log_ms;
    uint32_t last_hid_decode_log_ms;
    uint32_t last_can_tx_log_ms;
    uint32_t last_uart_heartbeat_ms;
    uint32_t uart_direct_tx_fail_count;
    uint32_t can_init_error_count;
    uint8_t can_ready;
    uint32_t prev_buttons;
    uint8_t prev_button_count;
    hid_profile_t prev_profile;
    uint8_t usb_prev_host_state;
    uint8_t usb_prev_enum_state;
    uint8_t usb_prev_req_state;
    uint8_t usb_prev_conn;
    uint8_t usb_prev_disc;
    uint8_t usb_prev_port;
    uint8_t usb_prev_speed;
    uint8_t usb_prev_addr;
    uint8_t usb_prev_iface;
    uint16_t usb_prev_vid;
    uint16_t usb_prev_pid;
    usb_host_hid_device_info_t hid_device;
    usb_host_hid_report_layout_t hid_layout;
    usb_host_hid_joystick_state_t shared_joystick_state;
    joystick_can_mapping_table_t mapping;
    joystick_can_pipeline_state_t pipeline;
} joystick_translator_runtime_t;

static joystick_translator_runtime_t s_app;
static UART_HandleTypeDef *s_stdout_uart;

static bool app_configure_can(void);
static void app_update_usb_state(ApplicationTypeDef appli_state, uint32_t now_ms);
static void app_send_mapped_can_frames(uint32_t now_ms);
static HAL_StatusTypeDef app_transmit_can_frame(const can_frame_t *frame);
static void app_dump_raw_hid(const uint8_t *data, uint16_t len, uint32_t now_ms);
static bool app_decode_hid_report(USBH_HandleTypeDef *phost,
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
static const char *app_usb_host_state_name(uint8_t state);
static const char *app_usb_enum_state_name(uint8_t state);
static const char *app_usb_req_state_name(uint8_t state);
static void app_log_hid_decoded(uint32_t now_ms);
static void app_flush_can_rx_log(void);
static void app_uart_heartbeat(uint32_t now_ms);
static void app_sync_shared_joystick_state(uint32_t now_ms);

void joystick_translator_app_bind_uart(UART_HandleTypeDef *huart)
{
    s_stdout_uart = huart;
}

void joystick_translator_app_init(CAN_HandleTypeDef *hcan,
                                  UART_HandleTypeDef *huart,
                                  USBH_HandleTypeDef *usb_host,
                                  ApplicationTypeDef initial_state,
                                  uint32_t now_ms)
{
    const char *msg = "UART OK\r\n";

    memset(&s_app, 0, sizeof(s_app));
    s_app.hcan = hcan;
    s_app.huart = huart;
    s_app.usb_host = usb_host;
    s_app.last_appli_state = initial_state;
    s_app.last_can_tx_ms = now_ms;
    s_app.last_uart_heartbeat_ms = now_ms;
    s_app.prev_profile = HID_PROFILE_UNKNOWN;
    s_app.usb_prev_host_state = 0xFFU;
    s_app.usb_prev_enum_state = 0xFFU;
    s_app.usb_prev_req_state = 0xFFU;
    s_app.usb_prev_conn = 0xFFU;
    s_app.usb_prev_disc = 0xFFU;
    s_app.usb_prev_port = 0xFFU;
    s_app.usb_prev_speed = 0xFFU;
    s_app.usb_prev_addr = 0xFFU;
    s_app.usb_prev_iface = 0xFFU;
    s_app.usb_prev_vid = 0xFFFFU;
    s_app.usb_prev_pid = 0xFFFFU;

    usb_host_hid_device_reset(&s_app.hid_device);
    usb_host_hid_default_layout_init(&s_app.hid_layout);
    usb_host_hid_joystick_state_clear(&s_app.shared_joystick_state);
    joystick_can_mapping_table_init_default(&s_app.mapping);
    joystick_can_pipeline_reset(&s_app.pipeline);
    s_app.pipeline.last_control_tx_ms = now_ms;
    s_app.pipeline.last_status_tx_ms = now_ms;
    usb_host_hid_device_set_stale_timeout(&s_app.hid_device,
                                          s_app.mapping.stale_timeout_ms);

    if ((huart != NULL) &&
        (HAL_UART_Transmit(huart,
                           (uint8_t *)msg,
                           (uint16_t)strlen(msg),
                           100U) != HAL_OK)) {
        s_app.uart_direct_tx_fail_count += 1U;
    }

    s_stdout_uart = huart;
    printf("APP: init complete\r\n");
    s_app.can_ready = app_configure_can() ? 1U : 0U;
    if (s_app.can_ready != 0U) {
        printf("CAN: ready\r\n");
    } else {
        printf("CAN: init failed, UART kept alive\r\n");
    }
    printf("USB UART verbose mode enabled\r\n");
}

void joystick_translator_app_run(uint32_t now_ms,
                                 ApplicationTypeDef appli_state)
{
    app_update_usb_state(appli_state, now_ms);
    app_log_usb_core_transitions();

    if (usb_host_hid_joystick_is_stale(&s_app.hid_device, now_ms)) {
        usb_host_hid_joystick_apply_stale_timeout(&s_app.hid_device,
                                                  now_ms,
                                                  &s_app.shared_joystick_state);
        s_app.pipeline.connected = false;
    }

    app_send_mapped_can_frames(now_ms);
    app_flush_can_rx_log();
    app_uart_heartbeat(now_ms);
    app_log_hid_decoded(now_ms);
}

void joystick_translator_app_on_hid_report(USBH_HandleTypeDef *phost)
{
    HID_HandleTypeDef *hid;
    uint8_t local[HID_RAW_MAX_BYTES];
    uint16_t to_read;
    uint16_t read_len;
    uint32_t now_ms;
    bool decoded;

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

    now_ms = HAL_GetTick();
    usb_host_hid_on_report(&s_app.hid_device, now_ms);
    memset(s_app.joystick_state.raw, 0, sizeof(s_app.joystick_state.raw));
    memcpy(s_app.joystick_state.raw, local, read_len);
    s_app.joystick_state.raw_len = (uint8_t)read_len;
    s_app.joystick_state.sequence += 1U;
    s_app.joystick_state.last_report_ms = now_ms;
    s_app.joystick_state.connected = 1U;
    s_app.joystick_state.hid_ready = 1U;

    app_dump_raw_hid(local, read_len, now_ms);
    decoded = app_decode_hid_report(phost, local, read_len);
    if (!decoded) {
        s_app.joystick_state.hid_ready = 0U;
        joystick_can_pipeline_note_hid_parse_error(&s_app.pipeline);
        return;
    }
    app_sync_shared_joystick_state(now_ms);
}

void joystick_translator_app_on_can_rx_pending(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx;
    uint8_t data[8];
    uint8_t i;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, data) != HAL_OK) {
        return;
    }

    s_app.can_rx_log.ide = (uint8_t)rx.IDE;
    s_app.can_rx_log.id = (rx.IDE == CAN_ID_STD) ? rx.StdId : rx.ExtId;
    s_app.can_rx_log.dlc = (uint8_t)rx.DLC;
    for (i = 0U; i < 8U; i++) {
        s_app.can_rx_log.data[i] = data[i];
    }
    s_app.can_rx_log.count += 1U;
    s_app.can_rx_log.pending = 1U;
}

static bool app_configure_can(void)
{
    CAN_FilterTypeDef filter = {0};

    if (s_app.hcan == NULL) {
        return false;
    }

    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(s_app.hcan, &filter) != HAL_OK) {
        s_app.can_init_error_count += 1U;
        return false;
    }
    if (HAL_CAN_Start(s_app.hcan) != HAL_OK) {
        s_app.can_init_error_count += 1U;
        return false;
    }
    if (HAL_CAN_ActivateNotification(s_app.hcan,
                                     CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        s_app.can_init_error_count += 1U;
        return false;
    }

    return true;
}

static void app_update_usb_state(ApplicationTypeDef appli_state, uint32_t now_ms)
{
    USBH_HandleTypeDef *phost = s_app.usb_host;

    if (appli_state != s_app.last_appli_state) {
        printf("USB APP state %u -> %u\r\n",
               (unsigned)s_app.last_appli_state,
               (unsigned)appli_state);

        if (appli_state == APPLICATION_START) {
            usb_host_hid_on_connection(&s_app.hid_device, now_ms);
            joystick_can_pipeline_on_connect(&s_app.pipeline, now_ms);
        } else if (appli_state == APPLICATION_READY) {
            if (phost != NULL) {
                usb_host_hid_on_class_selected(&s_app.hid_device,
                                               phost->device.DevDesc.idVendor,
                                               phost->device.DevDesc.idProduct,
                                               phost->device.speed,
                                               phost->device.CfgDesc.bNumInterfaces,
                                               now_ms);
            }
            usb_host_hid_on_class_active(&s_app.hid_device, now_ms);
        } else if ((appli_state == APPLICATION_DISCONNECT) ||
                   (appli_state == APPLICATION_IDLE)) {
            usb_host_hid_on_disconnection(&s_app.hid_device, now_ms);
            joystick_can_pipeline_on_disconnect(&s_app.pipeline, now_ms);
            usb_host_hid_joystick_mark_disconnected(&s_app.shared_joystick_state,
                                                    now_ms);
        }

        s_app.last_appli_state = appli_state;
    }

    if (appli_state == APPLICATION_READY) {
        s_app.joystick_state.connected = 1U;
    } else if ((appli_state == APPLICATION_DISCONNECT) ||
               (appli_state == APPLICATION_IDLE)) {
        s_app.joystick_state.connected = 0U;
        s_app.joystick_state.hid_ready = 0U;
        s_app.joystick_state.raw_len = 0U;
        s_app.joystick_state.axis_count = 0U;
        s_app.joystick_state.buttons = 0U;
        s_app.joystick_state.button_count = 0U;
        s_app.joystick_state.has_hat = 0U;
        s_app.joystick_state.profile = HID_PROFILE_UNKNOWN;
        s_app.joystick_state.report_offset = 0U;
        s_app.joystick_state.report_id = 0U;
        s_app.joystick_state.report_id_locked = 0U;
        s_app.joystick_state.report_id_hit_count = 0U;
        s_app.joystick_state.button_offset = 0U;
        s_app.joystick_state.button_bytes = 0U;
        s_app.prev_profile = HID_PROFILE_UNKNOWN;
        s_app.prev_buttons = 0U;
        s_app.prev_button_count = 0U;
    }
}

static HAL_StatusTypeDef app_transmit_can_frame(const can_frame_t *frame)
{
    CAN_TxHeaderTypeDef tx = {0};
    uint32_t mailbox;

    if ((s_app.hcan == NULL) || (s_app.can_ready == 0U) || (frame == NULL)) {
        return HAL_ERROR;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(s_app.hcan) == 0U) {
        return HAL_BUSY;
    }

    tx.StdId = frame->extended ? 0U : frame->id;
    tx.ExtId = frame->extended ? frame->id : 0U;
    tx.IDE = frame->extended ? CAN_ID_EXT : CAN_ID_STD;
    tx.RTR = frame->rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    tx.DLC = frame->dlc;

    return HAL_CAN_AddTxMessage(s_app.hcan, &tx, (uint8_t *)frame->data, &mailbox);
}

static void app_send_mapped_can_frames(uint32_t now_ms)
{
    can_frame_t frame;
    joystick_can_tx_kind_t kind;

    if ((s_app.hcan == NULL) || (s_app.can_ready == 0U)) {
        return;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(s_app.hcan) == 0U) {
        joystick_can_pipeline_note_tx_backpressure(&s_app.pipeline);
        return;
    }

    if (!joystick_can_pipeline_build_next_frame(&s_app.mapping,
                                                &s_app.pipeline,
                                                now_ms,
                                                &frame,
                                                &kind)) {
        return;
    }

    if (app_transmit_can_frame(&frame) == HAL_OK) {
        s_app.last_can_tx_ms = now_ms;
        s_app.can_tx_sequence += 1U;

        if ((now_ms - s_app.last_can_tx_log_ms) < CAN_TX_LOG_PERIOD_MS) {
            return;
        }
        s_app.last_can_tx_log_ms = now_ms;

        if (kind == JOYSTICK_CAN_TX_CONTROL) {
            int16_t steer = (int16_t)((uint16_t)frame.data[0] |
                                      ((uint16_t)frame.data[1] << 8U));
            int16_t throttle = (int16_t)((uint16_t)frame.data[2] |
                                         ((uint16_t)frame.data[3] << 8U));

            printf("CAN TX control id=0x%03lX seq=%lu steer=%d throttle=%d flags=%02X\r\n",
                   (unsigned long)frame.id,
                   (unsigned long)s_app.pipeline.frame_sequence,
                   (int)steer,
                   (int)throttle,
                   frame.data[7]);
        } else if (kind == JOYSTICK_CAN_TX_STATUS) {
            printf("CAN TX status id=0x%03lX seq=%lu state=%02X axes=%u\r\n",
                   (unsigned long)frame.id,
                   (unsigned long)s_app.pipeline.frame_sequence,
                   frame.data[0],
                   frame.data[1]);
        }
    } else {
        joystick_can_pipeline_note_tx_backpressure(&s_app.pipeline);
    }
}

static void app_dump_raw_hid(const uint8_t *data, uint16_t len, uint32_t now_ms)
{
    uint16_t i;
    uint16_t limit = len;

    if ((HID_LOG_PERIOD_MS > 0U) &&
        ((now_ms - s_app.last_hid_log_ms) < HID_LOG_PERIOD_MS)) {
        return;
    }
    s_app.last_hid_log_ms = now_ms;

    if (limit > HID_LOG_MAX_BYTES) {
        limit = HID_LOG_MAX_BYTES;
    }

    printf("HID RAW len=%u seq=%lu:",
           len,
           (unsigned long)s_app.joystick_state.sequence);
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

static bool app_decode_hid_report(USBH_HandleTypeDef *phost,
                                  const uint8_t *report,
                                  uint16_t report_len)
{
    uint8_t iface;
    uint8_t protocol = 0xFFU;

    if ((phost == NULL) || (report == NULL) || (report_len == 0U)) {
        return false;
    }

    s_app.joystick_state.vid = phost->device.DevDesc.idVendor;
    s_app.joystick_state.pid = phost->device.DevDesc.idProduct;

    iface = phost->device.current_interface;
    if (iface < phost->device.CfgDesc.bNumInterfaces) {
        protocol = phost->device.CfgDesc.Itf_Desc[iface].bInterfaceProtocol;
    }
    s_app.joystick_state.interface_protocol = protocol;

    if (protocol == HID_KEYBRD_BOOT_CODE) {
        if (report_len < 8U) {
            return false;
        }
        app_parse_keyboard_boot(&s_app.joystick_state, report, report_len);
        return true;
    } else if (protocol == HID_MOUSE_BOOT_CODE) {
        if (report_len < 3U) {
            return false;
        }
        app_parse_mouse_boot(&s_app.joystick_state, report, report_len);
        return true;
    } else {
        app_parse_joystick_generic(&s_app.joystick_state, report, report_len);
    }

    return ((s_app.joystick_state.axis_count > 0U) ||
            (s_app.joystick_state.button_count > 0U) ||
            (s_app.joystick_state.has_hat != 0U));
}

static const char *app_usb_host_state_name(uint8_t state)
{
    switch (state) {
    case HOST_IDLE:
        return "HOST_IDLE";
    case HOST_DEV_WAIT_FOR_ATTACHMENT:
        return "HOST_WAIT_ATTACH";
    case HOST_DEV_ATTACHED:
        return "HOST_ATTACHED";
    case HOST_DEV_DISCONNECTED:
        return "HOST_DISCONNECTED";
    case HOST_DETECT_DEVICE_SPEED:
        return "HOST_DETECT_SPEED";
    case HOST_ENUMERATION:
        return "HOST_ENUM";
    case HOST_CLASS_REQUEST:
        return "HOST_CLASS_REQ";
    case HOST_INPUT:
        return "HOST_INPUT";
    case HOST_SET_CONFIGURATION:
        return "HOST_SET_CFG";
    case HOST_CHECK_CLASS:
        return "HOST_CHECK_CLASS";
    case HOST_CLASS:
        return "HOST_CLASS";
    case HOST_ABORT_STATE:
        return "HOST_ABORT";
    default:
        return "HOST_UNKNOWN";
    }
}

static const char *app_usb_enum_state_name(uint8_t state)
{
    switch (state) {
    case ENUM_IDLE:
        return "ENUM_IDLE";
    case ENUM_GET_FULL_DEV_DESC:
        return "ENUM_GET_DEV_DESC";
    case ENUM_SET_ADDR:
        return "ENUM_SET_ADDR";
    case ENUM_GET_CFG_DESC:
        return "ENUM_GET_CFG_DESC";
    case ENUM_GET_FULL_CFG_DESC:
        return "ENUM_GET_FULL_CFG";
    case ENUM_GET_MFC_STRING_DESC:
        return "ENUM_GET_MFC";
    case ENUM_GET_PRODUCT_STRING_DESC:
        return "ENUM_GET_PRODUCT";
    case ENUM_GET_SERIALNUM_STRING_DESC:
        return "ENUM_GET_SERIAL";
    default:
        return "ENUM_UNKNOWN";
    }
}

static const char *app_usb_req_state_name(uint8_t state)
{
    switch (state) {
    case CMD_IDLE:
        return "CMD_IDLE";
    case CMD_SEND:
        return "CMD_SEND";
    case CMD_WAIT:
        return "CMD_WAIT";
    default:
        return "CMD_UNKNOWN";
    }
}

static void app_log_usb_core_transitions(void)
{
    USBH_HandleTypeDef *phost = s_app.usb_host;
    uint8_t host_state;
    uint8_t enum_state;
    uint8_t request_state;
    uint8_t is_connected;
    uint8_t is_disconnected;
    uint8_t port_enabled;
    uint8_t speed;
    uint8_t address;
    uint8_t interface_index;
    uint16_t vid;
    uint16_t pid;

    if (phost == NULL) {
        return;
    }

    host_state = (uint8_t)phost->gState;
    enum_state = (uint8_t)phost->EnumState;
    request_state = (uint8_t)phost->RequestState;
    is_connected = (uint8_t)phost->device.is_connected;
    is_disconnected = (uint8_t)phost->device.is_disconnected;
    port_enabled = (uint8_t)phost->device.PortEnabled;
    speed = (uint8_t)phost->device.speed;
    address = (uint8_t)phost->device.address;
    interface_index = (uint8_t)phost->device.current_interface;
    vid = phost->device.DevDesc.idVendor;
    pid = phost->device.DevDesc.idProduct;

    if ((host_state == s_app.usb_prev_host_state) &&
        (enum_state == s_app.usb_prev_enum_state) &&
        (request_state == s_app.usb_prev_req_state) &&
        (is_connected == s_app.usb_prev_conn) &&
        (is_disconnected == s_app.usb_prev_disc) &&
        (port_enabled == s_app.usb_prev_port) &&
        (speed == s_app.usb_prev_speed) &&
        (address == s_app.usb_prev_addr) &&
        (interface_index == s_app.usb_prev_iface) &&
        (vid == s_app.usb_prev_vid) &&
        (pid == s_app.usb_prev_pid)) {
        return;
    }

    if ((s_app.usb_prev_conn != 0xFFU) && (s_app.usb_prev_conn != is_connected)) {
        if (is_connected != 0U) {
            printf("USB: connect(edge) t=%lu\r\n", (unsigned long)HAL_GetTick());
        } else {
            printf("USB: disconnect(edge) t=%lu\r\n", (unsigned long)HAL_GetTick());
        }
    }

    s_app.usb_prev_host_state = host_state;
    s_app.usb_prev_enum_state = enum_state;
    s_app.usb_prev_req_state = request_state;
    s_app.usb_prev_conn = is_connected;
    s_app.usb_prev_disc = is_disconnected;
    s_app.usb_prev_port = port_enabled;
    s_app.usb_prev_speed = speed;
    s_app.usb_prev_addr = address;
    s_app.usb_prev_iface = interface_index;
    s_app.usb_prev_vid = vid;
    s_app.usb_prev_pid = pid;

    printf("USB CORE t=%lu g=%s(%u) e=%s(%u) r=%s(%u) c=%u d=%u p=%u spd=%u addr=%u if=%u vid=%04X pid=%04X\r\n",
           (unsigned long)HAL_GetTick(),
           app_usb_host_state_name(host_state),
           host_state,
           app_usb_enum_state_name(enum_state),
           enum_state,
           app_usb_req_state_name(request_state),
           request_state,
           is_connected,
           is_disconnected,
           port_enabled,
           speed,
           address,
           interface_index,
           vid,
           pid);
}

static void app_log_button_transitions(void)
{
    uint32_t changed;
    uint8_t max_bits;
    uint8_t bit;

    if (s_app.joystick_state.hid_ready == 0U) {
        return;
    }

    if (s_app.prev_profile != s_app.joystick_state.profile) {
        s_app.prev_profile = s_app.joystick_state.profile;
        s_app.prev_buttons = s_app.joystick_state.buttons;
        s_app.prev_button_count = s_app.joystick_state.button_count;
        return;
    }

    changed = s_app.prev_buttons ^ s_app.joystick_state.buttons;
    if (changed == 0U) {
        return;
    }

    max_bits = s_app.joystick_state.button_count;
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
               ((s_app.joystick_state.buttons & mask) != 0U) ? "pressed" : "released",
               (unsigned long)s_app.joystick_state.buttons,
               s_app.joystick_state.button_offset,
               s_app.joystick_state.button_bytes);
    }

    s_app.prev_buttons = s_app.joystick_state.buttons;
    s_app.prev_button_count = s_app.joystick_state.button_count;
}

static void app_log_hid_decoded(uint32_t now_ms)
{
    if ((HID_DECODE_LOG_PERIOD_MS > 0U) &&
        ((now_ms - s_app.last_hid_decode_log_ms) < HID_DECODE_LOG_PERIOD_MS)) {
        return;
    }
    s_app.last_hid_decode_log_ms = now_ms;

    if (s_app.joystick_state.hid_ready == 0U) {
        return;
    }

    if (s_app.joystick_state.profile == HID_PROFILE_KEYBOARD_BOOT) {
        printf("HID KBD mod=%02X k=%02X %02X %02X %02X %02X %02X\r\n",
               s_app.joystick_state.key_modifiers,
               s_app.joystick_state.keys[0],
               s_app.joystick_state.keys[1],
               s_app.joystick_state.keys[2],
               s_app.joystick_state.keys[3],
               s_app.joystick_state.keys[4],
               s_app.joystick_state.keys[5]);
        app_log_button_transitions();
        return;
    }

    if (s_app.joystick_state.profile == HID_PROFILE_MOUSE_BOOT) {
        printf("HID MOUSE btn=%02lX dx=%d dy=%d wheel=%d\r\n",
               (unsigned long)(s_app.joystick_state.buttons & 0x1FU),
               (int)s_app.joystick_state.mouse_dx,
               (int)s_app.joystick_state.mouse_dy,
               (int)s_app.joystick_state.mouse_wheel);
        app_log_button_transitions();
        return;
    }

    printf("HID JOY vid=%04X pid=%04X proto=%u rid=%u off=%u ax=[%d,%d,%d,%d] btn=%08lX bc=%u hat=%d\r\n",
           s_app.joystick_state.vid,
           s_app.joystick_state.pid,
           s_app.joystick_state.interface_protocol,
           s_app.joystick_state.report_id,
           s_app.joystick_state.report_offset,
           (int)s_app.joystick_state.axes[0],
           (int)s_app.joystick_state.axes[1],
           (int)s_app.joystick_state.axes[2],
           (int)s_app.joystick_state.axes[3],
           (unsigned long)s_app.joystick_state.buttons,
           s_app.joystick_state.button_count,
           (int)s_app.joystick_state.hat);
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
    pending = s_app.can_rx_log.pending;
    id = s_app.can_rx_log.id;
    count = s_app.can_rx_log.count;
    ide = s_app.can_rx_log.ide;
    dlc = s_app.can_rx_log.dlc;
    d0 = s_app.can_rx_log.data[0];
    d1 = s_app.can_rx_log.data[1];
    s_app.can_rx_log.pending = 0U;
    __enable_irq();

    if (pending == 0U) {
        return;
    }

    if (ide == CAN_ID_STD) {
        printf("CAN RX cnt=%lu STD id=0x%03lX dlc=%u d0=%02X d1=%02X\r\n",
               (unsigned long)count,
               (unsigned long)id,
               dlc,
               d0,
               d1);
    } else {
        printf("CAN RX cnt=%lu EXT id=0x%08lX dlc=%u d0=%02X d1=%02X\r\n",
               (unsigned long)count,
               (unsigned long)id,
               dlc,
               d0,
               d1);
    }
}

static void app_uart_heartbeat(uint32_t now_ms)
{
    char buf[64];
    int length;

    if ((s_app.huart == NULL) ||
        ((now_ms - s_app.last_uart_heartbeat_ms) < UART_HEARTBEAT_PERIOD_MS)) {
        return;
    }
    s_app.last_uart_heartbeat_ms = now_ms;

    length = snprintf(buf,
                      sizeof(buf),
                      "UART HB t=%lu fail=%lu can=%u canerr=%lu\r\n",
                      (unsigned long)now_ms,
                      (unsigned long)s_app.uart_direct_tx_fail_count,
                      (unsigned)s_app.can_ready,
                      (unsigned long)s_app.can_init_error_count);
    if (length <= 0) {
        return;
    }
    if (HAL_UART_Transmit(s_app.huart,
                          (uint8_t *)buf,
                          (uint16_t)length,
                          100U) != HAL_OK) {
        s_app.uart_direct_tx_fail_count += 1U;
    }
}

static void app_sync_shared_joystick_state(uint32_t now_ms)
{
    uint8_t i;

    usb_host_hid_joystick_state_clear(&s_app.shared_joystick_state);
    s_app.shared_joystick_state.connected = (s_app.joystick_state.connected != 0U);
    s_app.shared_joystick_state.valid = (s_app.joystick_state.hid_ready != 0U);
    s_app.shared_joystick_state.stale = false;
    s_app.shared_joystick_state.report_id = s_app.joystick_state.report_id;
    s_app.shared_joystick_state.sequence = s_app.joystick_state.sequence;
    s_app.shared_joystick_state.timestamp_ms = now_ms;
    s_app.shared_joystick_state.axis_count = s_app.joystick_state.axis_count;

    for (i = 0U; (i < s_app.joystick_state.axis_count) && (i < USB_HOST_HID_MAX_AXES); i++) {
        s_app.shared_joystick_state.axes[i] = s_app.joystick_state.axes[i];
    }

    if (s_app.joystick_state.has_hat != 0U) {
        s_app.shared_joystick_state.hat_count = 1U;
        s_app.shared_joystick_state.hats[0] = s_app.joystick_state.hat;
    }

    s_app.shared_joystick_state.buttons = s_app.joystick_state.buttons;
    s_app.shared_joystick_state.button_count = s_app.joystick_state.button_count;
    joystick_can_pipeline_submit_sample(&s_app.pipeline,
                                        &s_app.shared_joystick_state);
}

int _write(int file, char *ptr, int len)
{
    (void)file;

    if ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0U) {
        return len;
    }
    if ((s_stdout_uart == NULL) || (ptr == NULL) || (len <= 0)) {
        return len;
    }
    if (HAL_UART_Transmit(s_stdout_uart,
                          (uint8_t *)ptr,
                          (uint16_t)len,
                          20U) != HAL_OK) {
        return 0;
    }

    return len;
}
