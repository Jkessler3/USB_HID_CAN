#ifndef TARGET_BOARD_H
#define TARGET_BOARD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void target_board_init(void);
void target_can_transceiver_set(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* TARGET_BOARD_H */
