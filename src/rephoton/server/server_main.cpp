#include "photon_server.h"
#include "../diagnostics/photon_diagnostics.h"
#include <windows.h>
#include <iostream>
#include <csignal>

static ReFix::Photon::Server::PhotonServer g_server;
static std::atomic<bool> g_keepRunning{ true };

static void SignalHandler(int signum) {
    g_keepRunning = false;
}

int main(int argc, char* argv[]) {
    uint16_t port = 5055;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    printf("====================================================================\n");
    printf("         ReFix Online-Photon Realtime UDP Server (Port %u)          \n", port);
    printf("====================================================================\n");
    printf("[INFO] Clean-room ENet + Protocol 1.6 GpBinary Server Engine\n");
    printf("[INFO] Press Ctrl+C or ESC in console to stop server\n\n");

    if (!g_server.Start(port)) {
        printf("[ERROR] Failed to start PhotonServer on port %u\n", port);
        return 1;
    }

    while (g_keepRunning) {
        g_server.Update();
        Sleep(5); // 200 Hz server tick
    }

    g_server.Stop();
    printf("[INFO] Server stopped gracefully.\n");
    return 0;
}
