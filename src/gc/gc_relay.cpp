#include "gc_relay.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <cstdio>
#include <unordered_map>
#include <optional>
#include "steam_gc_emu.h"
#include "unreal_steam_emu.h"
#include "lan_core/refix_lan_core.h"
#include "lan_core/refix_lan_transport.h"
#include "source_main_thread_queue.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix::gc {

namespace {

constexpr uint32_t kGCRelayMagic = 0x53434752; // "RGCS" in little-endian

enum class GCRelayMsgType : uint8_t {
    RequestEquippedCache = 1,
    ResponseEquippedCache = 2,
    AckEquippedCache = 3,
    UpdateItemEquipped = 4,
    MatchmakingReserve = 5,
    QuestProgress = 6
};

#pragma pack(push, 1)
struct GCRelayHeader {
    uint32_t magic;      // kGCRelayMagic ("RGCS")
    uint8_t  msgType;    // GCRelayMsgType
    uint8_t  reserved[3];
    uint64_t steamId;    // Target or source SteamID
    uint32_t ticket;     // Auth ticket ID
    uint32_t dataLen;    // Length of subsequent payload bytes
};
#pragma pack(pop)

static bool s_initialized = false;
static std::mutex s_mutex;

// Helper to extract owner_soid.id from CMsgSOCacheSubscribed
static uint64_t ExtractOwnerSteamId(const uint8_t* data, size_t size) {
    auto getVarint = [](const uint8_t*& p, const uint8_t* end, uint64_t& val) -> bool {
        val = 0;
        int shift = 0;
        while (p < end) {
            uint8_t b = *p++;
            val |= (uint64_t)(b & 0x7F) << shift;
            if (!(b & 0x80)) return true;
            shift += 7;
            if (shift > 63) return false;
        }
        return false;
    };

    const uint8_t* p = data;
    const uint8_t* end = data + size;
    while (p < end) {
        uint64_t tag = 0, val = 0;
        if (!getVarint(p, end, tag)) break;
        uint32_t f = (uint32_t)(tag >> 3);
        uint32_t w = tag & 7;
        if (w == 0) {
            if (!getVarint(p, end, val)) break;
        } else if (w == 1) { if (end - p < 8) break; p += 8; }
        else if (w == 5) { if (end - p < 4) break; p += 4; }
        else if (w == 2) {
            if (!getVarint(p, end, val) || (size_t)(end - p) < val) break;
            if (f == 4) { // field 4 = owner_soid (CMsgSOIDOwner)
                const uint8_t* sp = p;
                const uint8_t* send = p + val;
                while (sp < send) {
                    uint64_t stag = 0, sval = 0;
                    if (!getVarint(sp, send, stag)) break;
                    uint32_t sf = (uint32_t)(stag >> 3);
                    uint32_t sw = stag & 7;
                    if (sw == 0) {
                        if (!getVarint(sp, send, sval)) break;
                        if (sf == 2) return sval; // field 2: id (SteamID)
                    } else if (sw == 1) { if (send - sp < 8) break; sp += 8; }
                    else if (sw == 5) { if (send - sp < 4) break; sp += 4; }
                    else if (sw == 2) {
                        if (!getVarint(sp, send, sval) || (size_t)(send - sp) < sval) break;
                        sp += sval;
                    }
                }
            }
            p += val;
        }
    }
    return 0;
}

} // namespace

void HandleRelayPacket(const refix::lan::PeerId& sender, const uint8_t* data, size_t len) {
    if (UnrealSteamEmu::GetAppID() != 730) return;
    if (!data || len < sizeof(GCRelayHeader)) return;
    const GCRelayHeader* hdr = reinterpret_cast<const GCRelayHeader*>(data);
    if (hdr->magic != kGCRelayMagic) return;
    ReFixLog("[GC-Relay] Received relay message %u for SteamID %llu (%zu bytes)", hdr->msgType, (unsigned long long)hdr->steamId, len);

    const uint8_t* payload = data + sizeof(GCRelayHeader);
    size_t payloadLen = len - sizeof(GCRelayHeader);
    if (payloadLen < hdr->dataLen) return;

    switch (static_cast<GCRelayMsgType>(hdr->msgType)) {
    case GCRelayMsgType::RequestEquippedCache: {
        uint64_t localSid = UnrealSteamEmu::GetLocalSteamID();
        if (hdr->steamId == localSid) {
            ReFixLog("[GC-Relay] Received RequestEquippedCache from host for local SteamID %llu", localSid);
            std::vector<uint8_t> cache = ReFixGC::BuildEquippedCacheSubscribed(localSid);

            std::vector<uint8_t> packet(sizeof(GCRelayHeader) + cache.size());
            GCRelayHeader* respHdr = reinterpret_cast<GCRelayHeader*>(packet.data());
            respHdr->magic = kGCRelayMagic;
            respHdr->msgType = static_cast<uint8_t>(GCRelayMsgType::ResponseEquippedCache);
            respHdr->steamId = localSid;
            respHdr->ticket = hdr->ticket;
            respHdr->dataLen = (uint32_t)cache.size();
            memcpy(packet.data() + sizeof(GCRelayHeader), cache.data(), cache.size());

            auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(sender);
            if (pInfo) {
                refix::lan::ILanCore::Get().Transport().SendReliable(sender, pInfo->endpoint, kLanChannelGC, packet.data(), packet.size());
                ReFixLog("[GC-Relay] Dispatched ResponseEquippedCache (%zu bytes) to host peer", cache.size());
            }
        }
        break;
    }
    case GCRelayMsgType::ResponseEquippedCache: {
        ReFixLog("[GC-Relay] Received ResponseEquippedCache from peer for SteamID %llu (%u bytes)", hdr->steamId, hdr->dataLen);
        std::vector<uint8_t> cacheBytes(payload, payload + hdr->dataLen);

        uint64_t ownerId = ExtractOwnerSteamId(cacheBytes.data(), cacheBytes.size());
        if (ownerId == hdr->steamId) {
            ReFixGC::SendToGameServer(24 /* k_ESOMsg_CacheSubscribed */, cacheBytes);
            ReFixLog("[GC] Injected verified remote player SOCacheSubscribed (24) for SteamID %llu into game server (%u bytes)",
                     hdr->steamId, hdr->dataLen);

            GCRelayHeader ackHdr = {};
            ackHdr.magic = kGCRelayMagic;
            ackHdr.msgType = static_cast<uint8_t>(GCRelayMsgType::AckEquippedCache);
            ackHdr.steamId = hdr->steamId;
            ackHdr.ticket = hdr->ticket;
            ackHdr.dataLen = 0;
            auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(sender);
            if (pInfo) {
                refix::lan::ILanCore::Get().Transport().SendReliable(sender, pInfo->endpoint, kLanChannelGC, &ackHdr, sizeof(ackHdr));
            }
        } else {
            ReFixLog("[GC-Relay] WARNING: owner_soid %llu mismatch with header SteamID %llu!", ownerId, hdr->steamId);
        }
        break;
    }
    case GCRelayMsgType::AckEquippedCache: {
        ReFixLog("[GC-Relay] Host confirmed SOCacheSubscribed receipt for SteamID %llu", hdr->steamId);
        break;
    }
    case GCRelayMsgType::UpdateItemEquipped: {
        ReFixLog("[GC-Relay] Host received UpdateItemEquipped for SteamID %llu (%u bytes)", hdr->steamId, hdr->dataLen);
        std::vector<uint8_t> updateBytes(payload, payload + hdr->dataLen);
        ReFixGC::SendToGameServer(26 /* k_ESOMsg_UpdateMultiple */, updateBytes);
        ReFixLog("[GC] Injected remote player UpdateMultiple (26) for SteamID %llu into game server", hdr->steamId);
        break;
    }
    case GCRelayMsgType::QuestProgress: {
        if (hdr->steamId != UnrealSteamEmu::GetLocalSteamID()) break;
        ReFixLog("[GC-Relay] Received match-end quest points from the host (%u bytes)", hdr->dataLen);
        ReFixGC::ApplyQuestProgress(payload, hdr->dataLen);
        break;
    }
    case GCRelayMsgType::MatchmakingReserve: {
        ReFixLog("[GC-Relay] Received MatchmakingReserve from leader (%u bytes)", hdr->dataLen);
        std::vector<uint8_t> reserveBytes(payload, payload + hdr->dataLen);
        ReFixGC::SendToClient(9107 /* kMsgGCCStrike15_v2_MatchmakingGC2ClientReserve */, reserveBytes);
        ReFixLog("[GC] Delivered MatchmakingGC2ClientReserve (9107) to local CS:GO client");
        break;
    }
    }
}

void InitializeGCRelay() {
    uint32_t appId = UnrealSteamEmu::GetAppID();
    if (appId != 730 && appId != 4465480) return;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) return;
    s_initialized = true;

    ReFixLog("[GC-Relay] Initializing CS:GO GC Cache Relay on LanCore channel %u ('RGCS')", kLanChannelGC);

    // Packets reach HandleRelayPacket from the LanCore event loop in unreal_steam_emu (no direct handler: the
    // channel is shared with rich presence).
}

void ShutdownGCRelay() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized) return;
    s_initialized = false;

}

void OnGameServerClientAuth(uint64_t clientSteamId, uint32_t ticket) {
    uint32_t appId = UnrealSteamEmu::GetAppID();
    if (appId != 730 && appId != 4465480) return;
    uint64_t localSid = UnrealSteamEmu::GetLocalSteamID();
    if (clientSteamId == localSid) {
        ReFixLog("[GC] Host player SteamID %llu connected to local game server. Injecting local equipped cache...", localSid);
        std::vector<uint8_t> cache = ReFixGC::BuildEquippedCacheSubscribed(localSid);
        ReFixGC::SendToGameServer(24 /* k_ESOMsg_CacheSubscribed */, cache);
        return;
    }

    // Remote client: locate peer and request cache with up to 5 retries
    std::thread([clientSteamId, ticket]() {
        GCRelayHeader req = {};
        req.magic = kGCRelayMagic;
        req.msgType = static_cast<uint8_t>(GCRelayMsgType::RequestEquippedCache);
        req.steamId = clientSteamId;
        req.ticket = ticket;
        req.dataLen = 0;

        for (int attempt = 1; attempt <= 5; ++attempt) {
            std::optional<refix::lan::PeerId> peerIdOpt = refix::lan::ILanCore::Get().Peers().FindBySteamId(clientSteamId);
            if (!peerIdOpt) {
                auto peers = refix::lan::ILanCore::Get().Peers().GetAllPeers();
                for (const auto& p : peers) {
                    if (p.externalId.numericId == clientSteamId) {
                        peerIdOpt = p.peerId;
                        break;
                    }
                }
            }

            if (peerIdOpt) {
                auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(*peerIdOpt);
                if (pInfo) {
                    auto res = refix::lan::ILanCore::Get().Transport().SendReliableDetailed(*peerIdOpt, pInfo->endpoint, kLanChannelGC, &req, sizeof(req));
                    ReFixLog("[GC-Relay] Dispatched RequestEquippedCache to %s for SteamID %llu (attempt %d/5, result %d)",
                             pInfo->endpoint.ToString().c_str(), clientSteamId, attempt, (int)res);
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }).detach();
}

void SendQuestProgress(uint64_t steamId, const std::vector<uint8_t>& playerQuestData) {
    if (UnrealSteamEmu::GetAppID() != 730) return;
    std::optional<refix::lan::PeerId> peerId = refix::lan::ILanCore::Get().Peers().FindBySteamId(steamId);
    if (!peerId) {
        for (const auto& p : refix::lan::ILanCore::Get().Peers().GetAllPeers())
            if (p.externalId.numericId == steamId) { peerId = p.peerId; break; }
    }
    auto pInfo = peerId ? refix::lan::ILanCore::Get().Peers().FindByPeerId(*peerId) : std::nullopt;
    if (!pInfo) {
        ReFixLog("[GC-Relay] No LAN peer for SteamID %llu: its quest points are lost", (unsigned long long)steamId);
        return;
    }
    std::vector<uint8_t> packet(sizeof(GCRelayHeader) + playerQuestData.size());
    GCRelayHeader* hdr = reinterpret_cast<GCRelayHeader*>(packet.data());
    hdr->magic = kGCRelayMagic;
    hdr->msgType = static_cast<uint8_t>(GCRelayMsgType::QuestProgress);
    hdr->steamId = steamId;
    hdr->dataLen = (uint32_t)playerQuestData.size();
    memcpy(packet.data() + sizeof(GCRelayHeader), playerQuestData.data(), playerQuestData.size());
    refix::lan::ILanCore::Get().Transport().SendReliable(*peerId, pInfo->endpoint, kLanChannelGC, packet.data(), packet.size());
}

void BroadcastEquippedUpdate(uint64_t itemId, uint32_t newClass, uint32_t newSlot, const std::vector<uint8_t>& soUpdateBytes) {
    if (UnrealSteamEmu::GetAppID() != 730) return;
    if (ReFixGC::IsGameServerActive()) {
        ReFixGC::SendToGameServer(26 /* k_ESOMsg_UpdateMultiple */, soUpdateBytes);
        ReFixLog("[GC] Host injected local UpdateMultiple (26) directly into game server");
    } else {
        std::vector<uint8_t> packet(sizeof(GCRelayHeader) + soUpdateBytes.size());
        GCRelayHeader* hdr = reinterpret_cast<GCRelayHeader*>(packet.data());
        hdr->magic = kGCRelayMagic;
        hdr->msgType = static_cast<uint8_t>(GCRelayMsgType::UpdateItemEquipped);
        hdr->steamId = UnrealSteamEmu::GetLocalSteamID();
        hdr->dataLen = (uint32_t)soUpdateBytes.size();
        memcpy(packet.data() + sizeof(GCRelayHeader), soUpdateBytes.data(), soUpdateBytes.size());

        auto allPeers = refix::lan::ILanCore::Get().Peers().GetAllPeers();
        for (const auto& p : allPeers) {
            if (p.peerId != refix::lan::ILanCore::Get().Identity().GetLocalPeerId()) {
                refix::lan::ILanCore::Get().Transport().SendReliable(p.peerId, p.endpoint, kLanChannelGC, packet.data(), packet.size());
            }
        }
        ReFixLog("[GC-Relay] Broadcasted UpdateItemEquipped to peers (%zu bytes)", soUpdateBytes.size());
    }
}


void BroadcastMatchmakingReserve(const std::vector<uint8_t>& reserveBytes) {
    if (UnrealSteamEmu::GetAppID() != 730) return;
    std::vector<uint8_t> packet(sizeof(GCRelayHeader) + reserveBytes.size());
    GCRelayHeader* hdr = reinterpret_cast<GCRelayHeader*>(packet.data());
    hdr->magic = kGCRelayMagic;
    hdr->msgType = static_cast<uint8_t>(GCRelayMsgType::MatchmakingReserve);
    hdr->steamId = UnrealSteamEmu::GetLocalSteamID();
    hdr->ticket = 0;
    hdr->dataLen = (uint32_t)reserveBytes.size();
    memcpy(packet.data() + sizeof(GCRelayHeader), reserveBytes.data(), reserveBytes.size());

    auto allPeers = refix::lan::ILanCore::Get().Peers().GetAllPeers();
    for (const auto& p : allPeers) {
        if (p.peerId != refix::lan::ILanCore::Get().Identity().GetLocalPeerId()) {
            refix::lan::ILanCore::Get().Transport().SendReliable(p.peerId, p.endpoint, kLanChannelGC, packet.data(), packet.size());
            ReFixLog("[GC-Relay] Sent MatchmakingReserve (9107) to peer at %s:%u",
                     p.endpoint.ToString().c_str(), p.endpoint.port);
        }
    }
}

void FindLanMatch(uint64_t targetSteamId, const std::function<void()>& onNotFound) {
    if (UnrealSteamEmu::GetAppID() != 730) return;
    // Hosts advertise their listen server as rich presence "refix:server" = "ip:port" (see unreal_steam_emu).
    for (const auto& peer : UnrealSteamEmu::PeersWithPresence("refix:server")) {
        if (targetSteamId != 0 && peer.first != targetSteamId) continue;
        if (!IsValidLanServerAddress(peer.second)) {
            ReFixLog("[GC-Relay] Ignoring invalid server address from SteamID %llu", (unsigned long long)peer.first);
            continue;
        }
        std::string cmd = "connect " + peer.second;
        ReFixLog("[GC-Relay] SteamID %llu hosts a match, joining: %s", (unsigned long long)peer.first, cmd.c_str());
        refix::source::MainThreadQueue::ExecuteClientCmd(cmd);
        ReFixGC::SendToClient(9104, { 0x08, 0x00 }); // MatchmakingGC2ClientUpdate: matchmaking = 0 (search over)
        return;
    }
    ReFixLog("[GC-Relay] No hosted match found on the LAN (target=%llu)", (unsigned long long)targetSteamId);
    if (onNotFound) onNotFound();
}

bool IsValidLanServerAddress(const std::string& address) {
    unsigned a, b, c, d, port;
    char tail;
    if (address.size() > 21 || address.find_first_not_of("0123456789.:") != std::string::npos) return false;
    if (sscanf_s(address.c_str(), "%u.%u.%u.%u:%u%c", &a, &b, &c, &d, &port, &tail, 1) != 5) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255 || port == 0 || port > 65535) return false;
    return a == 10 || a == 127 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168);
}

} // namespace refix::csgo
