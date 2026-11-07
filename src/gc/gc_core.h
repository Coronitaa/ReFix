// Valve GameCoordinator Core: Protobuf & legacy framing, SOCache, and multi-peer relay
// Generic across all Valve Source titles (CS:GO, TF2, Dota 2, Artifact, Underlords)
#pragma once

#include "../include/refix_services.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>

namespace refix::gc {

constexpr uint32_t kMsgProtobufFlag = 0x80000000;
constexpr size_t kLegacyHeaderSize = 18;

struct GcMessage {
    uint32_t type = 0;
    std::vector<uint8_t> body;
    bool isProtobuf = false;
    uint64_t jobIdSource = 0;
    uint64_t jobIdTarget = 0;
};

class GcCore : public refix::IGcCore {
public:
    static GcCore& Get();

    GcCore();
    virtual ~GcCore() override = default;

    // IGcCore Interface Implementation
    virtual void RegisterClientHandler(uint32_t msg_type, GcMessageHandlerFn fn) override;
    virtual void RegisterServerHandler(uint32_t msg_type, GcMessageHandlerFn fn) override;
    virtual void SendClient(uint32_t msg_type, const void* data, size_t size, bool is_protobuf) override;
    virtual void SendServer(uint32_t msg_type, const void* data, size_t size, bool is_protobuf) override;
    virtual void SendSOCacheSubscribed(uint64_t target_steam_id, const void* items, size_t count) override;
    virtual void SendSingleObject(uint64_t steam_id, int type_id, const void* data, size_t size) override;
    virtual uint64_t GetActiveSteamID() override;
    virtual uint32_t GetActiveAppID() override;
    virtual void RelayCacheToServer(uint64_t remote_steam_id) override;

    // Internal GC routing
    bool DispatchClientMessage(uint32_t msgType, const uint8_t* pubData, size_t cubData);
    bool DispatchServerMessage(uint32_t msgType, const uint8_t* pubData, size_t cubData);

    bool HasClientMessage() const;
    bool RetrieveClientMessage(uint32_t* punMsgType, void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize);

    bool HasServerMessage() const;
    bool RetrieveServerMessage(uint32_t* punMsgType, void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize);

private:
    std::mutex m_mu;
    std::unordered_map<uint32_t, GcMessageHandlerFn> m_clientHandlers;
    std::unordered_map<uint32_t, GcMessageHandlerFn> m_serverHandlers;

    std::vector<GcMessage> m_clientQueue;
    std::vector<GcMessage> m_serverQueue;
};

} // namespace refix::gc
