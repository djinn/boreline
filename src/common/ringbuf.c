#include "boreline/boreline.h"

#include <stdlib.h>
#include <string.h>

/* Bounded ring buffer with single contiguous allocation.
 *
 * Implements a classic ring buffer with head/tail indices. All operations
 * are O(1). The capacity is fixed at creation time; writes beyond capacity
 * are truncated (caller checks available space via boreline_ringbuf_available).
 */

struct boreline_ringbuf {
    uint8_t *data;
    size_t   capacity;
    size_t   head; /* read position */
    size_t   tail; /* write position */
    size_t   size; /* bytes currently stored */
};

boreline_ringbuf_t *boreline_ringbuf_create(size_t capacity)
{
    if (capacity == 0 || capacity > (size_t)1 << 32)
        return NULL;

    boreline_ringbuf_t *rb = calloc(1, sizeof(*rb));
    if (rb == NULL)
        return NULL;

    rb->data = malloc(capacity);
    if (rb->data == NULL) {
        free(rb);
        return NULL;
    }

    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->size = 0;
    return rb;
}

void boreline_ringbuf_destroy(boreline_ringbuf_t *rb)
{
    if (rb == NULL)
        return;
    free(rb->data);
    free(rb);
}

size_t boreline_ringbuf_write(boreline_ringbuf_t *rb,
                               const uint8_t *data, size_t len)
{
    if (rb == NULL || data == NULL)
        return 0;

    size_t space = rb->capacity - rb->size;
    if (len > space)
        len = space;
    if (len == 0)
        return 0;

    /* First chunk: from tail to end of buffer */
    size_t first = rb->capacity - rb->tail;
    if (first > len)
        first = len;
    memcpy(rb->data + rb->tail, data, first);

    /* Second chunk: wrapped, if any */
    if (len > first) {
        memcpy(rb->data, data + first, len - first);
    }

    rb->tail = (rb->tail + len) % rb->capacity;
    rb->size += len;
    return len;
}

size_t boreline_ringbuf_read(boreline_ringbuf_t *rb,
                              uint8_t *data, size_t len)
{
    if (rb == NULL || (data == NULL && len > 0))
        return 0;

    if (len > rb->size)
        len = rb->size;
    if (len == 0)
        return 0;

    size_t first = rb->capacity - rb->head;
    if (first > len)
        first = len;
    if (data != NULL)
        memcpy(data, rb->data + rb->head, first);
    if (len > first) {
        if (data != NULL)
            memcpy(data + first, rb->data, len - first);
    }

    rb->head = (rb->head + len) % rb->capacity;
    rb->size -= len;
    return len;
}

size_t boreline_ringbuf_available(const boreline_ringbuf_t *rb)
{
    return rb ? rb->size : 0;
}

size_t boreline_ringbuf_capacity(const boreline_ringbuf_t *rb)
{
    return rb ? rb->capacity : 0;
}

void boreline_ringbuf_reset(boreline_ringbuf_t *rb)
{
    if (rb == NULL)
        return;
    rb->head = 0;
    rb->tail = 0;
    rb->size = 0;
}