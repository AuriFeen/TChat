#include "network.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

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

struct IrohEndpoint {
    int listen_fd;
    int is_server;
};

IrohEndpoint* iroh_net_init_node(int listen_port) {
    struct IrohEndpoint *ctx = calloc(1, sizeof(struct IrohEndpoint));
    if (!ctx) return NULL;

    if (listen_port > 0) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { free(ctx); return NULL; }

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(listen_port);

        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(fd); free(ctx); return NULL;
        }
        if (listen(fd, 128) < 0) {
            perror("listen");
            close(fd); free(ctx); return NULL;
        }
        ctx->listen_fd = fd;
        ctx->is_server = 1;
    }
    return ctx;
}

char* iroh_net_get_node_id_str(IrohEndpoint *endpoint) {
    (void)endpoint;
    return strdup("tchat-node");
}

IrohConnection* iroh_net_accept(IrohEndpoint *endpoint) {
    if (!endpoint || !endpoint->is_server) return NULL;
    struct sockaddr_in c;
    socklen_t l = sizeof(c);
    int fd = accept(endpoint->listen_fd, (struct sockaddr*)&c, &l);
    return (fd < 0) ? NULL : (IrohConnection*)(intptr_t)fd;
}

IrohConnection* iroh_net_connect(IrohEndpoint *endpoint, const char *node_id_str) {
    (void)endpoint;
    char ip[128] = {0};
    int port = 7777;

    strncpy(ip, node_id_str, sizeof(ip)-1);
    char *colon = strrchr(ip, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon+1);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        struct hostent *h = gethostbyname(ip);
        if (!h) { close(fd); return NULL; }
        memcpy(&addr.sin_addr, h->h_addr_list[0], h->h_length);
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd); return NULL;
    }
    return (IrohConnection*)(intptr_t)fd;
}

int iroh_net_read_stream(IrohConnection *conn, RingBuffer *rb) {
    int fd = (int)(intptr_t)conn;
    uint8_t tmp[2048];
    int n = read(fd, tmp, sizeof(tmp));
    if (n <= 0) return -1;
    if (rb_push(rb, tmp, (size_t)n) < 0) rb_init(rb);
    return n;
}

int iroh_net_extract_packet(RingBuffer *rb, TWireHeader *out_hdr, TChatPayload *out_payload) {
    while (rb->count >= sizeof(TWireHeader)) {
        TWireHeader hdr;
        rb_peek(rb, (uint8_t*)&hdr, 0, sizeof(TWireHeader));

        if (hdr.magic != PROTO_MAGIC) { rb_advance_tail(rb, 1); continue; }
        if (hdr.length > sizeof(TChatPayload)) { rb_advance_tail(rb, sizeof(TWireHeader)); continue; }
        if (rb->count < sizeof(TWireHeader) + hdr.length) return 0;

        rb_advance_tail(rb, sizeof(TWireHeader));
        memset(out_payload, 0, sizeof(TChatPayload));
        if (hdr.length > 0) rb_pop(rb, (uint8_t*)out_payload, hdr.length);

        if (net_crc32((const uint8_t*)out_payload, hdr.length) != hdr.checksum) continue;
        *out_hdr = hdr;
        return 1;
    }
    return 0;
}

int iroh_net_send_packet(IrohConnection *conn, uint16_t type, const TChatPayload *payload) {
    int fd = (int)(intptr_t)conn;
    TWireHeader hdr = {
        .magic = PROTO_MAGIC,
        .type = type,
        .length = payload ? sizeof(TChatPayload) : 0,
        .checksum = payload ? net_crc32((const uint8_t*)payload, sizeof(TChatPayload)) : 0
    };
    uint8_t buf[sizeof(TWireHeader) + sizeof(TChatPayload)];
    memcpy(buf, &hdr, sizeof(TWireHeader));
    if (payload) memcpy(buf + sizeof(TWireHeader), payload, sizeof(TChatPayload));

    size_t total = sizeof(TWireHeader) + hdr.length;
    int w = write(fd, buf, total);
    return (w < 0) ? -1 : w;
}

void iroh_net_close(IrohConnection *conn) {
    int fd = (int)(intptr_t)conn;
    if (fd > 0) close(fd);
}
