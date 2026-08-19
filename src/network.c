#include "tailscale.h"
#include "network.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static tailscale ts_handle = 0;

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

IrohEndpoint* iroh_net_init_node(const char *alpn_str) {
    (void)alpn_str;
    
    ts_handle = tailscale_new();
    if (ts_handle <= 0) {
        fprintf(stderr, "Failed to create tailscale context\n");
        return NULL;
    }

    tailscale_set_control_url(ts_handle, "https://your-headscale.yourdomain.com");
    tailscale_set_dir(ts_handle, "./tstate");

    if (tailscale_start(ts_handle) != 0) {
        fprintf(stderr, "Failed to start embedded tailscale node engine\n");
        return NULL;
    }

    return (IrohEndpoint*)(intptr_t)ts_handle;
}

char* iroh_net_get_node_id_str(IrohEndpoint *endpoint) {
    (void)endpoint;
    char ip_buf[128];
    snprintf(ip_buf, sizeof(ip_buf), "100.64.0.1"); 
    return strdup(ip_buf);
}

IrohConnection* iroh_net_accept(IrohEndpoint *endpoint) {
    (void)endpoint;
    tailscale_listener listener = 0;
    
    if (tailscale_listen(ts_handle, "tcp", "0.0.0.0:7777", &listener) != 0 || listener <= 0) {
        return NULL;
    }

    tailscale_conn conn = 0;
    if (tailscale_accept(listener, &conn) != 0 || conn <= 0) {
        tailscale_close(listener);
        return NULL;
    }
    
    tailscale_close(listener);
    return (IrohConnection*)(intptr_t)conn;
}

IrohConnection* iroh_net_connect(IrohEndpoint *endpoint, const char *node_id_str) {
    (void)endpoint;
    char target_addr[128];
    snprintf(target_addr, sizeof(target_addr), "%s:7777", node_id_str);

    tailscale_conn conn = 0;
    if (tailscale_dial(ts_handle, "tcp", target_addr, &conn) != 0 || conn <= 0) {
        return NULL;
    }
    return (IrohConnection*)(intptr_t)conn;
}

int iroh_net_read_stream(IrohConnection *conn, RingBuffer *rb) {
    tailscale_conn c = (tailscale_conn)(intptr_t)conn;
    uint8_t temp[2048];
    
    int read_bytes = read(c, temp, sizeof(temp));
    if (read_bytes <= 0) {
        return -1;
    }
    
    if (rb_push(rb, temp, (size_t)read_bytes) < 0) {
        rb_init(rb);
    }
    return read_bytes;
}

int iroh_net_extract_packet(RingBuffer *rb, TWireHeader *out_hdr, TChatPayload *out_payload) {
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
            return 0; 
        }

        rb_advance_tail(rb, sizeof(TWireHeader));
        memset(out_payload, 0, sizeof(TChatPayload));
        if (hdr.length > 0) {
            rb_pop(rb, (uint8_t*)out_payload, hdr.length);
        }

        uint32_t calculated_crc = net_crc32((const uint8_t*)out_payload, hdr.length);
        if (calculated_crc != hdr.checksum) {
            continue; 
        }

        *out_hdr = hdr;
        return 1;
    }
    return 0;
}

int iroh_net_send_packet(IrohConnection *conn, uint16_t type, const TChatPayload *payload) {
    tailscale_conn c = (tailscale_conn)(intptr_t)conn;
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
    int written = write(c, tx_buf, total_len);
    if (written < 0) return -1;
    
    return written;
}

void iroh_net_close(IrohConnection *conn) {
    if (conn) {
        tailscale_conn c = (tailscale_conn)(intptr_t)conn;
        tailscale_close(c);
    }
}
