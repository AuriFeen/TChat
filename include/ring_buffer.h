#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#define RB_SIZE 8192

typedef struct {
    uint8_t buffer[RB_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} RingBuffer;

void rb_init(RingBuffer *rb);
int rb_push(RingBuffer *rb, const uint8_t *data, size_t len);
int rb_pop(RingBuffer *rb, uint8_t *out, size_t len);
int rb_peek(const RingBuffer *rb, uint8_t *out, size_t offset, size_t len);
void rb_advance_tail(RingBuffer *rb, size_t len);

#endif
