// chat_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define close closesocket
#else
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#define PORT 8080
#define BUF_SIZE 1024

int sock;
volatile int running = 1;

#ifdef _WIN32
unsigned __stdcall input_thread(void* arg) {
#else
void* input_thread(void* arg) {
#endif
    char buffer[BUF_SIZE];
    while (running) {
        printf(">> ");
        fflush(stdout);

        if (fgets(buffer, BUF_SIZE, stdin)) {
            buffer[strcspn(buffer, "\n")] = '\0';

            int all_space = 1;
            for (int i = 0; buffer[i]; i++) {
                if (!isspace((unsigned char)buffer[i])) {
                    all_space = 0;
                    break;
                }
            }

            if (!all_space && strlen(buffer) > 0) {
                send(sock, buffer, strlen(buffer), 0);
            }
        }
    }

    return 0;
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    char nickname[BUF_SIZE];
    printf("Enter your nickname: ");
    fgets(nickname, BUF_SIZE, stdin);
    nickname[strcspn(nickname, "\n")] = '\0'; // remove newline

    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to chat server.\n");
    send(sock, nickname, strlen(nickname), 0);


    // 입력 스레드 시작
#ifdef _WIN32
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, input_thread, NULL, 0, NULL);
#else
    pthread_t tid;
    pthread_create(&tid, NULL, input_thread, NULL);
#endif

    // 서버로부터 메시지 받기
    while (1) {
        int bytes = recv(sock, buffer, BUF_SIZE - 1, 0);
        if (bytes <= 0) {
            printf("\nDisconnected from server.\n");
            break;
        }
        buffer[bytes] = '\0';

        // 현재 입력줄 지우고 메시지 출력, 다시 입력줄 표시
        printf("\33[2K\r%s\n>> ", buffer);  // 메시지 출력 후 >> 프롬프트 다시 표시
        fflush(stdout);
    }




    running = 0;
    close(sock);

#ifdef _WIN32
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    WSACleanup();
#else
    pthread_join(tid, NULL);
#endif

    return 0;
}