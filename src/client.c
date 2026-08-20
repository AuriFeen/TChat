#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <time.h>

#include "protocol.h"
#include "network.h"
#include "ring_buffer.h"
#include "config.h"

typedef enum { STATE_CHAT, STATE_SETTINGS } TuiMode;
TuiMode current_mode = STATE_CHAT;

char current_input[MAX_MSG] = {0};
size_t input_len = 0;
pthread_mutex_t tui_lock = PTHREAD_MUTEX_INITIALIZER;

int setting_timestamps = 1;
int setting_notifications = 0;
int selected_menu_item = 0;
#define TOTAL_SETTINGS 3

IrohConnection *global_conn = NULL;
char my_nickname[MAX_NICK] = {0};

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
        else printf("    ");

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

void* handle_server_messages(void* arg) {
    IrohConnection *conn = (IrohConnection*)arg;
    RingBuffer rb;
    rb_init(&rb);

    TWireHeader hdr;
    TChatPayload payload;

    while (1) {
        if (iroh_net_read_stream(conn, &rb) <= 0) break;

        while (iroh_net_extract_packet(&rb, &hdr, &payload)) {
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
    tui_print(C_SRV "[Critical Alert] Lost connection to remote mesh node." C_RST);
    disable_raw_mode();
    exit(EXIT_FAILURE);
}

int main() {
    ClientConfig client_cfg;
    int has_cfg = (config_load_client("tchat_client.conf", &client_cfg) == 0);

    printf("--- Welcome to TChat Global Overlay Mesh Framework ---\n");
    if (has_cfg && client_cfg.count > 0) {
        printf("Saved peers from tchat_client.conf:\n");
        for (int i = 0; i < client_cfg.count; i++) {
            printf("  %-20s -> %s:%d\n",
                   client_cfg.peers[i].name,
                   client_cfg.peers[i].host,
                   client_cfg.peers[i].port);
        }
    }

    char node_id_str[512];
    printf("Enter target server address or alias: ");
    if (!fgets(node_id_str, sizeof(node_id_str), stdin)) return 0;
    node_id_str[strcspn(node_id_str, "\n")] = 0;

    char resolved_host[128];
    int resolved_port;
    if (has_cfg && config_resolve_peer(&client_cfg, node_id_str, resolved_host, &resolved_port) == 0) {
        snprintf(node_id_str, sizeof(node_id_str), "%s:%d", resolved_host, resolved_port);
        printf("[Config] Resolved alias to %s\n", node_id_str);
    }

    if (strlen(node_id_str) == 0) {
        fprintf(stderr, "Error: Target cannot be empty.\n");
        return 1;
    }

    IrohEndpoint *endpoint = iroh_net_init_node(0);
    if (!endpoint) {
        fprintf(stderr, "Failed to initialize client network.\n");
        return 1;
    }

    printf("Connecting to %s ...\n", node_id_str);
    IrohConnection *conn = iroh_net_connect(endpoint, node_id_str);
    if (!conn) {
        fprintf(stderr, "Connection failure to target server.\n");
        return 1;
    }
    global_conn = conn;

    printf("Enter system registration identity pseudonym token: ");
    if (!fgets(my_nickname, MAX_NICK, stdin)) {
        iroh_net_close(conn);
        return 0;
    }
    my_nickname[strcspn(my_nickname, "\n")] = 0;

    TChatPayload join_p = {0};
    strncpy(join_p.nickname, my_nickname, MAX_NICK - 1);
    iroh_net_send_packet(conn, TYPE_JOIN, &join_p);

    enable_raw_mode();
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_server_messages, (void*)conn);

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
                } else if (strcmp(current_input, "/settings") == 0) {
                    pthread_mutex_lock(&tui_lock);
                    current_mode = STATE_SETTINGS;
                    selected_menu_item = 0;
                    printf("\n");
                    redraw_settings_menu();
                    pthread_mutex_unlock(&tui_lock);
                    continue;
                } else if (strcmp(current_input, "/users") == 0) {
                    iroh_net_send_packet(conn, TYPE_CMD_USERS, NULL);
                } else if (strncmp(current_input, "/name ", 6) == 0) {
                    TChatPayload p = {0};
                    strncpy(p.nickname, current_input + 6, MAX_NICK - 1);
                    strncpy(my_nickname, current_input + 6, MAX_NICK - 1);
                    iroh_net_send_packet(conn, TYPE_NAME_CHANGE, &p);
                } else if (strncmp(current_input, "/msg ", 5) == 0) {
                    char *target = strtok(current_input + 5, " ");
                    char *msg = strtok(NULL, "");
                    if (target && msg) {
                        TChatPayload p = {0};
                        strncpy(p.target, target, MAX_NICK - 1);
                        strncpy(p.data, msg, MAX_MSG - 1);
                        iroh_net_send_packet(conn, TYPE_PRIVATE, &p);

                        char local_echo[1024];
                        snprintf(local_echo, sizeof(local_echo), C_PRIV "[PM to %s]" C_RST ": %s", target, msg);
                        printf("%s\n", local_echo);
                    } else {
                        printf(C_SRV "[System] Invalid /msg syntax format.\n" C_RST);
                    }
                } else {
                    TChatPayload p = {0};
                    strncpy(p.data, current_input, MAX_MSG - 1);
                    iroh_net_send_packet(conn, TYPE_CHAT, &p);

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
    iroh_net_close(conn);
    return 0;
}
