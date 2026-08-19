#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <poll.h>

#include "protocol.h"
#include "network.h"
#include "ring_buffer.h"

#define MAX_FDS 256

typedef struct {
    int fd;
    char name[MAX_NICK];
    int active;
    RingBuffer rb;
} UserSession;

UserSession users[MAX_FDS];
struct pollfd fds[MAX_FDS];
int nfds = 2; // Index 0: server_fd, Index 1: STDIN command panel, 2+: clients
int current_port = 8080;

void broadcast_server_msg(const char *msg) {
    TChatPayload p = {0};
    strncpy(p.data, msg, MAX_MSG - 1);
    for (int i = 2; i < nfds; i++) {
        if (users[fds[i].fd].active) {
            net_send_packet(fds[i].fd, TYPE_SERVER, &p);
        }
    }
}

int check_duplicate_name(const char* name) {
    for(int i = 2; i < nfds; i++) {
        if(users[fds[i].fd].active && strcmp(users[fds[i].fd].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

void terminate_session(int index) {
    int sock = fds[index].fd;
    if (users[sock].active) {
        char msg[MAX_MSG];
        snprintf(msg, sizeof(msg), "User session [%s] dropped by daemon framework.", users[sock].name);
        broadcast_server_msg(msg);
        printf("\n[Daemon Event] %s\n", msg);
    }
    close(sock);
    users[sock].active = 0;
    rb_init(&users[sock].rb);

    fds[index] = fds[nfds - 1];
    nfds--;
}

void print_active_mesh_addresses(int port) {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) return;

    printf("\n========================================================\n");
    printf(" TChat Active Interface & Mesh Endpoint Routing Matrix  \n");
    printf("========================================================\n");
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
        int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (s == 0) {
            if (strncmp(ifa->ifa_name, "wt", 2) == 0 || strncmp(ifa->ifa_name, "netbird", 7) == 0 || strncmp(ifa->ifa_name, "ts", 2) == 0) {
                printf(" [MESH OVERLAY] Interface: %-10s -> Endpoint: %s:%d\n", ifa->ifa_name, host, port);
            } else if (strcmp(ifa->ifa_name, "lo") != 0) {
                printf(" [LOCAL/VPN]    Interface: %-10s -> Endpoint: %s:%d\n", ifa->ifa_name, host, port);
            }
        }
    }
    printf("========================================================\n\n");
    freeifaddrs(ifaddr);
}

// Full-Featured Asynchronous Server Command Line Interpreter
void handle_server_console_input(int *server_fd) {
    char cmd[256];
    if (!fgets(cmd, sizeof(cmd), stdin)) return;
    cmd[strcspn(cmd, "\n")] = 0;

    // Parse commands natively
    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "/status") == 0) {
        int active_count = 0;
        for (int i = 2; i < nfds; i++) if (users[fds[i].fd].active) active_count++;
        printf("\n=== TChat Daemon Operational Status ===\n");
        printf(" Bound Port      : %d\n", current_port);
        printf(" Active Clients  : %d / %d max sockets\n", active_count, MAX_FDS - 2);
        printf(" Total Poll FDs  : %d\n", nfds);
        printf("=======================================\n");
    } 
    else if (strcmp(cmd, "endpoints") == 0 || strcmp(cmd, "/endpoints") == 0) {
        print_active_mesh_addresses(current_port);
    }
    else if (strncmp(cmd, "broadcast ", 10) == 0) {
        char *msg = cmd + 10;
        char broadcast_buf[MAX_MSG];
        snprintf(broadcast_buf, sizeof(broadcast_buf), "[Admin Broadcast] %s", msg);
        broadcast_server_msg(broadcast_buf);
        printf("[Console] Global broadcast dispatched successfully.\n");
    }
    else if (strncmp(cmd, "kick ", 5) == 0) {
        char *target_name = cmd + 5;
        int found = 0;
        for (int i = 2; i < nfds; i++) {
            if (users[fds[i].fd].active && strcmp(users[fds[i].fd].name, target_name) == 0) {
                TChatPayload err_p = {0};
                strcpy(err_p.data, "Server Administrator terminated your session mapping profile.");
                net_send_packet(fds[i].fd, TYPE_SERVER, &err_p);
                terminate_session(i);
                found = 1;
                printf("[Console] User '%s' forcefully evicted from network node.\n", target_name);
                break;
            }
        }
        if (!found) {
            printf("[Console Error] Target user identity '%s' not located.\n", target_name);
        }
    }
    else if (strcmp(cmd, "users") == 0 || strcmp(cmd, "/users") == 0) {
        printf("\n--- Connected Client Registry --- \n");
        int active_found = 0;
        for (int i = 2; i < nfds; i++) {
            if (users[fds[i].fd].active) {
                printf(" [%d] User Handle: @%s (FD: %d)\n", i, users[fds[i].fd].name, fds[i].fd);
                active_found = 1;
            }
        }
        if (!active_found) printf(" No active chat sessions registered.\n");
        printf("---------------------------------\n");
    }
    else if (strncmp(cmd, "port ", 5) == 0) {
        int new_port = atoi(cmd + 5);
        if (new_port > 0 && new_port < 65536) {
            close(*server_fd);
            int new_server_fd = socket(AF_INET, SOCK_STREAM, 0);
            int opt = 1;
            setsockopt(new_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(new_port) };
            if (bind(new_server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("[Console Error] Failed to bind new port");
                // Restore previous socket binding behavior or handle gracefully
            } else {
                listen(new_server_fd, 32);
                current_port = new_port;
                *server_fd = new_server_fd;
                fds[0].fd = new_server_fd;
                printf("[Console] Port binding dynamically migrated to port: %d\n", current_port);
                print_active_mesh_addresses(current_port);
            }
        } else {
            printf("[Console Error] Invalid port specification range.\n");
        }
    }
    else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "/help") == 0) {
        printf("\n============= TChat Server Daemon Command Directory =============\n");
        printf(" status              - Display engine metrics and client loads\n");
        printf(" endpoints           - Refresh and print active overlay/mesh IPs\n");
        printf(" users               - List all online user nodes and socket IDs\n");
        printf(" broadcast <msg>     - Transmit an admin message to all clients\n");
        printf(" kick <username>     - Evict a specific user from the server\n");
        printf(" port <number>       - Dynamically rebind server to a new port\n");
        printf(" help                - Show this command reference menu\n");
        printf("===============================================================\n");
    }
    else if (strlen(cmd) > 0) {
        printf("[Console] Unknown command sequence '%s'. Type 'help' for options.\n", cmd);
    }
    printf("tchat-admin> ");
    fflush(stdout);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(current_port) };
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Server socket bind allocation failure");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 32);
    memset(fds, 0, sizeof(fds));

    // Register Server Socket
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    // Register Asynchronous Console Input (Stdin) at Index 1
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    printf("TChat Core Engine Service Initialized Cleanly.\n");
    print_active_mesh_addresses(current_port);
    printf("Interactive command panel active. Type 'help' for available commands.\n");
    printf("tchat-admin> ");
    fflush(stdout);

    TWireHeader hdr;
    TChatPayload payload;

    while (1) {
        int status = poll(fds, nfds, -1);
        if (status < 0) { perror("Poll loop fault error"); break; }

        // Check for Async Console Input Activity on Stdin
        if (fds[1].revents & POLLIN) {
            handle_server_console_input(&server_fd);
        }

        // Check Network Sockets
        for (int i = 0; i < nfds; i++) {
            if (i == 1) continue; // Skip stdin descriptor entry
            if (!(fds[i].revents & POLLIN)) continue;

            if (fds[i].fd == server_fd) { 
                int new_sock = accept(server_fd, NULL, NULL);
                if (new_sock >= MAX_FDS || nfds >= MAX_FDS) {
                    close(new_sock);
                } else {
                    fds[nfds].fd = new_sock;
                    fds[nfds].events = POLLIN;
                    fds[nfds].revents = 0;
                    
                    users[new_sock].fd = new_sock;
                    users[new_sock].active = 0;
                    memset(users[new_sock].name, 0, MAX_NICK);
                    rb_init(&users[new_sock].rb);
                    nfds++;
                    printf("\n[Network Event] Client socket linked (FD %d).\n", new_sock);
                    printf("tchat-admin> ");
                    fflush(stdout);
                }
            } else { 
                int sock = fds[i].fd;
                int bytes = net_read_stream(sock, &users[sock].rb);

                if (bytes <= 0) {
                    terminate_session(i);
                    i--;
                    printf("tchat-admin> ");
                    fflush(stdout);
                    continue;
                }

                while (net_extract_packet(&users[sock].rb, &hdr, &payload)) {
                    if (hdr.type == TYPE_JOIN) {
                        if (check_duplicate_name(payload.nickname) || strlen(payload.nickname) == 0) {
                            TChatPayload err_p = {0};
                            strcpy(err_p.data, "Error: Username token already claimed or formatted incorrectly.");
                            net_send_packet(sock, TYPE_SERVER, &err_p);
                            terminate_session(i);
                            i--;
                            break;
                        }
                        strncpy(users[sock].name, payload.nickname, MAX_NICK - 1);
                        users[sock].active = 1;

                        char welcome_msg[MAX_MSG];
                        snprintf(welcome_msg, sizeof(welcome_msg), "%s entered the secure channel.", users[sock].name);
                        broadcast_server_msg(welcome_msg);
                    } 
                    else if (hdr.type == TYPE_CHAT && users[sock].active) {
                        strncpy(payload.nickname, users[sock].name, MAX_NICK - 1);
                        for (int j = 2; j < nfds; j++) {
                            if (fds[j].fd != sock && users[fds[j].fd].active) {
                                net_send_packet(fds[j].fd, TYPE_CHAT, &payload);
                            }
                        }
                    }
                    else if (hdr.type == TYPE_PRIVATE && users[sock].active) {
                        strncpy(payload.nickname, users[sock].name, MAX_NICK - 1);
                        int routed = 0;
                        for (int j = 2; j < nfds; j++) {
                            if (users[fds[j].fd].active && strcmp(users[fds[j].fd].name, payload.target) == 0) {
                                net_send_packet(fds[j].fd, TYPE_PRIVATE, &payload);
                                routed = 1;
                                break;
                            }
                        }
                        if (!routed) {
                            TChatPayload err_p = {0};
                            snprintf(err_p.data, MAX_MSG, "Routing failure: target user '@%s' is offline.", payload.target);
                            net_send_packet(sock, TYPE_SERVER, &err_p);
                        }
                    }
                    else if (hdr.type == TYPE_CMD_USERS && users[sock].active) {
                        TChatPayload resp = {0};
                        strcpy(resp.data, "Active Channel Nodes Registry:\n");
                        for (int j = 2; j < nfds; j++) {
                            if (users[fds[j].fd].active) {
                                strncat(resp.data, " -> @", MAX_MSG - strlen(resp.data) - 1);
                                strncat(resp.data, users[fds[j].fd].name, MAX_MSG - strlen(resp.data) - 1);
                                strncat(resp.data, "\n", MAX_MSG - strlen(resp.data) - 1);
                            }
                        }
                        net_send_packet(sock, TYPE_SERVER, &resp);
                    }
                }
            }
        }
    }
    close(server_fd);
    return 0;
}
