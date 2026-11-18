#include "joystick_translator_app.h"
#include "joystick_uart_trace.h"

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
#define CAN_RX_LOG_PERIOD_MS     250U

typedef struct {
    volatile uint32_t id;
    volatile uint8_t ide;
    volatile uint8_t dlc;
    volatile uint8_t data[8];
    volatile uint32_t count;
    volatile uint8_t pending;
} can_rx_log_t;

typedef enum {
    CAN_SLOT_FREE,
    CAN_SLOT_QUEUED,
    CAN_SLOT_NEEDS_ABORT,
    CAN_SLOT_ABORTING
} can_slot_state_t;

typedef struct {
    uint32_t id;
    uint32_t source_ms;
    uint8_t hid_quality;
    can_slot_state_t state;
} can_tx_slot_t;

typedef struct {
    CAN_HandleTypeDef *hcan;
    UART_HandleTypeDef *huart;
    USBH_HandleTypeDef *usb_host;
    can_rx_log_t can_rx_log;
    ApplicationTypeDef last_appli_state;
    uint32_t last_can_tx_ms;
    uint8_t can_tx_sequence;
    uint32_t last_hid_log_ms;
    uint32_t last_hid_decode_log_ms;
    uint32_t last_can_tx_log_ms;
    uint32_t last_can_rx_log_ms;
    can_tx_slot_t tx_slots[3];
    uint32_t last_uart_heartbeat_ms;
    uint32_t uart_direct_tx_fail_count;
    uint32_t can_init_error_count;
    uint8_t can_ready;
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
    usb_host_hid_descriptor_t hid_layout;
    usb_host_hid_joystick_state_t shared_joystick_state;
    joystick_can_mapping_table_t mapping;
    joystick_can_pipeline_state_t pipeline;
} joystick_translator_runtime_t;

static joystick_translator_runtime_t s_app;
static UART_HandleTypeDef *s_stdout_uart;

static bool app_configure_can(void);
static void app_update_usb_state(ApplicationTypeDef appli_state, uint32_t now_ms);
static void app_send_mapped_can_frames(uint32_t now_ms);
static HAL_StatusTypeDef app_transmit_can_frame(const can_frame_t *frame, uint32_t *mailbox);
static void app_dump_raw_hid(const uint8_t *data, uint16_t len, uint32_t now_ms);
static void app_log_usb_core_transitions(void);
static const char *app_usb_host_state_name(uint8_t state);
static const char *app_usb_enum_state_name(uint8_t state);
static const char *app_usb_req_state_name(uint8_t state);
static void app_log_hid_decoded(uint32_t now_ms);
static void app_flush_can_rx_log(uint32_t now_ms);
static void app_uart_heartbeat(uint32_t now_ms);

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
    usb_host_hid_joystick_state_clear(&s_app.shared_joystick_state);
    joystick_can_mapping_table_init_default(&s_app.mapping);
    joystick_can_pipeline_reset(&s_app.pipeline);
    for (uint8_t i = 0U; i < JOYSTICK_CANAEROSPACE_PARAM_COUNT; i++) {
        s_app.pipeline.last_parameter_tx_ms[i] = now_ms;
    }
    s_app.pipeline.last_status_tx_ms = now_ms;
    usb_host_hid_device_set_stale_timeout(&s_app.hid_device,
                                          s_app.mapping.stale_timeout_ms);

    if (joystick_uart_trace_enabled() && (huart != NULL) &&
        (HAL_UART_Transmit(huart,
                           (uint8_t *)msg,
                           (uint16_t)strlen(msg),
                           100U) != HAL_OK)) {
        s_app.uart_direct_tx_fail_count += 1U;
    }

    s_stdout_uart = huart;
    JOYSTICK_TRACE_PRINTF("APP: init complete\r\n");
    s_app.can_ready = app_configure_can() ? 1U : 0U;
    if (s_app.can_ready != 0U) {
        JOYSTICK_TRACE_PRINTF("CAN: ready\r\n");
    } else {
        JOYSTICK_TRACE_PRINTF("CAN: init failed, UART kept alive\r\n");
    }
    JOYSTICK_TRACE_PRINTF("USB UART verbose mode enabled\r\n");
}

void joystick_translator_app_run(uint32_t now_ms,
                                 ApplicationTypeDef appli_state)
{
    app_update_usb_state(appli_state, now_ms);
    app_send_mapped_can_frames(now_ms);
    if (joystick_uart_trace_enabled()) {
        app_log_usb_core_transitions();
        app_flush_can_rx_log(now_ms);
        app_uart_heartbeat(now_ms);
        app_log_hid_decoded(now_ms);
    }
}

void USBH_HID_ReportDescriptorCallback(USBH_HandleTypeDef *phost, const uint8_t *descriptor, uint16_t length)
{
    if (phost == NULL || phost != s_app.usb_host) {
        return;
    }
    if (!usb_host_hid_compile_descriptor(descriptor, length, &s_app.hid_layout)) {
        s_app.shared_joystick_state.valid = false;
        joystick_can_pipeline_note_hid_parse_error(&s_app.pipeline);
    }
}

void USBH_HID_InputReportCallback(USBH_HandleTypeDef *phost, const uint8_t *report, uint16_t length)
{
    if (phost == NULL || phost != s_app.usb_host || !s_app.hid_device.class_active) {
        return;
    }
    uint32_t now_ms = HAL_GetTick();
    if (!usb_host_hid_decode_input(&s_app.hid_layout, report, length, now_ms,
                                   s_app.hid_device.report_sequence + 1U, &s_app.shared_joystick_state)) {
        s_app.shared_joystick_state.valid = false;
        joystick_can_pipeline_note_hid_parse_error(&s_app.pipeline);
        return;
    }
    usb_host_hid_on_report(&s_app.hid_device, now_ms);
    joystick_can_pipeline_submit_sample(&s_app.pipeline, &s_app.shared_joystick_state);
    if (joystick_uart_trace_enabled()) {
        app_dump_raw_hid(report, length, now_ms);
    }
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
        JOYSTICK_TRACE_PRINTF("USB APP state %u -> %u\r\n",
               (unsigned)s_app.last_appli_state,
               (unsigned)appli_state);

        if (appli_state == APPLICATION_START) {
            memset(&s_app.hid_layout, 0, sizeof(s_app.hid_layout));
            usb_host_hid_joystick_state_clear(&s_app.shared_joystick_state);
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
            memset(&s_app.hid_layout, 0, sizeof(s_app.hid_layout));
            usb_host_hid_on_disconnection(&s_app.hid_device, now_ms);
            joystick_can_pipeline_on_disconnect(&s_app.pipeline, now_ms);
            usb_host_hid_joystick_mark_disconnected(&s_app.shared_joystick_state,
                                                    now_ms);
        }

        s_app.last_appli_state = appli_state;
    }

}

static HAL_StatusTypeDef app_transmit_can_frame(const can_frame_t *frame, uint32_t *mailbox)
{
    CAN_TxHeaderTypeDef tx = {0};

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

    return HAL_CAN_AddTxMessage(s_app.hcan, &tx, frame->data, mailbox);
}

/* Only this task writes bxCAN's three mailboxes. Keep an ID unavailable through
 * asynchronous abort completion. A frame already on the bus cannot be retracted. */
static uint32_t app_pending_parameter_mask(uint32_t now_ms, uint8_t hid_quality)
{
    uint32_t unavailable = 0U;
    uint16_t timeout = s_app.mapping.stale_timeout_ms;
    for (uint8_t slot = 0U; slot < 3U; ++slot) {
        uint32_t mask = CAN_TX_MAILBOX0 << slot;
        can_tx_slot_t *pending = &s_app.tx_slots[slot];
        if (pending->state == CAN_SLOT_FREE) {
            continue;
        }
        if (!HAL_CAN_IsTxMessagePending(s_app.hcan, mask)) {
            pending->state = CAN_SLOT_FREE;
            continue;
        }
        bool expired = timeout != 0U && (now_ms - pending->source_ms) > timeout;
        if (pending->state == CAN_SLOT_QUEUED &&
            (pending->hid_quality != hid_quality || expired)) {
            pending->state = CAN_SLOT_NEEDS_ABORT;
        }
        if (pending->state == CAN_SLOT_NEEDS_ABORT &&
            HAL_CAN_AbortTxRequest(s_app.hcan, mask) == HAL_OK) {
            pending->state = CAN_SLOT_ABORTING;
        }
        for (uint8_t param = 0U; param < JOYSTICK_CANAEROSPACE_PARAM_COUNT; ++param) {
            if (s_app.mapping.parameter_can_ids[param] == pending->id) {
                unavailable |= UINT32_C(1) << param;
            }
        }
    }
    return unavailable;
}

static void app_send_mapped_can_frames(uint32_t now_ms)
{
    can_frame_t frame;
    joystick_can_tx_kind_t kind;
    uint32_t mailbox;

    if ((s_app.hcan == NULL) || (s_app.can_ready == 0U)) {
        return;
    }

    uint8_t hid_quality = joystick_can_pipeline_hid_quality(&s_app.mapping, &s_app.pipeline, now_ms);
    uint32_t unavailable = app_pending_parameter_mask(now_ms, hid_quality);

    if (!joystick_can_pipeline_prepare_available(&s_app.mapping, &s_app.pipeline, now_ms,
                                                 unavailable, &frame, &kind)) {
        return;
    }

    HAL_StatusTypeDef result = app_transmit_can_frame(&frame, &mailbox);
    if (result == HAL_OK) {
        joystick_can_pipeline_commit_frame(&s_app.pipeline, kind, now_ms);
        for (uint8_t slot = 0U; slot < 3U; ++slot) {
            if ((CAN_TX_MAILBOX0 << slot) == mailbox) {
                can_tx_slot_t *pending = &s_app.tx_slots[slot];
                pending->id = frame.id;
                pending->source_ms = hid_quality == 0U ? s_app.pipeline.last_report_ms : now_ms;
                pending->hid_quality = hid_quality;
                pending->state = CAN_SLOT_QUEUED;
                break;
            }
        }
        s_app.last_can_tx_ms = now_ms;
        s_app.can_tx_sequence += 1U;

        if (!joystick_uart_trace_enabled() || (now_ms - s_app.last_can_tx_log_ms) < CAN_TX_LOG_PERIOD_MS) {
            return;
        }
        s_app.last_can_tx_log_ms = now_ms;

        printf("CAN TX ca id=%lu/0x%03lX kind=%u seq=%lu node=%u type=0x%02X svc=0x%02X msg=0x%02X data=%02X%02X%02X%02X\r\n",
               (unsigned long)frame.id,
               (unsigned long)frame.id,
               (unsigned int)kind,
               (unsigned long)s_app.pipeline.frame_sequence,
               frame.data[0],
               frame.data[1],
               frame.data[2],
               frame.data[3],
               frame.data[4],
               frame.data[5],
               frame.data[6],
               frame.data[7]);
    } else {
        uint16_t period = kind == JOYSTICK_CAN_TX_STATUS ? s_app.mapping.status_period_ms
                                                         : s_app.mapping.parameter_period_ms;
        uint8_t param = (uint8_t)(kind - JOYSTICK_CAN_TX_AXIS_XY);
        uint32_t elapsed = now_ms - s_app.pipeline.last_parameter_tx_ms[param];
        /* An ordinary burst can wait briefly for one of the three mailboxes.
         * Record a busy episode once a second scheduled opportunity is reached. */
        if (result != HAL_BUSY || (period != 0U && elapsed >= 2U * (uint32_t)period)) {
            joystick_can_pipeline_note_tx_backpressure(&s_app.pipeline);
        }
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
           (unsigned long)s_app.hid_device.report_sequence);
    for (i = 0U; i < limit; i++) {
        printf(" %02X", data[i]);
    }
    if (limit < len) {
        printf(" ...");
    }
    printf("\r\n");
}

static const char *app_usb_host_state_name(uint8_t state)
{
    switch (state) {
    case HOST_IDLE:
        return "HOST_IDLE";
    case HOST_DEV_WAIT_FOR_ATTACHMENT:
        return "HOST_WAIT_ATTACH";
    case HOST_DEV_ATTACH_SETTLE:
        return "HOST_ATTACH_SETTLE";
    case HOST_DEV_RESET_ASSERTED:
        return "HOST_RESET_ASSERTED";
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

static void app_log_hid_decoded(uint32_t now_ms)
{
    const usb_host_hid_joystick_state_t *sample = &s_app.shared_joystick_state;
    if (!sample->valid || usb_host_hid_joystick_is_stale(&s_app.hid_device, now_ms) ||
        now_ms - s_app.last_hid_decode_log_ms < HID_DECODE_LOG_PERIOD_MS) {
        return;
    }
    s_app.last_hid_decode_log_ms = now_ms;
    printf("HID JOY vid=%04X pid=%04X proto=0 rid=%u off=%u ax=[",
           s_app.hid_device.vid, s_app.hid_device.pid, sample->report_id,
           (unsigned)s_app.hid_layout.has_report_id);
    for (uint8_t i = 0; i < sample->axis_count; i++) {
        printf("%s%d", i ? "," : "", (int)sample->axes[i]);
    }
    printf("] btn=%08lX%08lX bc=%u hat=%d\r\n",
           (unsigned long)(uint32_t)(sample->buttons >> 32),
           (unsigned long)(uint32_t)sample->buttons, sample->button_count,
           sample->hat_count ? (int)sample->hats[0] : -1);
}

static void app_flush_can_rx_log(uint32_t now_ms)
{
    uint32_t id;
    uint32_t count;
    uint8_t ide;
    uint8_t dlc;
    uint8_t d0;
    uint8_t d1;
    uint8_t pending;

    if ((now_ms - s_app.last_can_rx_log_ms) < CAN_RX_LOG_PERIOD_MS) {
        return;
    }
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
    s_app.last_can_rx_log_ms = now_ms;

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

int _write(int file, char *ptr, int len)
{
    (void)file;

    if (!joystick_uart_trace_enabled()) {
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
