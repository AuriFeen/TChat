#include "ring_buffer.h"
#include <string.h>

void rb_init(RingBuffer *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    memset(rb->buffer, 0, RB_SIZE);
}

int rb_push(RingBuffer *rb, const uint8_t *data, size_t len) {
    if (len > (RB_SIZE - rb->count)) return -1;
    for (size_t i = 0; i < len; i++) {
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1) % RB_SIZE;
    }
    rb->count += len;
    return 0;
}

int rb_pop(RingBuffer *rb, uint8_t *out, size_t len) {
    if (rb->count < len) return -1;
    for (size_t i = 0; i < len; i++) {
        out[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % RB_SIZE;
    }
    rb->count -= len;
    return 0;
}

int rb_peek(const RingBuffer *rb, uint8_t *out, size_t offset, size_t len) {
    if (rb->count < (offset + len)) return -1;
    size_t curr_tail = (rb->tail + offset) % RB_SIZE;
    for (size_t i = 0; i < len; i++) {
        out[i] = rb->buffer[curr_tail];
        curr_tail = (curr_tail + 1) % RB_SIZE;
    }
    return 0;
}

void rb_advance_tail(RingBuffer *rb, size_t len) {
    if (len > rb->count) len = rb->count;
    rb->tail = (rb->tail + len) % RB_SIZE;
    rb->count -= len;
}
