#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static char* trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

int config_load_server(const char *path, ServerConfig *out) {
    memset(out, 0, sizeof(*out));
    out->port = 7777;
    strncpy(out->stun_host, "stun.l.google.com", sizeof(out->stun_host)-1);
    out->stun_port = 19302;
    out->upnp = 1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[CONF_MAX_LINE];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '\0' || *p == '#' || *p == ';') continue;

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) { *end = '\0'; strncpy(section, p+1, sizeof(section)-1); }
            continue;
        }
        if (strcmp(section, "server") != 0) continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq+1);

        if (strcmp(key, "alias") == 0) {
            strncpy(out->alias, val, sizeof(out->alias)-1);
        } else if (strcmp(key, "port") == 0) {
            out->port = atoi(val);
        } else if (strcmp(key, "stun_host") == 0) {
            strncpy(out->stun_host, val, sizeof(out->stun_host)-1);
        } else if (strcmp(key, "stun_port") == 0) {
            out->stun_port = atoi(val);
        } else if (strcmp(key, "upnp") == 0) {
            out->upnp = (strcmp(val,"true")==0 || strcmp(val,"1")==0 || strcmp(val,"yes")==0);
        }
    }
    fclose(f);
    return 0;
}

int config_load_client(const char *path, ClientConfig *out) {
    memset(out, 0, sizeof(*out));
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[CONF_MAX_LINE];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '\0' || *p == '#' || *p == ';') continue;

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) { *end = '\0'; strncpy(section, p+1, sizeof(section)-1); }
            continue;
        }
        if (strcmp(section, "peers") != 0) continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *name = trim(p);
        char *hostport = trim(eq+1);

        if (out->count >= 64) continue;
        PeerEntry *pe = &out->peers[out->count];
        strncpy(pe->name, name, sizeof(pe->name)-1);

        char *colon = strrchr(hostport, ':');
        if (colon) {
            *colon = '\0';
            strncpy(pe->host, hostport, sizeof(pe->host)-1);
            pe->port = atoi(colon+1);
        } else {
            strncpy(pe->host, hostport, sizeof(pe->host)-1);
            pe->port = 7777;
        }
        out->count++;
    }
    fclose(f);
    return 0;
}

int config_resolve_peer(const ClientConfig *cfg, const char *name,
                        char *out_host, int *out_port) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->peers[i].name, name) == 0) {
            strncpy(out_host, cfg->peers[i].host, 127);
            out_host[127] = '\0';
            *out_port = cfg->peers[i].port;
            return 0;
        }
    }
    return -1;
}
