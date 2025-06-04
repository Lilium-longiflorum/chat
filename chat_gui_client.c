#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024
#define DEFAULT_PORT 8080

HWND hwndOutput, hwndInput, hwndMain;
SOCKET sock;
char nickname[BUF_SIZE] = "Anonymous";
char server_ip[BUF_SIZE] = "127.0.0.1";
int server_port = DEFAULT_PORT;

WNDPROC oldInputProc;
// ========================= UTF-8 출력 ============================
void PrintUTF8ToEdit(HWND hwnd, const char* utf8str) {
    wchar_t wbuf[BUF_SIZE * 2];
    char ansi[BUF_SIZE * 2];

    // UTF-8 → UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, wbuf, sizeof(wbuf)/sizeof(wchar_t));
    if (wlen == 0) return;

    // UTF-16 → ANSI (로컬 인코딩)
    int alen = WideCharToMultiByte(CP_ACP, 0, wbuf, -1, ansi, sizeof(ansi), NULL, NULL);
    if (alen == 0) return;

    // 출력
    SendMessage(hwnd, EM_SETSEL, -1, -1);
    SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)ansi);
}


// ======================= 서버 주소 입력 ===========================
int PromptServerAddress(HINSTANCE hInstance) {
    HWND hwndDialog = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "STATIC", "Connect to server",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        400, 300, 300, 200,
        NULL, NULL, hInstance, NULL
    );

    HWND hwndIPEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        20, 40, 250, 25, hwndDialog, (HMENU)201, hInstance, NULL);

    HWND hwndPortEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        20, 80, 250, 25, hwndDialog, (HMENU)202, hInstance, NULL);

    HWND hwndButton = CreateWindow("BUTTON", "Connect", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        100, 120, 80, 25, hwndDialog, (HMENU)203, hInstance, NULL);

    SetWindowText(hwndIPEdit, "127.0.0.1");
    SetWindowText(hwndPortEdit, "8080");

    ShowWindow(hwndDialog, SW_SHOW);
    UpdateWindow(hwndDialog);

    MSG msg;
    BOOL running = TRUE;

    while (running && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.hwnd == hwndButton && msg.message == WM_LBUTTONUP) {
            char portStr[16];
            GetWindowText(hwndIPEdit, server_ip, BUF_SIZE);
            GetWindowText(hwndPortEdit, portStr, sizeof(portStr));
            server_port = atoi(portStr);
            if (strlen(server_ip) > 0 && server_port > 0) {
                running = FALSE;
                DestroyWindow(hwndDialog);
            } else {
                MessageBox(hwndDialog, "Invalid IP or port", "Error", MB_OK);
            }
        } else if (msg.message == WM_CLOSE || msg.message == WM_DESTROY) {
            return 0;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 1;
}

// ======================= 닉네임 입력 ===========================
int PromptNickname(HINSTANCE hInstance) {
    HWND hwndDialog = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "STATIC", "Enter your nickname",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        400, 300, 300, 150,
        NULL, NULL, hInstance, NULL
    );

    HWND hwndEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
        20, 40, 250, 25, hwndDialog, (HMENU)101, hInstance, NULL);

    HWND hwndButton = CreateWindow("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        100, 80, 80, 25, hwndDialog, (HMENU)102, hInstance, NULL);

    ShowWindow(hwndDialog, SW_SHOW);
    UpdateWindow(hwndDialog);

    MSG msg;
    BOOL running = TRUE;

    while (running && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.hwnd == hwndButton && msg.message == WM_LBUTTONUP) {
            GetWindowText(hwndEdit, nickname, BUF_SIZE);
            if (strlen(nickname) > 0) {
                running = FALSE;
                DestroyWindow(hwndDialog);
            } else {
                MessageBox(hwndDialog, "Nickname cannot be empty.", "Error", MB_OK);
            }
        } else if (msg.message == WM_CLOSE || msg.message == WM_DESTROY) {
            nickname[0] = '\0';
            running = FALSE;
            DestroyWindow(hwndDialog);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return strlen(nickname) > 0;
}

// ======================= 수신 스레드 ===========================
DWORD WINAPI ReceiveThread(LPVOID lpParam) {
    char buffer[BUF_SIZE];
    int bytes;

    while ((bytes = recv(sock, buffer, BUF_SIZE - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        // SendMessage(hwndOutput, EM_SETSEL, -1, -1);
        // SendMessage(hwndOutput, EM_REPLACESEL, FALSE, (LPARAM)buffer);
        PrintUTF8ToEdit(hwndOutput, buffer);
    }

    MessageBox(NULL, "Disconnected from server", "Info", MB_OK);
    PostQuitMessage(0);
    return 0;
}

// ======================= 서버로 메시지 전송 ===========================
void SendMessageToServer() {
    wchar_t wbuf[BUF_SIZE];
    char utf8buf[BUF_SIZE * 2];

    GetWindowTextW(hwndInput, wbuf, BUF_SIZE);  // 유니코드로 가져옴

    if (wcslen(wbuf) == 0) return;

    int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8buf, sizeof(utf8buf), NULL, NULL);
    if (len > 0) {
        strcat(utf8buf, "\n");
        send(sock, utf8buf, strlen(utf8buf), 0);
    }

    SetWindowText(hwndInput, "");
}

// ======================= 입력창 서브클래싱 ===========================
LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageToServer();
        return 0;
    }
    return CallWindowProc(oldInputProc, hwnd, msg, wParam, lParam);
}

// ======================= 메인 윈도우 콜백 ===========================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            hwndOutput = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN | WS_VSCROLL,
                10, 10, 460, 300, hwnd, NULL, NULL, NULL);

            hwndInput = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 320, 360, 25, hwnd, NULL, NULL, NULL);

            oldInputProc = (WNDPROC)SetWindowLongPtr(hwndInput, GWLP_WNDPROC, (LONG_PTR)InputProc);

            CreateWindow("BUTTON", "Send", WS_CHILD | WS_VISIBLE | WS_BORDER,
                380, 320, 90, 25, hwnd, (HMENU)1, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1) {
                SendMessageToServer();
            }
            break;

        case WM_CLOSE:
            if (MessageBox(hwnd, "Are you sure you want to quit?", "Exit", MB_OKCANCEL) == IDOK) {
                PostQuitMessage(0);
            }
            return 0;

        case WM_DESTROY:
            closesocket(sock);
            WSACleanup();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ======================= WinMain ===========================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nCmdShow) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    // 1. 서버 주소 입력
    if (!PromptServerAddress(hInstance)) {
        MessageBox(NULL, "Server address required!", "Error", MB_OK);
        return 1;
    }

    // 2. 서버 연결
    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        MessageBox(NULL, "Unable to connect", "Error", MB_OK);
        return 1;
    }

    // 3. 닉네임 입력
    if (!PromptNickname(hInstance)) {
        MessageBox(NULL, "Nickname required!", "Error", MB_OK);
        return 1;
    }

    // 4. 닉네임 전송
    send(sock, nickname, strlen(nickname), 0);

    // 5. 채팅 GUI 띄우기
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ChatClientClass";
    RegisterClass(&wc);

    hwndMain = CreateWindow("ChatClientClass", "C Chat Client GUI",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // 6. 수신 스레드 시작
    CreateThread(NULL, 0, ReceiveThread, NULL, 0, NULL);

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
