#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#include "protocol.h"
#include "network.h"

#define PORT 8080
#define MAX_FDS 128

typedef struct {
    int fd;
    char name[MAX_NICK];
    int active;
} UserSession;

UserSession users[MAX_FDS];

int main() {
    int server_fd;
    struct sockaddr_in address;
    struct pollfd fds[MAX_FDS];
    int nfds = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 10);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("TChat Server started on port %d...\n", PORT);

    while (1) {
        int res = poll(fds, nfds, -1); 
        if (res < 0) { perror("poll"); break; }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                
                // CASE 1: NEW CONNECTION
                if (fds[i].fd == server_fd) {
                    int new_socket = accept(server_fd, NULL, NULL);
                    if (nfds < MAX_FDS) {
                        fds[nfds].fd = new_socket;
                        fds[nfds].events = POLLIN;
                        nfds++;
                        printf("New user connected (FD %d). Total: %d\n", new_socket, nfds - 1);
                    }
                } 
                // CASE 2: DATA FROM EXISTING CLIENT
                else {
                    TChatPacket packet;
                    int bytes = recv(fds[i].fd, &packet, sizeof(TChatPacket), 0);
                    
                    if (bytes <= 0) {
                        // CLIENT DISCONNECTED
                        printf("User %s (FD %d) disconnected.\n", users[fds[i].fd].name, fds[i].fd);
                        memset(users[fds[i].fd].name, 0, MAX_NICK);
                        users[fds[i].fd].active = 0;

                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1]; 
                        nfds--;
                        i--; 
                    } else {
                        // HANDLE PACKETS
                        if (packet.type == TYPE_JOIN) {
                            strncpy(users[fds[i].fd].name, packet.nickname, MAX_NICK);
                            users[fds[i].fd].active = 1;
                            printf("User '%s' registered on FD %d\n", packet.nickname, fds[i].fd);
                        } 
                        else if (packet.type == TYPE_CHAT) {
                            // Attach identity to packet
                            strncpy(packet.nickname, users[fds[i].fd].name, MAX_NICK);
                            printf("Broadcasting from %s: %s\n", packet.nickname, packet.data);

                            for (int j = 1; j < nfds; j++) {
                                if (fds[j].fd != fds[i].fd) {
                                    send(fds[j].fd, &packet, sizeof(TChatPacket), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
