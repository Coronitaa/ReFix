#include "../src/refix_online/refix_backend_client.h"
#include "../src/refix_online/refix_backend_state.h"
#include <iostream>
#include <cassert>

using namespace ReFixOnline;

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix Online v2 - Backend Reconnect & Resync Unit Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& server = BackendServerState::Get();
    server.Reset();

    auto transport = std::make_unique<InProcessDirectTransport>(server);
    auto client = std::make_unique<BackendClient>(std::move(transport));

    std::string puid = "2b8db5d0c5cbd56ff14dff84a61cd9a2";
    std::string name = "Player_Alpha";

    // 1. Initial Connect & Auth
    std::cout << "\n[TEST 1] Initial Connect & Lobby Creation..." << std::endl;
    assert(client->Connect());
    assert(client->GetConnectionState() == EClientConnectionState::CONNECTED);

    bool authDone = false;
    client->Authenticate(puid, name, [&authDone](EBackendResult res, const std::string& token) {
        assert(res == SUCCESS);
        authDone = true;
    });
    client->Tick();
    assert(authDone);
    assert(client->GetConnectionState() == EClientConnectionState::AUTHENTICATED);

    bool createDone = false;
    std::string createdLobbyId;
    client->CreateLobby(4, { {"stage", "forest"}, {"mode", "coop"} }, [&createDone, &createdLobbyId](EBackendResult res, const LobbyData& lob) {
        assert(res == SUCCESS);
        createdLobbyId = lob.lobbyId;
        createDone = true;
    });
    client->Tick();
    assert(createDone && !createdLobbyId.empty());
    std::cout << "  Authenticated and created lobby: " << createdLobbyId << std::endl;

    // 2. Disconnect Simulation
    std::cout << "\n[TEST 2] Disconnect Simulation..." << std::endl;
    client->Disconnect();
    assert(client->GetConnectionState() == EClientConnectionState::DISCONNECTED);
    std::cout << "  Client disconnected." << std::endl;

    // 3. Reconnect & Re-Authentication
    std::cout << "\n[TEST 3] Reconnect & Re-Authentication..." << std::endl;
    assert(client->Connect());
    assert(client->GetConnectionState() == EClientConnectionState::CONNECTED);

    authDone = false;
    client->Authenticate(puid, name, [&authDone](EBackendResult res, const std::string& token) {
        assert(res == SUCCESS);
        authDone = true;
    });
    client->Tick();
    assert(authDone);
    assert(client->GetConnectionState() == EClientConnectionState::AUTHENTICATED);
    std::cout << "  Re-connected and re-authenticated successfully." << std::endl;

    // 4. Resync Active Lobby State
    std::cout << "\n[TEST 4] Resyncing Existing Lobby State..." << std::endl;
    bool resyncDone = false;
    client->ResyncLobby(createdLobbyId, [&resyncDone, createdLobbyId](EBackendResult res, const LobbyData& lob) {
        assert(res == SUCCESS);
        assert(lob.lobbyId == createdLobbyId);
        assert(lob.attributes.at("stage") == "forest");
        assert(lob.attributes.at("mode") == "coop");
        assert(lob.members.size() == 1);
        resyncDone = true;
    });
    client->Tick();
    assert(resyncDone);
    std::cout << "  Lobby state, attributes, and membership 100% resynchronized!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Backend Reconnect & Resync Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
