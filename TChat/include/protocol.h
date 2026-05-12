#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_NICK 16
#define MAX_MSG  512

typedef enum {
    TYPE_JOIN,
    TYPE_CHAT,
    TYPE_PRIVATE,
    TYPE_LEAVE
} PacketType;

typedef struct {
    uint8_t type;
    char nickname[MAX_NICK];
    char data[MAX_MSG];
} TChatPacket;

#endif
