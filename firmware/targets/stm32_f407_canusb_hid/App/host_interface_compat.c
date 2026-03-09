#include "host_interface_compat.h"

#include "hid_protocol.h"
#include "usb_hid_transport.h"

static uint32_t s_compat_seq = 1U;

static uint32_t next_seq(void)
{
    uint32_t seq = s_compat_seq;
    s_compat_seq++;
    if (s_compat_seq == 0U) {
        s_compat_seq = 1U;
    }
    return seq;
}

void host_interface_compat_init(void)
{
    s_compat_seq = 1U;
}

bool host_interface_send_can_rx(const can_frame_t *frame, uint32_t timestamp_ms)
{
    return hid_transport_send_frame(frame, timestamp_ms, next_seq(), HID_PROTOCOL_EVT_CAN_RX);
}

bool host_interface_send_obd_mode01(uint8_t pid,
                                    const uint8_t *payload,
                                    uint8_t payload_len,
                                    uint32_t timestamp_ms)
{
    return hid_transport_send_obd_response(pid, payload, payload_len, next_seq(), timestamp_ms);
}
