#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h> // Added for real-time timestamp operations

#include "protocol.h"
#include "network.h"
#include "ring_buffer.h"

// TUI State Definitions
typedef enum { STATE_CHAT, STATE_SETTINGS } TuiMode;
TuiMode current_mode = STATE_CHAT;

char current_input[MAX_MSG] = {0};
size_t input_len = 0;
pthread_mutex_t tui_lock = PTHREAD_MUTEX_INITIALIZER;

// Runtime Configuration Settings Toggle Indicators
int setting_timestamps = 1;
int setting_notifications = 0;
int selected_menu_item = 0;
#define TOTAL_SETTINGS 3

int global_sock = -1;
char my_nickname[MAX_NICK] = {0}; // Track nickname locally for echo printing

#define C_RST   "\033[0m"
#define C_PRMPT "\033[1;32m"
#define C_SRV   "\033[1;33m"
#define C_PRIV  "\033[1;35m"
#define C_NICK  "\033[1;36m"
#define C_HI    "\033[1;7m" 

struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Helper function to dynamically generate a clean timestamp string
void get_current_timestamp(char *out_str, size_t max_len) {
    if (!setting_timestamps) {
        out_str[0] = '\0';
        return;
    }
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(out_str, max_len, "[%H:%M:%S] ", timeinfo);
}

void redraw_settings_menu() {
    printf("\r\033[K=== CONFIGURATION DASHBOARD ===\n");
    for (int i = 0; i < TOTAL_SETTINGS; i++) {
        printf("\033[K");
        if (i == selected_menu_item) printf("%s-> ", C_HI);
        else printf("   ");

        if (i == 0) printf("Display Messages Timestamps: [%s]%s\n", setting_timestamps ? "ON" : "OFF", C_RST);
        else if (i == 1) printf("Enable Sound Audio Alert Ping: [%s]%s\n", setting_notifications ? "ON" : "OFF", C_RST);
        else if (i == 2) printf("Exit Configuration Dashboard Menu%s\n", C_RST);
    }
    printf("\033[K(Navigate via Up/Down Arrows, Change/Execute with Space/Enter)\n");
    printf("\033[%dA", TOTAL_SETTINGS + 2); 
    fflush(stdout);
}

void tui_print(const char *formatted_msg) {
    pthread_mutex_lock(&tui_lock);
    if (current_mode == STATE_SETTINGS) {
        printf("\r\033[%dB\n%s\033[%dA", TOTAL_SETTINGS + 2, formatted_msg, TOTAL_SETTINGS + 3);
    } else {
        printf("\r\033[K%s\n", formatted_msg);
        printf(C_PRMPT "> " C_RST "%s", current_input);
    }
    fflush(stdout);
    pthread_mutex_unlock(&tui_lock);
}

void get_connection_info(int sock) {
    struct sockaddr_in local_addr, peer_addr;
    socklen_t local_len = sizeof(local_addr);
    socklen_t peer_len = sizeof(peer_addr);
    char local_ip[INET_ADDRSTRLEN], peer_ip[INET_ADDRSTRLEN];

    getsockname(sock, (struct sockaddr*)&local_addr, &local_len);
    getpeername(sock, (struct sockaddr*)&peer_addr, &peer_len);

    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));
    inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, sizeof(peer_ip));

    char output[256];
    snprintf(output, sizeof(output), C_SRV "[Network Profile]\n | Link Bound Point: %s:%d\n | Remote Endpoint Target: %s:%d" C_RST,
             local_ip, ntohs(local_addr.sin_port), peer_ip, ntohs(peer_addr.sin_port));
    tui_print(output);
}

void* handle_server_messages(void* arg) {
    int sock = *(int*)arg;
    RingBuffer rb;
    rb_init(&rb);

    TWireHeader hdr;
    TChatPayload payload;

    while (1) {
        if (net_read_stream(sock, &rb) <= 0) break;

        while (net_extract_packet(&rb, &hdr, &payload)) {
            char buffer[1024];
            char time_str[32] = {0};
            get_current_timestamp(time_str, sizeof(time_str));

            if (hdr.type == TYPE_CHAT) {
                snprintf(buffer, sizeof(buffer), "%s" C_NICK "[%s]" C_RST ": %s", time_str, payload.nickname, payload.data);
            } else if (hdr.type == TYPE_SERVER) {
                snprintf(buffer, sizeof(buffer), "%s" C_SRV "[System Header]" C_RST " %s", time_str, payload.data);
            } else if (hdr.type == TYPE_PRIVATE) {
                snprintf(buffer, sizeof(buffer), "%s" C_PRIV "[PM From %s]" C_RST ": %s", time_str, payload.nickname, payload.data);
            } else if (hdr.type == TYPE_PONG) {
                continue; 
            }
            
            if (setting_notifications && hdr.type == TYPE_PRIVATE) {
                printf("\a"); 
            }
            tui_print(buffer);
        }
    }
    tui_print(C_SRV "[Critical Alert System Error] Remote operational core target lost connection parameters." C_RST);
    disable_raw_mode();
    exit(EXIT_FAILURE);
}

int main() {
    printf("--- Welcome to TChat Overbuilt Platform Framework ---\n");
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(8080) };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Socket allocation subsystem binding tracking exception mapping failure");
        return 1;
    }
    global_sock = sock;

    printf("Enter system registration identity pseudonym token: ");
    if(!fgets(my_nickname, MAX_NICK, stdin)) return 0;
    my_nickname[strcspn(my_nickname, "\n")] = 0;

    TChatPayload join_p = {0};
    strncpy(join_p.nickname, my_nickname, MAX_NICK - 1);
    net_send_packet(sock, TYPE_JOIN, &join_p);

    printf("Dynamic binding active context configuration loaded. Engine initialized. Command triggers active.\n");
    
    enable_raw_mode();
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_server_messages, (void*)&sock);

    printf(C_PRMPT "> " C_RST);
    fflush(stdout);

    while (1) {
        uint8_t c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        if (current_mode == STATE_SETTINGS) {
            if (c == '\033') { 
                uint8_t seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        if (seq[1] == 'A') { 
                            selected_menu_item = (selected_menu_item - 1 + TOTAL_SETTINGS) % TOTAL_SETTINGS;
                            redraw_settings_menu();
                        } else if (seq[1] == 'B') { 
                            selected_menu_item = (selected_menu_item + 1) % TOTAL_SETTINGS;
                            redraw_settings_menu();
                        }
                    }
                }
            } else if (c == ' ' || c == '\n' || c == '\r') {
                if (selected_menu_item == 0) {
                    setting_timestamps = !setting_timestamps;
                    redraw_settings_menu();
                } else if (selected_menu_item == 1) {
                    setting_notifications = !setting_notifications;
                    redraw_settings_menu();
                } else if (selected_menu_item == 2) {
                    pthread_mutex_lock(&tui_lock);
                    printf("\r\033[%dB", TOTAL_SETTINGS + 2);
                    printf("\r\033[K=== CHAT INTERACTION RESTORED ===\n");
                    current_mode = STATE_CHAT;
                    memset(current_input, 0, sizeof(current_input));
                    input_len = 0;
                    printf(C_PRMPT "> " C_RST);
                    fflush(stdout);
                    pthread_mutex_unlock(&tui_lock);
                }
            }
        } else {
            if (c == '\n' || c == '\r') {
                if (input_len == 0) continue;
                
                printf("\r\033[K"); 
                
                if (strcmp(current_input, "/exit") == 0) {
                    break;
                } else if (strcmp(current_input, "/port") == 0) {
                    get_connection_info(sock);
                } else if (strcmp(current_input, "/settings") == 0) {
                    pthread_mutex_lock(&tui_lock);
                    current_mode = STATE_SETTINGS;
                    selected_menu_item = 0;
                    printf("\n");
                    redraw_settings_menu();
                    pthread_mutex_unlock(&tui_lock);
                    continue;
                } else if (strcmp(current_input, "/users") == 0) {
                    net_send_packet(sock, TYPE_CMD_USERS, NULL);
                } else if (strncmp(current_input, "/name ", 6) == 0) {
                    TChatPayload p = {0};
                    strncpy(p.nickname, current_input + 6, MAX_NICK - 1);
                    // Update local copy of nickname immediately for local echos
                    strncpy(my_nickname, current_input + 6, MAX_NICK - 1);
                    net_send_packet(sock, TYPE_NAME_CHANGE, &p);
                } else if (strncmp(current_input, "/msg ", 5) == 0) {
                    char *target = strtok(current_input + 5, " ");
                    char *msg = strtok(NULL, "");
                    if (target && msg) {
                        TChatPayload p = {0};
                        strncpy(p.target, target, MAX_NICK - 1);
                        strncpy(p.data, msg, MAX_MSG - 1);
                        net_send_packet(sock, TYPE_PRIVATE, &p);

                        char local_echo[1024];
                        snprintf(local_echo, sizeof(local_echo), C_PRIV "[PM to %s]" C_RST ": %s", target, msg);
                        printf("%s\n", local_echo);
                    } else {
                        printf(C_SRV "[System] Syntax error format layout structure validation constraint violation matching /msg <user> <text>\n" C_RST);
                    }
                } else {
                    // Standard Chat Message Broadcast Execution
                    TChatPayload p = {0};
                    strncpy(p.data, current_input, MAX_MSG - 1);
                    net_send_packet(sock, TYPE_CHAT, &p);

                    // FIXED: Generate a clean, real-time local echo string 
                    // so the sender can see their own broadcast messages.
                    char time_str[32] = {0};
                    get_current_timestamp(time_str, sizeof(time_str));
                    printf("%s" C_NICK "[%s]" C_RST ": %s\n", time_str, my_nickname, current_input);
                }

                pthread_mutex_lock(&tui_lock);
                memset(current_input, 0, MAX_MSG);
                input_len = 0;
                printf(C_PRMPT "> " C_RST);
                fflush(stdout);
                pthread_mutex_unlock(&tui_lock);
            } else if (c == 127 || c == 8) { 
                if (input_len > 0) {
                    input_len--;
                    current_input[input_len] = '\0';
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (c >= 32 && c < 127 && input_len < MAX_MSG - 1) {
                current_input[input_len++] = c;
                putchar(c);
                fflush(stdout);
            }
        }
    }

    disable_raw_mode();
    close(sock);
    return 0;
}
