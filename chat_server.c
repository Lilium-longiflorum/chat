#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#define closesocket close
#endif

#define PORT 8080
#define WEB_PORT 9000
#define BUF_SIZE 1024
#define MAX_CLIENTS 10
#define NAME_LEN 32

int is_private_ip(struct in_addr addr) {
    uint32_t ip = ntohl(addr.s_addr); // host byte order로 변환

    if ((ip >> 24) == 127) return 1; // 127.0.0.0/8
    if ((ip >> 24) == 10) return 1;  // 10.0.0.0/8
    if ((ip >> 20) == (172 << 4 | 1)) return 1; // 172.16.0.0 ~ 172.31.255.255
    if ((ip >> 16) == (192 << 8 | 168)) return 1; // 192.168.0.0/16
    return 0;
}

int is_http_request(const char* buffer) {
    // 요청의 첫 줄만 검사하는 게 아니라, 전체가 HTTP 요청처럼 생겼는지 체크
    if (strstr(buffer, "HTTP/") != NULL &&
        (strstr(buffer, "GET ") != NULL ||
         strstr(buffer, "POST") != NULL ||
         strstr(buffer, "HEAD") != NULL ||
         strstr(buffer, "PUT") != NULL ||
         strstr(buffer, "DELETE") != NULL ||
         strstr(buffer, "OPTIONS") != NULL ||
         strstr(buffer, "PATCH") != NULL)) {
        return 1;
    }

    return 0;
}


void normalize_newlines(char* dest, const char* src) {
    while (*src) {
        if (*src == '\r') {
            if (*(src + 1) == '\n') {
                *dest++ = '\r';
                *dest++ = '\n';
                src += 2;
            } else {
                *dest++ = '\r';
                *dest++ = '\n';
                src++;
            }
        } else if (*src == '\n') {
            *dest++ = '\r';
            *dest++ = '\n';
            src++;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

void broadcast_message_normalized(int sender_fd, const char* raw_message, int* client_sockets, int max_clients) {
    char normalized[BUF_SIZE * 2];
    normalize_newlines(normalized, raw_message);
    for (int i = 0; i < max_clients; i++) {
        int sd = client_sockets[i];
        if (sd != 0) {
            send(sd, normalized, strlen(normalized), 0);
        }
    }
}

void send_normalized(int client_fd, const char* raw_message) {
    char normalized[BUF_SIZE * 2];
    normalize_newlines(normalized, raw_message);
    send(client_fd, normalized, strlen(normalized), 0);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    int server_fd, client_fd, max_fd, activity, i;
    int client_sockets[MAX_CLIENTS] = {0};
    int name_received[MAX_CLIENTS] = {0};
    char nicknames[MAX_CLIENTS][NAME_LEN] = {{0}};
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[BUF_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, MAX_CLIENTS);

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* host = gethostbyname(hostname);
        if (host) {
            printf("Server available at:\n");
            for (int i = 0; host->h_addr_list[i] != NULL; i++) {
                struct in_addr addr;
                memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));

                if (!is_private_ip(addr)) {
                    printf("  app: %s:%d\n", inet_ntoa(addr), PORT);
                    printf("  web: %s:%d\n", inet_ntoa(addr), WEB_PORT);
                }
            }
        } else {
            perror("gethostbyname");
        }
    } else {
        perror("gethostname");
    }

    printf("server start...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_fd)
                    max_fd = client_sockets[i];
            }
        }

        activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            socklen_t addrlen = sizeof(address);
            client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_fd;
                    name_received[i] = 0;
                    printf("new client (socket %d)\n", client_fd);
                    break;
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int valread = recv(sd, buffer, BUF_SIZE - 1, 0);
                if (valread <= 0) {
                    if (name_received[i]) {
                        char leave_msg[BUF_SIZE + NAME_LEN];
                        snprintf(leave_msg, sizeof(leave_msg), "[System]: %s left the chat\n", nicknames[i]);
                        broadcast_message_normalized(sd, leave_msg, client_sockets, MAX_CLIENTS);
                    }

                    printf("client exit(socket %d)\n", sd);
                    closesocket(sd);
                    client_sockets[i] = 0;
                    name_received[i] = 0;
                    nicknames[i][0] = '\0';
                } else {
                    buffer[valread] = '\0';

                    if (name_received[i]) {
                        if (is_http_request(buffer)) {
                            const char* reject_msg = "Invalid connection.\n";
                            send(sd, reject_msg, strlen(reject_msg), 0);
                            closesocket(sd);
                            client_sockets[i] = 0;
                            name_received[i] = 0;
                            nicknames[i][0] = '\0';
                            continue;
                        }
                    }

                    if (!name_received[i]) {
                        if (is_http_request(buffer)) {
                            const char* reject_msg = "Invalid connection.\n";
                            send(sd, reject_msg, strlen(reject_msg), 0);
                            closesocket(sd);
                            client_sockets[i] = 0;
                            continue;
                        }

                        strncpy(nicknames[i], buffer, NAME_LEN - 1);
                        nicknames[i][NAME_LEN - 1] = '\0';

                        char* temp = nicknames[i];
                        int si = 0;
                        int sj = 0;
                        for (; temp[si] != '\0'; si++) {
                            if (temp[si] != '\n' && temp[si] != '\r') {
                                nicknames[i][sj++] = temp[si];
                            }
                        }
                        nicknames[i][sj] = '\0';

                        name_received[i] = 1;
                        printf("client(%d) name: %s\n", sd, nicknames[i]);

                        int count = 0;
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (client_sockets[j] != 0 && name_received[j]) {
                                count++;
                            }
                        }

                        char join_new[BUF_SIZE];
                        snprintf(join_new, sizeof(join_new), "[System]: total %d is there\n", count);
                        send_normalized(sd, join_new);

                        char join_msg[BUF_SIZE + NAME_LEN];
                        snprintf(join_msg, sizeof(join_msg), "[System]: %s joined the chat\n", nicknames[i]);
                        broadcast_message_normalized(sd, join_msg, client_sockets, MAX_CLIENTS);

                    } else {
                        char msg_with_name[BUF_SIZE + NAME_LEN];
                        if(buffer[strlen(buffer)-1] != '\n'){
                            snprintf(msg_with_name, sizeof(msg_with_name), "[%s]: %s\n", nicknames[i], buffer);
                        } else{
                            snprintf(msg_with_name, sizeof(msg_with_name), "[%s]: %s", nicknames[i], buffer);
                        }
                        broadcast_message_normalized(sd, msg_with_name, client_sockets, MAX_CLIENTS);
                    }
                }
            }
        }
    }

    closesocket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
