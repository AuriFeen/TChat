#include "network.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "ring_buffer.h"

uint32_t net_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

int net_read_stream(int sock, RingBuffer *rb) {
    uint8_t temp[2048];
    ssize_t bytes = recv(sock, temp, sizeof(temp), 0);
    if (bytes <= 0) return (int)bytes;
    
    if (rb_push(rb, temp, bytes) < 0) {
        rb_init(rb); // Flush ring buffer on hard overflow constraint violation
    }
    return (int)bytes;
}

int net_extract_packet(RingBuffer *rb, TWireHeader *out_hdr, TChatPayload *out_payload) {
    while (rb->count >= sizeof(TWireHeader)) {
        TWireHeader hdr;
        rb_peek(rb, (uint8_t*)&hdr, 0, sizeof(TWireHeader));

        if (hdr.magic != PROTO_MAGIC) {
            rb_advance_tail(rb, 1);
            continue;
        }

        if (hdr.length > sizeof(TChatPayload)) {
            rb_advance_tail(rb, sizeof(TWireHeader));
            continue;
        }

        if (rb->count < (sizeof(TWireHeader) + hdr.length)) {
            return 0; // Wait for missing stream bytes
        }

        rb_advance_tail(rb, sizeof(TWireHeader));
        memset(out_payload, 0, sizeof(TChatPayload));
        if (hdr.length > 0) {
            rb_pop(rb, (uint8_t*)out_payload, hdr.length);
        }

        uint32_t calculated_crc = net_crc32((const uint8_t*)out_payload, hdr.length);
        if (calculated_crc != hdr.checksum) {
            continue; // Drop corrupt packet silently
        }

        *out_hdr = hdr;
        return 1;
    }
    return 0;
}

int net_send_packet(int sock, uint16_t type, const TChatPayload *payload) {
    TWireHeader hdr = {
        .magic = PROTO_MAGIC,
        .type = type,
        .length = payload ? sizeof(TChatPayload) : 0,
        .checksum = payload ? net_crc32((const uint8_t*)payload, sizeof(TChatPayload)) : 0
    };

    uint8_t tx_buf[sizeof(TWireHeader) + sizeof(TChatPayload)];
    memcpy(tx_buf, &hdr, sizeof(TWireHeader));
    if (payload) {
        memcpy(tx_buf + sizeof(TWireHeader), payload, sizeof(TChatPayload));
    }

    size_t total_len = sizeof(TWireHeader) + hdr.length;
    size_t total_sent = 0;
    
    while (total_sent < total_len) {
        ssize_t sent = send(sock, tx_buf + total_sent, total_len - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) return -1;
        total_sent += sent;
    }
    return (int)total_sent;
}
