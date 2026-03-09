#include "app_main.h"

#include "app_can.h"
#include "host_interface_compat.h"
#include "main.h"
#include "obd_mode01.h"
#include "usb_hid_transport.h"
#include "usbd_custom_hid_if.h"

static bool app_transport_can_tx(const can_frame_t *frame)
{
    return app_can_send_frame(frame);
}

static bool app_transport_obd_request(uint8_t mode, uint8_t pid, uint32_t seq)
{
    can_frame_t request;

    (void)seq;

    if (mode != OBD_MODE01_CURRENT_DATA) {
        return false;
    }
    if (!obd_mode01_build_request(pid, &request)) {
        return false;
    }
    return app_can_send_frame(&request);
}

static bool app_transport_set_bitrate(uint32_t bitrate_bps)
{
    return app_can_set_bitrate(bitrate_bps);
}

static bool app_transport_set_filter(uint32_t can_id, uint32_t mask, bool extended)
{
    return app_can_set_filter(can_id, mask, extended);
}

static void app_on_can_rx(const can_frame_t *frame, uint32_t timestamp_ms)
{
    (void)hid_transport_queue_can_rx_from_isr(frame, timestamp_ms);
}

void app_init(void)
{
    hid_transport_callbacks_t callbacks;

    callbacks.can_tx = app_transport_can_tx;
    callbacks.obd_request = app_transport_obd_request;
    callbacks.set_bitrate = app_transport_set_bitrate;
    callbacks.set_filter = app_transport_set_filter;

    app_can_init(app_on_can_rx);
    if (!app_can_start()) {
        Error_Handler();
    }

    hid_transport_init(&callbacks);
    hid_transport_set_usb_tx_callback(CUSTOM_HID_SendReport_FS);
    host_interface_compat_init();
}

void app_loop(void)
{
    hid_transport_process_host_reports();
    hid_transport_poll_tx_queue();
}

void app_tick_1ms(void)
{
}
