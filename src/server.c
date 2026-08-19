#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "protocol.h"
#include "network.h"
#include "ring_buffer.h"

typedef struct {
    IrohConnection *conn;
    char name[MAX_NICK];
    int active;
    RingBuffer rb;
} UserSession;

UserSession users[MAX_FDS];
int client_count = 0;
pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;

void broadcast_server_msg(const char *msg) {
    TChatPayload p = {0};
    strncpy(p.data, msg, MAX_MSG - 1);
    pthread_mutex_lock(&server_lock);
    for (int i = 0; i < MAX_FDS; i++) {
        if (users[i].active) {
            iroh_net_send_packet(users[i].conn, TYPE_SERVER, &p);
        }
    }
    pthread_mutex_unlock(&server_lock);
}

int check_duplicate_name(const char* name) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (users[i].active && strcmp(users[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

void terminate_session(int index) {
    pthread_mutex_lock(&server_lock);
    if (users[index].active) {
        char msg[MAX_MSG];
        snprintf(msg, sizeof(msg), "User session [%s] dropped by daemon framework.", users[index].name);
        broadcast_server_msg(msg);
        printf("\n[Daemon Event] %s\n", msg);
    }
    iroh_net_close(users[index].conn);
    users[index].active = 0;
    users[index].conn = NULL;
    rb_init(&users[index].rb);
    client_count--;
    pthread_mutex_unlock(&server_lock);
}

void* handle_client_connection(void* arg) {
    int index = *(int*)arg;
    free(arg);

    IrohConnection *conn = users[index].conn;
    TWireHeader hdr;
    TChatPayload payload;

    while (1) {
        int bytes = iroh_net_read_stream(conn, &users[index].rb);
        if (bytes <= 0) {
            terminate_session(index);
            break;
        }

        while (iroh_net_extract_packet(&users[index].rb, &hdr, &payload)) {
            if (hdr.type == TYPE_JOIN) {
                if (check_duplicate_name(payload.nickname) || strlen(payload.nickname) == 0) {
                    TChatPayload err_p = {0};
                    strcpy(err_p.data, "Error: Username token already claimed or formatted incorrectly.");
                    iroh_net_send_packet(conn, TYPE_SERVER, &err_p);
                    terminate_session(index);
                    return NULL;
                }
                strncpy(users[index].name, payload.nickname, MAX_NICK - 1);
                users[index].active = 1;

                char welcome_msg[MAX_MSG];
                snprintf(welcome_msg, sizeof(welcome_msg), "%s entered the secure channel.", users[index].name);
                broadcast_server_msg(welcome_msg);
            } 
            else if (hdr.type == TYPE_CHAT && users[index].active) {
                strncpy(payload.nickname, users[index].name, MAX_NICK - 1);
                pthread_mutex_lock(&server_lock);
                for (int j = 0; j < MAX_FDS; j++) {
                    if (users[j].active && users[j].conn != conn) {
                        iroh_net_send_packet(users[j].conn, TYPE_CHAT, &payload);
                    }
                }
                pthread_mutex_unlock(&server_lock);
            }
            else if (hdr.type == TYPE_PRIVATE && users[index].active) {
                strncpy(payload.nickname, users[index].name, MAX_NICK - 1);
                int routed = 0;
                pthread_mutex_lock(&server_lock);
                for (int j = 0; j < MAX_FDS; j++) {
                    if (users[j].active && strcmp(users[j].name, payload.target) == 0) {
                        iroh_net_send_packet(users[j].conn, TYPE_PRIVATE, &payload);
                        routed = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&server_lock);

                if (!routed) {
                    TChatPayload err_p = {0};
                    snprintf(err_p.data, MAX_MSG, "Routing failure: target user '@%s' is offline.", payload.target);
                    iroh_net_send_packet(conn, TYPE_SERVER, &err_p);
                }
            }
            else if (hdr.type == TYPE_CMD_USERS && users[index].active) {
                TChatPayload resp = {0};
                strcpy(resp.data, "Active Channel Nodes Registry:\n");
                pthread_mutex_lock(&server_lock);
                for (int j = 0; j < MAX_FDS; j++) {
                    if (users[j].active) {
                        strncat(resp.data, " -> @", MAX_MSG - strlen(resp.data) - 1);
                        strncat(resp.data, users[j].name, MAX_MSG - strlen(resp.data) - 1);
                        strncat(resp.data, "\n", MAX_MSG - strlen(resp.data) - 1);
                    }
                }
                pthread_mutex_unlock(&server_lock);
                iroh_net_send_packet(conn, TYPE_SERVER, &resp);
            }
        }
    }
    return NULL;
}

int main() {
    memset(users, 0, sizeof(users));

    IrohEndpoint *endpoint = iroh_net_init_node(TCHAT_ALPN);
    if (!endpoint) {
        fprintf(stderr, "Failed to initialize Iroh P2P Endpoint node.\n");
        return 1;
    }

    char *node_id = iroh_net_get_node_id_str(endpoint);
    printf("========================================================\n");
    printf(" TChat P2P Mesh Server Engine Initialized               \n");
    printf(" Iroh Node ID: %s\n", node_id ? node_id : "UNKNOWN");
    printf("========================================================\n");
    if (node_id) free(node_id);

    while (1) {
        IrohConnection *conn = iroh_net_accept(endpoint);
        if (!conn) continue;

        pthread_mutex_lock(&server_lock);
        int slot = -1;
        for (int i = 0; i < MAX_FDS; i++) {
            if (!users[i].active && users[i].conn == NULL) {
                slot = i;
                break;
            }
        }

        if (slot == -1 || client_count >= MAX_FDS) {
            pthread_mutex_unlock(&server_lock);
            iroh_net_close(conn);
            continue;
        }

        users[slot].conn = conn;
        users[slot].active = 0;
        rb_init(&users[slot].rb);
        client_count++;

        int *arg = malloc(sizeof(int));
        *arg = slot;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client_connection, arg);
        pthread_detach(tid);
        pthread_mutex_unlock(&server_lock);

        printf("\n[Network Event] New peer connection established (Slot %d).\n", slot);
    }

    return 0;
}
