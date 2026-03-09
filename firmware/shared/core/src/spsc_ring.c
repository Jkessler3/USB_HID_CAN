#include "spsc_ring.h"

#include <string.h>

static uint16_t next_index(uint16_t index, uint16_t capacity)
{
    return (uint16_t)((index + 1U) % capacity);
}

void spsc_ring_init(spsc_ring_t *ring, uint8_t *storage, uint16_t item_size, uint16_t capacity)
{
    if (ring == NULL) {
        return;
    }

    ring->storage = storage;
    ring->item_size = item_size;
    ring->capacity = capacity;
    ring->head = 0U;
    ring->tail = 0U;
}

void spsc_ring_reset(spsc_ring_t *ring)
{
    if (ring == NULL) {
        return;
    }
    ring->head = 0U;
    ring->tail = 0U;
}

bool spsc_ring_push(spsc_ring_t *ring, const void *item)
{
    uint16_t head;
    uint16_t next;
    uint8_t *slot;

    if ((ring == NULL) || (item == NULL) || (ring->storage == NULL) || (ring->capacity < 2U) || (ring->item_size == 0U)) {
        return false;
    }

    head = ring->head;
    next = next_index(head, ring->capacity);
    if (next == ring->tail) {
        return false;
    }

    slot = &ring->storage[(size_t)head * ring->item_size];
    memcpy(slot, item, ring->item_size);
    ring->head = next;
    return true;
}

bool spsc_ring_pop(spsc_ring_t *ring, void *item)
{
    uint16_t tail;
    uint8_t *slot;

    if ((ring == NULL) || (item == NULL) || (ring->storage == NULL) || (ring->item_size == 0U)) {
        return false;
    }

    tail = ring->tail;
    if (tail == ring->head) {
        return false;
    }

    slot = &ring->storage[(size_t)tail * ring->item_size];
    memcpy(item, slot, ring->item_size);
    ring->tail = next_index(tail, ring->capacity);
    return true;
}

bool spsc_ring_peek(const spsc_ring_t *ring, void *item)
{
    uint8_t *slot;

    if ((ring == NULL) || (item == NULL) || (ring->storage == NULL) || (ring->item_size == 0U)) {
        return false;
    }

    if (ring->tail == ring->head) {
        return false;
    }

    slot = &ring->storage[(size_t)ring->tail * ring->item_size];
    memcpy(item, slot, ring->item_size);
    return true;
}

bool spsc_ring_is_empty(const spsc_ring_t *ring)
{
    if (ring == NULL) {
        return true;
    }
    return (ring->head == ring->tail);
}
