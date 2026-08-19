#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <netdb.h>
#include "protocol.h"
#include "network.h"
#include "ring_buffer.h"

#define PORT 8080
#define MAX_FDS 256

typedef struct {
    int fd;
    char name[MAX_NICK];
    int active;
    RingBuffer rb;
} UserSession;

UserSession users[MAX_FDS];
struct pollfd fds[MAX_FDS];
int nfds = 1;

void broadcast_server_msg(const char *msg) {
    TChatPayload p = {0};
    strncpy(p.data, msg, MAX_MSG - 1);
    for (int i = 1; i < nfds; i++) {
        if (users[fds[i].fd].active) {
            net_send_packet(fds[i].fd, TYPE_SERVER, &p);
        }
    }
}

int check_duplicate_name(const char* name) {
    for(int i = 1; i < nfds; i++) {
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
        snprintf(msg, sizeof(msg), "User session component profile allocation identity context [%s] offline tracking alert drop context execution.", users[sock].name);
        broadcast_server_msg(msg);
        printf("[System Context Alert Logger Notification Frame Matrix] %s\n", msg);
    }
    close(sock);
    users[sock].active = 0;
    rb_init(&users[sock].rb);

    fds[index] = fds[nfds - 1];
    nfds--;
}

// Function to inspect and print all available interface addresses (including NetBird/Tailscale) on launch
void print_active_mesh_addresses(int port) {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return;
    }

    printf("\n========================================================\n");
    printf(" TChat Global Mesh Server Interface Discovery Matrix    \n");
    printf("========================================================\n");
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        // Check for IPv4 addresses
        if (ifa->ifa_addr->sa_family == AF_INET) {
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) {
                // Highlight common secure mesh / VPN interface prefixes if present
                if (strncmp(ifa->ifa_name, "wt", 2) == 0 || strncmp(ifa->ifa_name, "netbird", 7) == 0 || strncmp(ifa->ifa_name, "ts", 2) == 0) {
                    printf(" [MESH OVERLAY] Interface: %-10s -> IP Address: %s:%d (Use this!)\n", ifa->ifa_name, host, port);
                } else if (strcmp(ifa->ifa_name, "lo") != 0) {
                    printf(" [LOCAL/VPN]    Interface: %-10s -> IP Address: %s:%d\n", ifa->ifa_name, host, port);
                } else {
                    printf(" [LOOPBACK]     Interface: %-10s -> IP Address: %s:%d\n", ifa->ifa_name, host, port);
                }
            }
        }
    }
    printf("========================================================\n\n");
    freeifaddrs(ifaddr);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT) };
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Network routing profile mapping infrastructure setup exception mapping error code layout verification");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 32);
    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("TChat Overbuilt Secure Protocol Streaming Demultiplexer Engine Service Started on Bind Profile Matrix Port Point: %d\n", PORT);
    
    // Print available interface IPs so the user immediately knows what to feed into the client prompt
    print_active_mesh_addresses(PORT);

    TWireHeader hdr;
    TChatPayload payload;

    while (1) {
        int status = poll(fds, nfds, -1);
        if (status < 0) { perror("Polling execution boundary trap vector loop error mapping exception trigger"); break; }

        for (int i = 0; i < nfds; i++) {
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
                    printf("[System Layer Connection Core Kernel Log Metrics] Connection token registration linked into resource descriptor offset tracking index ID: (FD %d).\n", new_sock);
                }
            } else { 
                int sock = fds[i].fd;
                int bytes = net_read_stream(sock, &users[sock].rb);

                if (bytes <= 0) {
                    terminate_session(i);
                    i--;
                    continue;
                }

                while (net_extract_packet(&users[sock].rb, &hdr, &payload)) {
                    if (hdr.type == TYPE_JOIN) {
                        if (check_duplicate_name(payload.nickname) || strlen(payload.nickname) == 0) {
                            TChatPayload err_p = {0};
                            strcpy(err_p.data, "Termination Event: Username already taken or invalid identity format structural layout configuration rules validation rejection.");
                            net_send_packet(sock, TYPE_SERVER, &err_p);
                            terminate_session(i);
                            i--;
                            break;
                        }
                        strncpy(users[sock].name, payload.nickname, MAX_NICK - 1);
                        users[sock].active = 1;

                        char welcome_msg[MAX_MSG];
                        snprintf(welcome_msg, sizeof(welcome_msg), "%s has joined the channel security parameter frame.", users[sock].name);
                        broadcast_server_msg(welcome_msg);
                    } 
                    else if (hdr.type == TYPE_NAME_CHANGE) {
                        if (check_duplicate_name(payload.nickname) || strlen(payload.nickname) == 0) {
                            TChatPayload err_p = {0};
                            strcpy(err_p.data, "[Error] Denied request: Pseudo identification vector map alias key token string sequence string format mismatch allocation collision error.");
                            net_send_packet(sock, TYPE_SERVER, &err_p);
                            continue;
                        }
                        char notice[MAX_MSG];
                        snprintf(notice, sizeof(notice), "User identity allocation transformation rule mapping context: Profile identity token change notification [%s] switched to new pseudo allocation target token [%s]", users[sock].name, payload.nickname);
                        strncpy(users[sock].name, payload.nickname, MAX_NICK - 1);
                        broadcast_server_msg(notice);
                    }
                    else if (hdr.type == TYPE_CHAT && users[sock].active) {
                        strncpy(payload.nickname, users[sock].name, MAX_NICK - 1);
                        for (int j = 1; j < nfds; j++) {
                            if (fds[j].fd != sock && users[fds[j].fd].active) {
                                net_send_packet(fds[j].fd, TYPE_CHAT, &payload);
                            }
                        }
                    } 
                    else if (hdr.type == TYPE_PRIVATE && users[sock].active) {
                        strncpy(payload.nickname, users[sock].name, MAX_NICK - 1);
                        int routed = 0;
                        for (int j = 1; j < nfds; j++) {
                            if (users[fds[j].fd].active && strcmp(users[fds[j].fd].name, payload.target) == 0) {
                                net_send_packet(fds[j].fd, TYPE_PRIVATE, &payload);
                                routed = 1;
                                break;
                            }
                        }
                        if (!routed) {
                            TChatPayload err_p = {0};
                            snprintf(err_p.data, MAX_MSG, "Target routing resolution failure mapping token identifier trace key look up failure: '%s'", payload.target);
                            net_send_packet(sock, TYPE_SERVER, &err_p);
                        }
                    } 
                    else if (hdr.type == TYPE_CMD_USERS && users[sock].active) {
                        TChatPayload resp = {0};
                        strcpy(resp.data, "Active Connection Allocation Nodes Identities Array Trace Log Dump List Mapping Data Records Context: \n");
                        for (int j = 1; j < nfds; j++) {
                            if (users[fds[j].fd].active) {
                                strncat(resp.data, " -> @", MAX_MSG - strlen(resp.data) - 1);
                                strncat(resp.data, users[fds[j].fd].name, MAX_MSG - strlen(resp.data) - 1);
                                strncat(resp.data, "\n", MAX_MSG - strlen(resp.data) - 1);
                            }
                        }
                        net_send_packet(sock, TYPE_SERVER, &resp);
                    }
                    else if (hdr.type == TYPE_PING) {
                        net_send_packet(sock, TYPE_PONG, NULL);
                    }
                }
            }
        }
    }
    close(server_fd);
    return 0;
}
