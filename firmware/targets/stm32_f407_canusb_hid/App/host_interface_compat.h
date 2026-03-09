#ifndef HOST_INTERFACE_COMPAT_H
#define HOST_INTERFACE_COMPAT_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void host_interface_compat_init(void);

bool host_interface_send_can_rx(const can_frame_t *frame, uint32_t timestamp_ms);
bool host_interface_send_obd_mode01(uint8_t pid,
                                    const uint8_t *payload,
                                    uint8_t payload_len,
                                    uint32_t timestamp_ms);

#ifdef __cplusplus
}
#endif

#endif /* HOST_INTERFACE_COMPAT_H */
