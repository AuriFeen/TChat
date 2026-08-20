#include "stun.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define STUN_MAGIC_COOKIE 0x2112A442

int stun_discover_endpoint(const char *stun_host, int stun_port,
                           char *out_ip, size_t ip_len, int *out_port)
{
    struct addrinfo hints = {0}, *res;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", stun_port);

    if (getaddrinfo(stun_host, port_str, &hints, &res) != 0)
        return -1;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }

    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t req[20];
    req[0] = 0x00; req[1] = 0x01;
    req[2] = 0x00; req[3] = 0x00;
    req[4] = 0x21; req[5] = 0x12;
    req[6] = 0xA4; req[7] = 0x42;

    srand((unsigned)(time(NULL) ^ getpid()));
    for (int i = 8; i < 20; i++) req[i] = rand() & 0xFF;

    if (sendto(sock, req, 20, 0, res->ai_addr, res->ai_addrlen) != 20) {
        close(sock); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);

    uint8_t resp[256];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int n = recvfrom(sock, resp, sizeof(resp), 0, (struct sockaddr *)&from, &fromlen);
    close(sock);
    if (n < 20) return -1;

    if (resp[0] != 0x01 || resp[1] != 0x01) return -1;
    if (resp[4] != 0x21 || resp[5] != 0x12 || resp[6] != 0xA4 || resp[7] != 0x42)
        return -1;
    if (memcmp(req + 8, resp + 8, 12) != 0) return -1;

    uint16_t msg_len = ((uint16_t)resp[2] << 8) | resp[3];
    int pos = 20;
    int end = 20 + msg_len;

    while (pos + 4 <= n && pos < end) {
        uint16_t attr_type = ((uint16_t)resp[pos] << 8) | resp[pos + 1];
        uint16_t attr_len  = ((uint16_t)resp[pos + 2] << 8) | resp[pos + 3];
        pos += 4;

        if (attr_type == 0x0020 && attr_len >= 8 && resp[pos + 1] == 0x01) {
            uint16_t xport = ((uint16_t)resp[pos + 2] << 8) | resp[pos + 3];
            uint16_t port  = xport ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16);

            uint32_t xaddr = ((uint32_t)resp[pos + 4] << 24) |
                             ((uint32_t)resp[pos + 5] << 16) |
                             ((uint32_t)resp[pos + 6] << 8)  | resp[pos + 7];
            uint32_t addr  = xaddr ^ STUN_MAGIC_COOKIE;

            struct in_addr in = { .s_addr = htonl(addr) };
            inet_ntop(AF_INET, &in, out_ip, ip_len);
            *out_port = port;
            return 0;
        }
        else if (attr_type == 0x0001 && attr_len >= 8 && resp[pos + 1] == 0x01) {
            uint16_t port = ((uint16_t)resp[pos + 2] << 8) | resp[pos + 3];
            uint32_t addr = ((uint32_t)resp[pos + 4] << 24) |
                            ((uint32_t)resp[pos + 5] << 16) |
                            ((uint32_t)resp[pos + 6] << 8)  | resp[pos + 7];
            struct in_addr in = { .s_addr = htonl(addr) };
            inet_ntop(AF_INET, &in, out_ip, ip_len);
            *out_port = port;
            return 0;
        }

        pos += attr_len;
        if (attr_len % 4) pos += 4 - (attr_len % 4);
    }
    return -1;
}
