#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"
#include "ring_buffer.h"

int net_read_stream(int sock, RingBuffer *rb);
int net_extract_packet(RingBuffer *rb, TWireHeader *out_hdr, TChatPayload *out_payload);
int net_send_packet(int sock, uint16_t type, const TChatPayload *payload);
uint32_t net_crc32(const uint8_t *data, size_t length);

#endif
