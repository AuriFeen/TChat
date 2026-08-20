#ifndef CONFIG_H
#define CONFIG_H

#define CONF_MAX_LINE 512

typedef struct {
    char alias[64];
    int  port;
    char stun_host[128];
    int  stun_port;
    int  upnp;
} ServerConfig;

typedef struct {
    char name[64];
    char host[128];
    int  port;
} PeerEntry;

typedef struct {
    PeerEntry peers[64];
    int count;
} ClientConfig;

int config_load_server(const char *path, ServerConfig *out);
int config_load_client(const char *path, ClientConfig *out);
int config_resolve_peer(const ClientConfig *cfg, const char *name,
                        char *out_host, int *out_port);

#endif
