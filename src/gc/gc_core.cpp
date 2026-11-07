#include "gc_core.h"
#include "../lan_core/refix_lan_core.h"
#include <cstring>
#include <windows.h>

#include "../unreal_steam_emu.h" 

namespace refix::gc {

GcCore& GcCore::Get() {
    static GcCore s_instance;
    return s_instance;
}

GcCore::GcCore() {
    refix::GetRefixServices().gc = this;
}

void GcCore::RegisterClientHandler(uint32_t msg_type, GcMessageHandlerFn fn) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_clientHandlers[msg_type] = std::move(fn);
}

void GcCore::RegisterServerHandler(uint32_t msg_type, GcMessageHandlerFn fn) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_serverHandlers[msg_type] = std::move(fn);
}

void GcCore::SendClient(uint32_t msg_type, const void* data, size_t size, bool is_protobuf) {
    std::lock_guard<std::mutex> lock(m_mu);
    GcMessage msg;
    msg.type = is_protobuf ? (msg_type | kMsgProtobufFlag) : msg_type;
    msg.isProtobuf = is_protobuf;
    if (size > 0 && data) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
        msg.body.assign(bytes, bytes + size);
    }
    m_clientQueue.push_back(std::move(msg));
}

void GcCore::SendServer(uint32_t msg_type, const void* data, size_t size, bool is_protobuf) {
    std::lock_guard<std::mutex> lock(m_mu);
    GcMessage msg;
    msg.type = is_protobuf ? (msg_type | kMsgProtobufFlag) : msg_type;
    msg.isProtobuf = is_protobuf;
    if (size > 0 && data) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
        msg.body.assign(bytes, bytes + size);
    }
    m_serverQueue.push_back(std::move(msg));
}

void GcCore::SendSOCacheSubscribed(uint64_t target_steam_id, const void* items, size_t count) {
    // Forwarded to SOCache builder or mod handler
}

void GcCore::SendSingleObject(uint64_t steam_id, int type_id, const void* data, size_t size) {
    // Forwarded to SOCache single object
}

uint64_t GcCore::GetActiveSteamID() {
    return UnrealSteamEmu::GetLocalSteamID();
}

uint32_t GcCore::GetActiveAppID() {
    return UnrealSteamEmu::GetAppID();
}

void GcCore::RelayCacheToServer(uint64_t remote_steam_id) {
    // Multi-peer cache relay trigger
}

bool GcCore::DispatchClientMessage(uint32_t msgType, const uint8_t* pubData, size_t cubData) {
    GcMessageHandlerFn handler;
    bool isProtobuf = (msgType & kMsgProtobufFlag) != 0;
    uint32_t rawType = msgType & ~kMsgProtobufFlag;

    {
        std::lock_guard<std::mutex> lock(m_mu);
        auto it = m_clientHandlers.find(rawType);
        if (it != m_clientHandlers.end()) {
            handler = it->second;
        }
    }

    if (handler) {
        handler(pubData, cubData, isProtobuf);
        return true;
    }
    return false;
}

bool GcCore::DispatchServerMessage(uint32_t msgType, const uint8_t* pubData, size_t cubData) {
    GcMessageHandlerFn handler;
    bool isProtobuf = (msgType & kMsgProtobufFlag) != 0;
    uint32_t rawType = msgType & ~kMsgProtobufFlag;

    {
        std::lock_guard<std::mutex> lock(m_mu);
        auto it = m_serverHandlers.find(rawType);
        if (it != m_serverHandlers.end()) {
            handler = it->second;
        }
    }

    if (handler) {
        handler(pubData, cubData, isProtobuf);
        return true;
    }
    return false;
}

bool GcCore::HasClientMessage() const {
    return !m_clientQueue.empty();
}

bool GcCore::RetrieveClientMessage(uint32_t* punMsgType, void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize) {
    if (m_clientQueue.empty()) return false;

    auto msg = m_clientQueue.front();
    m_clientQueue.erase(m_clientQueue.begin());

    if (punMsgType) *punMsgType = msg.type;
    if (pcubMsgSize) *pcubMsgSize = static_cast<uint32_t>(msg.body.size());

    if (pubDest && cubDest >= msg.body.size()) {
        if (!msg.body.empty()) {
            memcpy(pubDest, msg.body.data(), msg.body.size());
        }
        return true;
    }
    return false;
}

bool GcCore::HasServerMessage() const {
    return !m_serverQueue.empty();
}

bool GcCore::RetrieveServerMessage(uint32_t* punMsgType, void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize) {
    if (m_serverQueue.empty()) return false;

    auto msg = m_serverQueue.front();
    m_serverQueue.erase(m_serverQueue.begin());

    if (punMsgType) *punMsgType = msg.type;
    if (pcubMsgSize) *pcubMsgSize = static_cast<uint32_t>(msg.body.size());

    if (pubDest && cubDest >= msg.body.size()) {
        if (!msg.body.empty()) {
            memcpy(pubDest, msg.body.data(), msg.body.size());
        }
        return true;
    }
    return false;
}

} // namespace refix::gc
