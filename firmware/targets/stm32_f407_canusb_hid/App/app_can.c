#include "app_can.h"

#include <string.h>

#include "can.h"

static app_can_rx_callback_t s_rx_callback;

static bool restart_can_after_reconfig(void)
{
    if (HAL_CAN_Init(&hcan1) != HAL_OK) {
        return false;
    }
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        return false;
    }
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return false;
    }
    return true;
}

static void encode_filter_registers(uint32_t id,
                                    uint32_t mask,
                                    bool extended,
                                    uint16_t *id_high,
                                    uint16_t *id_low,
                                    uint16_t *mask_high,
                                    uint16_t *mask_low)
{
    uint32_t packed_id;
    uint32_t packed_mask;

    if (extended) {
        packed_id = ((id & 0x1FFFFFFFU) << 3U) | (1U << 2U);
        packed_mask = ((mask & 0x1FFFFFFFU) << 3U) | (1U << 2U);
    } else {
        packed_id = (id & 0x7FFU) << 21U;
        packed_mask = (mask & 0x7FFU) << 21U;
    }

    *id_high = (uint16_t)((packed_id >> 16U) & 0xFFFFU);
    *id_low = (uint16_t)(packed_id & 0xFFFFU);
    *mask_high = (uint16_t)((packed_mask >> 16U) & 0xFFFFU);
    *mask_low = (uint16_t)(packed_mask & 0xFFFFU);
}

void app_can_init(app_can_rx_callback_t rx_callback)
{
    s_rx_callback = rx_callback;
}

bool app_can_start(void)
{
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        return false;
    }
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return false;
    }
    return true;
}

bool app_can_send_frame(const can_frame_t *frame)
{
    CAN_TxHeaderTypeDef tx_header;
    uint32_t mailbox;
    uint8_t payload[CAN_FRAME_MAX_DATA_LEN];

    if (!can_frame_is_valid(frame)) {
        return false;
    }

    memset(&tx_header, 0, sizeof(tx_header));
    tx_header.IDE = frame->extended ? CAN_ID_EXT : CAN_ID_STD;
    tx_header.RTR = frame->rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    tx_header.DLC = frame->dlc;
    if (frame->extended) {
        tx_header.ExtId = frame->id & 0x1FFFFFFFU;
    } else {
        tx_header.StdId = frame->id & 0x7FFU;
    }

    memset(payload, 0, sizeof(payload));
    memcpy(payload, frame->data, frame->dlc);

    return (HAL_CAN_AddTxMessage(&hcan1, &tx_header, payload, &mailbox) == HAL_OK);
}

bool app_can_set_bitrate(uint32_t bitrate_bps)
{
    const uint32_t tq_per_bit = 14U;
    const uint32_t can_clk_hz = HAL_RCC_GetPCLK1Freq();
    uint32_t denominator;
    uint32_t prescaler;

    if (bitrate_bps == 0U) {
        return false;
    }

    denominator = bitrate_bps * tq_per_bit;
    if ((denominator == 0U) || (can_clk_hz < denominator)) {
        return false;
    }

    prescaler = can_clk_hz / denominator;
    if ((prescaler == 0U) || (prescaler > 1024U) || ((can_clk_hz % denominator) != 0U)) {
        return false;
    }

    (void)HAL_CAN_Stop(&hcan1);
    hcan1.Init.Prescaler = prescaler;
    return restart_can_after_reconfig();
}

bool app_can_set_filter(uint32_t can_id, uint32_t mask, bool extended)
{
    CAN_FilterTypeDef filter;

    memset(&filter, 0, sizeof(filter));
    filter.FilterBank = 0U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterActivation = ENABLE;

    encode_filter_registers(can_id,
                            mask,
                            extended,
                            &filter.FilterIdHigh,
                            &filter.FilterIdLow,
                            &filter.FilterMaskIdHigh,
                            &filter.FilterMaskIdLow);

    return (HAL_CAN_ConfigFilter(&hcan1, &filter) == HAL_OK);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    can_frame_t frame;
    uint8_t payload[CAN_FRAME_MAX_DATA_LEN];

    if ((hcan == NULL) || (hcan->Instance != CAN1)) {
        return;
    }
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, payload) != HAL_OK) {
        return;
    }

    can_frame_clear(&frame);
    frame.extended = (rx_header.IDE == CAN_ID_EXT);
    frame.rtr = (rx_header.RTR == CAN_RTR_REMOTE);
    frame.id = frame.extended ? rx_header.ExtId : rx_header.StdId;
    frame.dlc = (rx_header.DLC <= CAN_FRAME_MAX_DATA_LEN) ? rx_header.DLC : CAN_FRAME_MAX_DATA_LEN;
    memcpy(frame.data, payload, frame.dlc);

    if (s_rx_callback != NULL) {
        s_rx_callback(&frame, HAL_GetTick());
    }
}
