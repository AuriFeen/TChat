#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"
#include "ring_buffer.h"

uint32_t net_crc32(const uint8_t *data, size_t length);
int net_read_stream(int sock, RingBuffer *rb);
int net_extract_packet(RingBuffer *rb, TWireHeader *out_hdr, TChatPayload *out_payload);
int net_send_packet(int sock, uint16_t type, const TChatPayload *payload);

#endif
