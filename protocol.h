#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_MAGIC 0x54434841 // "TCHA" in ASCII
#define MAX_NICK    32
#define MAX_MSG     512

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

// Explicitly packed wire-frame header
typedef struct __attribute__((packed)) {
    uint32_t magic;      // System stream verification sentinel
    uint16_t type;       // PacketType identifier
    uint32_t length;     // Total size of the payload following this header
    uint32_t checksum;   // CRC32 verification token
} TWireHeader;

// Maximum abstract structural layout payload mapping
typedef struct __attribute__((packed)) {
    char nickname[MAX_NICK];
    char target[MAX_NICK];
    char data[MAX_MSG];
} TChatPayload;

#endif
