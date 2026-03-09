#ifndef SPSC_RING_H
#define SPSC_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *storage;
    uint16_t item_size;
    uint16_t capacity;
    volatile uint16_t head;
    volatile uint16_t tail;
} spsc_ring_t;

void spsc_ring_init(spsc_ring_t *ring, uint8_t *storage, uint16_t item_size, uint16_t capacity);
void spsc_ring_reset(spsc_ring_t *ring);

bool spsc_ring_push(spsc_ring_t *ring, const void *item);
bool spsc_ring_pop(spsc_ring_t *ring, void *item);
bool spsc_ring_peek(const spsc_ring_t *ring, void *item);
bool spsc_ring_is_empty(const spsc_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif /* SPSC_RING_H */
