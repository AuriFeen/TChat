#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"
#include "ring_buffer.h"

typedef struct IrohEndpoint IrohEndpoint;
typedef struct IrohConnection IrohConnection;

uint32_t net_crc32(const uint8_t *data, size_t length);

/* listen_port: 0 for client (no bind), >0 for server */
IrohEndpoint* iroh_net_init_node(int listen_port);
char* iroh_net_get_node_id_str(IrohEndpoint *endpoint);
IrohConnection* iroh_net_accept(IrohEndpoint *endpoint);
IrohConnection* iroh_net_connect(IrohEndpoint *endpoint, const char *node_id_str);

int iroh_net_read_stream(IrohConnection *conn, RingBuffer *rb);
int iroh_net_extract_packet(RingBuffer *rb, TWireHeader *out_hdr, TChatPayload *out_payload);
int iroh_net_send_packet(IrohConnection *conn, uint16_t type, const TChatPayload *payload);
void iroh_net_close(IrohConnection *conn);

#endif
