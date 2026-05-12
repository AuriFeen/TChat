#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "network.h"

// --- Thread function must be outside main ---
void* handle_server_messages(void* arg) {
    int sock = *(int*)arg;
    TChatPacket incoming;

    // This loop keeps the client listening for broadcasts
    while (recv(sock, &incoming, sizeof(TChatPacket), 0) > 0) {
        // \r clears the current line so the prompt "> " doesn't get messy
        printf("\r[Other]: %s\n> ", incoming.data);
        fflush(stdout); 
    }
    printf("\nLost connection to server.\n");
    exit(0);
}

int main() {
    printf("--- Welcome to TChat ---\n");
    printf("1. Connect to Server\n");
    printf("2. Join Existing Session\n");
    printf("3. Settings\n\nChoice: ");

    int choice;
    if (scanf("%d", &choice) != 1) return 1;

    if (choice == 1) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(8080) };
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Connection failed");
            return 1;
        }
        char name[MAX_NICK];
			printf("Enter nickname: ");
			fgets(name, MAX_NICK, stdin);
			name[strcspn(name, "\n")] = 0;

			TChatPacket join_p = { .type = TYPE_JOIN };
			strncpy(join_p.nickname, name, MAX_NICK);
			send(sock, &join_p, sizeof(join_p), 0);

        printf("Connected! Type /exit to quit.\n");

        // --- START THE LISTENER THREAD ---
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_server_messages, (void*)&sock);

        getchar(); // Flush the newline from scanf

        // --- CONTINUOUS INPUT LOOP ---
        char msg[MAX_MSG];
        while (1) {
            printf("> ");
            if (!fgets(msg, MAX_MSG, stdin)) break;

            // Strip the newline character from fgets
            msg[strcspn(msg, "\n")] = 0;

            if (strcmp(msg, "/exit") == 0) break;

            TChatPacket p = { .type = TYPE_CHAT };
            strncpy(p.data, msg, MAX_MSG);
            send(sock, &p, sizeof(p), 0);
        }

        close(sock);
    }

    return 0;
}
