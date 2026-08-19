#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "server_browser_gui.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")

static HWND g_hBrowserWnd = NULL;
static HWND g_hListView = NULL;
static HWND g_hStatusText = NULL;
static HWND g_hIpInput = NULL;
static HANDLE g_hUIThread = NULL;

struct ServerItem {
    std::string name;
    std::string ipPort;
    std::string lobbyId;
    std::string status;
};

static std::vector<ServerItem> g_guiServerList;

static void RefreshServerListUI() {
    if (!g_hListView) return;
    SendMessage(g_hListView, LVM_DELETEALLITEMS, 0, 0);
    g_guiServerList.clear();

    char gameFilter[128] = "refix_game_default";
    GetEnvironmentVariableA("REFIX_GAME_FILTER", gameFilter, sizeof(gameFilter));

    char userName[128] = "Player";
    GetEnvironmentVariableA("REFIX_USERNAME", userName, sizeof(userName));

    char publicIP[64] = "";
    GetEnvironmentVariableA("REFIX_PUBLIC_IP", publicIP, sizeof(publicIP));
    if (publicIP[0] == '\0') GetEnvironmentVariableA("REFIX_LOCAL_IP", publicIP, sizeof(publicIP));

    if (publicIP[0] != '\0' && strcmp(publicIP, "Unknown") != 0) {
        ServerItem s;
        s.name = std::string(userName) + " (Host)";
        s.ipPort = std::string(publicIP) + ":7777";
        s.lobbyId = "76561197960287930";
        s.status = "ONLINE";
        g_guiServerList.push_back(s);
    }

    for (size_t i = 0; i < g_guiServerList.size(); i++) {
        LVITEMA lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;

        char numStr[16];
        sprintf_s(numStr, sizeof(numStr), "%zu", i + 1);
        lvi.pszText = numStr;
        SendMessageA(g_hListView, LVM_INSERTITEMA, 0, (LPARAM)&lvi);

        ListView_SetItemText(g_hListView, (int)i, 1, (LPSTR)g_guiServerList[i].name.c_str());
        ListView_SetItemText(g_hListView, (int)i, 2, (LPSTR)g_guiServerList[i].ipPort.c_str());
        ListView_SetItemText(g_hListView, (int)i, 3, (LPSTR)g_guiServerList[i].lobbyId.c_str());
        ListView_SetItemText(g_hListView, (int)i, 4, (LPSTR)g_guiServerList[i].status.c_str());
    }

    if (g_hStatusText) {
        char statusBuf[256];
        sprintf_s(statusBuf, sizeof(statusBuf), "Active Game Filter: [%s] | Found %zu active server(s)", gameFilter, g_guiServerList.size());
        SetWindowTextA(g_hStatusText, statusBuf);
    }
}

static LRESULT CALLBACK WndProcBrowser(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_LISTVIEW_CLASSES;
            InitCommonControlsEx(&icex);

            g_hStatusText = CreateWindowA("STATIC", "Querying Servers...", WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 12, 715, 20, hWnd, NULL, NULL, NULL);

            g_hListView = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                          15, 38, 715, 275, hWnd, (HMENU)101, NULL, NULL);

            ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            LVCOLUMNA lvc = { 0 };
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            
            lvc.cx = 40; lvc.pszText = (LPSTR)"#"; SendMessageA(g_hListView, LVM_INSERTCOLUMNA, 0, (LPARAM)&lvc);
            lvc.cx = 180; lvc.pszText = (LPSTR)"Host Player Name"; SendMessageA(g_hListView, LVM_INSERTCOLUMNA, 1, (LPARAM)&lvc);
            lvc.cx = 180; lvc.pszText = (LPSTR)"Public IP Address:Port"; SendMessageA(g_hListView, LVM_INSERTCOLUMNA, 2, (LPARAM)&lvc);
            lvc.cx = 180; lvc.pszText = (LPSTR)"Steam Lobby ID"; SendMessageA(g_hListView, LVM_INSERTCOLUMNA, 3, (LPARAM)&lvc);
            lvc.cx = 100; lvc.pszText = (LPSTR)"Status"; SendMessageA(g_hListView, LVM_INSERTCOLUMNA, 4, (LPARAM)&lvc);

            CreateWindowA("BUTTON", "Refresh Servers", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 328, 140, 32, hWnd, (HMENU)201, NULL, NULL);
            CreateWindowA("BUTTON", "Connect Selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 165, 328, 150, 32, hWnd, (HMENU)202, NULL, NULL);

            CreateWindowA("STATIC", "Direct IP:", WS_CHILD | WS_VISIBLE | SS_LEFT, 330, 335, 60, 20, hWnd, NULL, NULL, NULL);
            g_hIpInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "127.0.0.1:7777", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 395, 331, 180, 26, hWnd, (HMENU)102, NULL, NULL);
            CreateWindowA("BUTTON", "Connect IP", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 585, 328, 145, 32, hWnd, (HMENU)203, NULL, NULL);

            RefreshServerListUI();
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == 201) { // Refresh
                RefreshServerListUI();
            } else if (wmId == 202 || wmId == 203) { // Connect
                char ipBuf[128] = "";
                if (wmId == 203 && g_hIpInput) {
                    GetWindowTextA(g_hIpInput, ipBuf, sizeof(ipBuf));
                } else if (g_hListView) {
                    int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                    if (sel >= 0 && sel < (int)g_guiServerList.size()) {
                        strcpy_s(ipBuf, sizeof(ipBuf), g_guiServerList[sel].ipPort.c_str());
                    }
                }

                if (ipBuf[0] != '\0') {
                    char msg[256];
                    sprintf_s(msg, sizeof(msg), "Connecting to server: %s\n\nInvite link active! Check Steam Overlay if required.", ipBuf);
                    MessageBoxA(hWnd, msg, "ReFix Connection", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxA(hWnd, "Please select a server or enter an IP:Port to connect.", "ReFix Notice", MB_OK | MB_ICONWARNING);
                }
            }
            break;
        }
        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            g_hBrowserWnd = NULL;
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static DWORD WINAPI UIMessageLoopThread(LPVOID lpParam) {
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProcBrowser;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "ReFixServerBrowserClass";

    RegisterClassExA(&wc);

    g_hBrowserWnd = CreateWindowExA(
        WS_EX_TOPMOST,
        "ReFixServerBrowserClass",
        "ReFix Online - Integrated Server Browser",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 415,
        NULL, NULL, GetModuleHandleA(NULL), NULL
    );

    if (g_hBrowserWnd) {
        ShowWindow(g_hBrowserWnd, SW_SHOW);
        UpdateWindow(g_hBrowserWnd);
        SetForegroundWindow(g_hBrowserWnd);
    }

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    g_hBrowserWnd = NULL;
    g_hUIThread = NULL;
    return 0;
}

void ShowServerBrowserGUI() {
    if (g_hBrowserWnd && IsWindow(g_hBrowserWnd)) {
        ShowWindow(g_hBrowserWnd, SW_SHOW);
        SetForegroundWindow(g_hBrowserWnd);
        RefreshServerListUI();
        return;
    }

    if (!g_hUIThread) {
        g_hUIThread = CreateThread(NULL, 0, UIMessageLoopThread, NULL, 0, NULL);
    }
}

static DWORD WINAPI GUIHotkeyThread(LPVOID lpParam) {
    DWORD lastToggle = 0;
    while (true) {
        Sleep(50);
        DWORD now = GetTickCount();
        if (now - lastToggle > 300) {
            bool keyF2Pressed   = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
            bool keyHomePressed = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
            if (keyF2Pressed || keyHomePressed) {
                lastToggle = now;
                ShowServerBrowserGUI();
            }
        }
    }
    return 0;
}

void StartHotkeyThreadGUI() {
    CreateThread(NULL, 0, GUIHotkeyThread, NULL, 0, NULL);
}
