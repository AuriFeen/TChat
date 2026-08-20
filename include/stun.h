#ifndef STUN_H
#define STUN_H

#include <stddef.h>

int stun_discover_endpoint(const char *stun_host, int stun_port,
                           char *out_ip, size_t ip_len, int *out_port);

#endif
