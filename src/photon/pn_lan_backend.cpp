// ReFix LAN backend for the Photon emulator: implements the ReFixSteamFlat calls pn_p2p uses on top of the LAN
// emulator (UnrealSteamEmu) and LanCore instead of the Valve DLL, so the same emulator runs with no Steam at all.
//
//  * Identity, lobbies and "friends" come from the LAN emulator: every ReFix LAN peer of the same game is a
//    candidate host, so pn_p2p's room queries/announcements reach everyone on the LAN.
//  * Messages travel on a reserved LanCore channel (kPhotonLanChannel) and are handed to the Photon pump directly
//    from the transport thread (ILanCore::SetChannelHandler), not through the per-frame Steam callback queue.
//  * Rich presence and Steam session diagnostics have no LAN equivalent and answer empty.
// Built only into ReFix LAN (build.bat / build_x86_lan.bat); ReFix Online links refix_steam_flat.cpp instead.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../refix_steam_flat.h"
#include "../unreal_steam_emu.h"
#include "../lan_core/refix_lan_core.h"
#include "../lan_core/refix_lan_wire.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>
#include <map>

extern void ReFixLog(const char* fmt, ...);

namespace {

// LanCore channels 249 (lobby chat) and 250 (RFXS sockets) are taken by the emulator; games' own P2P channels
// are small numbers.
// Steam channel 9 (Photon's default) keeps channel 248 as is, so older ReFix builds still talk to this one; every
// other Steam channel (the Party Animals relay, a non-default Photon channel) shares 246 with a 1-byte channel
// prefix. Each Steam channel has its own inbox: the relay's pump must never swallow Photon's messages.
constexpr uint8_t kPhotonLanChannel = 248;
constexpr uint8_t kOtherLanChannel = 246;
constexpr int kPhotonSteamChannel = 9;
constexpr size_t kMaxQueuedMessages = 8192; // per Steam channel; drop the oldest beyond this if the pump stalls

struct Inbound {
    refix::lan::PeerId sender;
    std::vector<uint8_t> data;
};

std::mutex g_inboxMutex;
std::map<int, std::deque<Inbound>> g_inbox;   // by Steam channel
std::atomic<bool> g_attached{ false };

std::mutex g_idCacheMutex;
std::map<refix::lan::PeerId, uint64_t> g_peerToSteam;
std::map<uint64_t, refix::lan::PeerId> g_steamToPeer;

uint64_t SteamIdOf(const refix::lan::PeerId& peer) {
    auto info = refix::lan::ILanCore::Get().Peers().FindByPeerId(peer);
    if (info && info->externalId.numericId != 0) {
        std::lock_guard<std::mutex> lock(g_idCacheMutex);
        g_peerToSteam[peer] = info->externalId.numericId;
        g_steamToPeer[info->externalId.numericId] = peer;
        return info->externalId.numericId;
    }
    std::lock_guard<std::mutex> lock(g_idCacheMutex);
    auto it = g_peerToSteam.find(peer);
    return it != g_peerToSteam.end() ? it->second : 0;
}

void Queue(int channel, const refix::lan::PeerId& sender, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(g_inboxMutex);
    std::deque<Inbound>& box = g_inbox[channel];
    if (box.size() >= kMaxQueuedMessages) box.pop_front();
    box.push_back({ sender, std::vector<uint8_t>(data, data + len) });
}

// Installs the channel handler once the LAN emulator is up (LanCore is initialized with it).
bool EnsureAttached() {
    if (g_attached.load(std::memory_order_acquire)) return true;
    if (!UnrealSteamEmu::IsInitialized() || !UnrealSteamEmu::GetLocalSteamID()) return false;
    if (g_attached.exchange(true)) return true;
    refix::lan::ILanCore::Get().SetChannelHandler(kPhotonLanChannel,
        [](const refix::lan::PeerId& sender, const uint8_t* data, size_t len) {
            Queue(kPhotonSteamChannel, sender, data, len);
        });
    refix::lan::ILanCore::Get().SetChannelHandler(kOtherLanChannel,
        [](const refix::lan::PeerId& sender, const uint8_t* data, size_t len) {
            if (len > 1) Queue(data[0], sender, data + 1, len - 1);
        });
    ReFixLog("[Photon] LAN backend attached (LanCore channel %u)", kPhotonLanChannel);
    return true;
}

} // namespace

namespace ReFixSteamFlat {

uint64_t LocalSteamID() {
    return EnsureAttached() ? UnrealSteamEmu::GetLocalSteamID() : 0;
}

bool SetRichPresence(const char* key, const std::string& value) {
    if (!EnsureAttached()) return false;
    return UnrealSteamEmu::SetRichPresence(key, value.c_str());
}

std::vector<FriendPresence> FriendsWithRichPresence(const char* key) {
    std::vector<FriendPresence> result;
    if (!EnsureAttached()) return result;
    auto pairs = UnrealSteamEmu::GetAllFriendsWithRichPresence(key);
    result.reserve(pairs.size());
    for (const auto& p : pairs) {
        result.push_back({ p.first, p.second });
    }
    return result;
}

std::vector<uint64_t> FriendSteamIDs() {
    std::vector<uint64_t> ids;
    if (!EnsureAttached()) return ids;
    const uint64_t self = UnrealSteamEmu::GetLocalSteamID();
    for (const auto& peer : refix::lan::ILanCore::Get().Peers().GetAllPeers()) {
        uint64_t id = peer.externalId.numericId;
        if (id && id != self) ids.push_back(id);
    }
    return ids;
}

void ServiceMainThread() {}

std::string FriendRichPresence(uint64_t steamID, const char* key) {
    if (!EnsureAttached()) return {};
    const char* val = UnrealSteamEmu::GetFriendRichPresence(steamID, key);
    return val ? val : "";
}

uint64_t LobbyOwner(uint64_t lobbyId) {
    return lobbyId && EnsureAttached() ? UnrealSteamEmu::Matchmaking_GetLobbyOwner(lobbyId) : 0;
}

uint64_t ActiveLobbyID() {
    return EnsureAttached() ? UnrealSteamEmu::GetActiveLobbyID() : 0;
}

bool SendMessage(uint64_t steamID, const void* data, uint32_t size, int channel, bool reliable) {
    if (!data || !size || !EnsureAttached()) return false;
    auto& core = refix::lan::ILanCore::Get();
    auto peerId = core.Peers().FindBySteamId(steamID);
    if (!peerId) {
        std::lock_guard<std::mutex> lock(g_idCacheMutex);
        auto it = g_steamToPeer.find(steamID);
        if (it != g_steamToPeer.end()) peerId = it->second;
    }
    if (!peerId) return false;
    auto peer = core.Peers().FindByPeerId(*peerId);
    if (!peer || !peer->endpoint.IsValid()) return false;
    std::vector<uint8_t> tagged;
    uint8_t lanChannel = kPhotonLanChannel;
    if (channel != kPhotonSteamChannel) {
        if (channel < 0 || channel > 255) return false;
        tagged.reserve(size + 1);
        tagged.push_back((uint8_t)channel);
        tagged.insert(tagged.end(), (const uint8_t*)data, (const uint8_t*)data + size);
        data = tagged.data();
        size = (uint32_t)tagged.size();
        lanChannel = kOtherLanChannel;
    }
    if (reliable || size > refix::lan::REFIX_MAX_UNRELIABLE_PAYLOAD)
        return core.Transport().SendReliable(*peerId, peer->endpoint, lanChannel, data, size);
    return core.Transport().SendUnreliable(peer->endpoint, lanChannel, data, size);
}

int ReceiveMessages(int channel, int maxMessages, const std::function<void(uint64_t, const uint8_t*, uint32_t)>& onMessage) {
    if (maxMessages <= 0 || !EnsureAttached()) return 0;
    std::vector<Inbound> batch;
    {
        std::lock_guard<std::mutex> lock(g_inboxMutex);
        auto it = g_inbox.find(channel);
        while (it != g_inbox.end() && !it->second.empty() && (int)batch.size() < maxMessages) {
            batch.push_back(std::move(it->second.front()));
            it->second.pop_front();
        }
    }
    for (const auto& m : batch) {
        uint64_t from = SteamIdOf(m.sender);
        if (from) onMessage(from, m.data.data(), (uint32_t)m.data.size());
    }
    return (int)batch.size();
}

bool ConfigureNetworking(const NetworkingConfig&) { return true; } // LAN: direct UDP, nothing to configure

bool SessionInfo(uint64_t, SessionStatus&) { return false; }

bool AcceptSession(uint64_t) { return true; }

void OnSessionRequestCallback(const void*, int32_t) {}

void AcceptFriendSessions() {}

} // namespace ReFixSteamFlat
