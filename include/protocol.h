#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_MAGIC 0x54434841 // "TCHA" in ASCII
#define TCHAT_ALPN  "tchat/p2p-mesh/1.0"
#define MAX_NICK    32
#define MAX_MSG     512
#define MAX_FDS     256

typedef enum {
    TYPE_JOIN,
    TYPE_CHAT,
    TYPE_PRIVATE,
    TYPE_SERVER,
    TYPE_CMD_USERS,
    TYPE_NAME_CHANGE,
    TYPE_PING,
    TYPE_PONG
} PacketType;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t type;
    uint32_t length;
    uint32_t checksum;
} TWireHeader;

typedef struct __attribute__((packed)) {
    char nickname[MAX_NICK];
    char target[MAX_NICK];
    char data[MAX_MSG];
} TChatPayload;

#endif
