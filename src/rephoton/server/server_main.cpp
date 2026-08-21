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
    uint16_t masterPort = 5055;
    uint16_t nameServerPort = 5058;

    if (argc > 1) {
        masterPort = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc > 2) {
        nameServerPort = static_cast<uint16_t>(std::atoi(argv[2]));
    }

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    printf("====================================================================\n");
    printf("     ReFix Online-Photon Realtime & Name Server (Ports %u / %u)    \n", masterPort, nameServerPort);
    printf("====================================================================\n");
    printf("[INFO] Clean-room ENet + Protocol 1.6 GpBinary Server Engine\n");
    printf("[INFO] MasterServer Port:     %u (Matchmaking, Rooms, Lobbies)\n", masterPort);
    printf("[INFO] NameServer Port:       %u (Region Discovery - 'sa', 'us', 'eu', 'asia')\n", nameServerPort);
    printf("[INFO] Default Region:        'sa' (South America - 127.0.0.1:%u)\n", masterPort);
    printf("[INFO] Press Ctrl+C in console to stop server\n\n");

    if (!g_server.Start(masterPort, nameServerPort)) {
        printf("[ERROR] Failed to start PhotonServer on port %u\n", masterPort);
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
