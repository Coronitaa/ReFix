#include "fault_injector.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

namespace ReFix {

FaultInjector& FaultInjector::Get() {
    static FaultInjector s_instance;
    return s_instance;
}

FaultInjector::FaultInjector() {
    Initialize();
}

void FaultInjector::Initialize() {
    std::lock_guard<std::mutex> lock(m_rngMutex);
    if (m_initialized) return;

    // Seed configuration: REFIX_SIM_SEED
    char buf[64] = { 0 };
    if (GetEnvironmentVariableA("REFIX_SIM_SEED", buf, sizeof(buf)) && buf[0] != '\0') {
        m_seed = (uint32_t)strtoul(buf, nullptr, 0);
    } else {
        m_seed = 1337;
    }
    m_rng.seed(m_seed);

    // Direction filter: REFIX_SIM_DIR
    if (GetEnvironmentVariableA("REFIX_SIM_DIR", buf, sizeof(buf))) {
        std::string dirStr = buf;
        if (dirStr == "C2H" || dirStr == "CLIENT_TO_HOST") m_dirFilter = PacketDirection::CLIENT_TO_HOST;
        else if (dirStr == "H2C" || dirStr == "HOST_TO_CLIENT") m_dirFilter = PacketDirection::HOST_TO_CLIENT;
        else m_dirFilter = PacketDirection::ANY;
    }

    // Class configs from environment variables
    auto readEnvInt = [](const char* name, int defVal) -> int {
        char val[32];
        if (GetEnvironmentVariableA(name, val, sizeof(val))) {
            return atoi(val);
        }
        return defVal;
    };

    // Generic fallbacks
    int simDrop = readEnvInt("REFIX_SIM_DROP", 0);
    int simDup = readEnvInt("REFIX_SIM_DUP", 0);
    int simReorder = readEnvInt("REFIX_SIM_REORDER", 0);
    int simDelayMin = readEnvInt("REFIX_SIM_DELAY_MIN", 0);
    int simDelayMax = readEnvInt("REFIX_SIM_DELAY_MAX", 0);

    // Handshake
    m_configHandshake.dropRate = readEnvInt("REFIX_SIM_DROP_HANDSHAKE", 0);
    m_configHandshake.dropCount = readEnvInt("REFIX_SIM_DROP_COUNT_HANDSHAKE", 0);
    m_configHandshake.dupRate = readEnvInt("REFIX_SIM_DUP_HANDSHAKE", 0);
    m_configHandshake.delayMinMs = readEnvInt("REFIX_SIM_DELAY_HANDSHAKE_MIN", 0);
    m_configHandshake.delayMaxMs = readEnvInt("REFIX_SIM_DELAY_HANDSHAKE_MAX", 0);

    // Handshake ACK
    m_configHandshakeAck.dropRate = readEnvInt("REFIX_SIM_DROP_HANDSHAKE_ACK", 0);
    m_configHandshakeAck.dropCount = readEnvInt("REFIX_SIM_DROP_COUNT_HANDSHAKE_ACK", 0);
    m_configHandshakeAck.dupRate = readEnvInt("REFIX_SIM_DUP_HANDSHAKE_ACK", 0);
    m_configHandshakeAck.delayMinMs = readEnvInt("REFIX_SIM_DELAY_HANDSHAKE_ACK_MIN", 0);
    m_configHandshakeAck.delayMaxMs = readEnvInt("REFIX_SIM_DELAY_HANDSHAKE_ACK_MAX", 0);

    // Data
    m_configData.dropRate = readEnvInt("REFIX_SIM_DROP_DATA", simDrop);
    m_configData.dropCount = readEnvInt("REFIX_SIM_DROP_COUNT_DATA", 0);
    m_configData.dupRate = readEnvInt("REFIX_SIM_DUP_DATA", simDup);
    m_configData.reorderRate = readEnvInt("REFIX_SIM_REORDER_DATA", simReorder);
    m_configData.delayMinMs = readEnvInt("REFIX_SIM_DELAY_DATA_MIN", simDelayMin);
    m_configData.delayMaxMs = readEnvInt("REFIX_SIM_DELAY_DATA_MAX", simDelayMax);

    // Data ACK
    m_configDataAck.dropRate = readEnvInt("REFIX_SIM_DROP_DATA_ACK", 0);
    m_configDataAck.dropCount = readEnvInt("REFIX_SIM_DROP_COUNT_DATA_ACK", 0);
    m_configDataAck.dupRate = readEnvInt("REFIX_SIM_DUP_DATA_ACK", 0);
    m_configDataAck.reorderRate = readEnvInt("REFIX_SIM_REORDER_DATA_ACK", 0);
    m_configDataAck.delayMinMs = readEnvInt("REFIX_SIM_DELAY_DATA_ACK_MIN", 0);
    m_configDataAck.delayMaxMs = readEnvInt("REFIX_SIM_DELAY_DATA_ACK_MAX", 0);

    m_initialized = true;
}

void FaultInjector::ResetStats() {
    m_statsHandshake.Reset();
    m_statsHandshakeAck.Reset();
    m_statsData.Reset();
    m_statsDataAck.Reset();
    m_statsOther.Reset();
    for (int d = 0; d < 2; d++) {
        m_statsHandshakeDir[d].Reset();
        m_statsHandshakeAckDir[d].Reset();
        m_statsDataDir[d].Reset();
        m_statsDataAckDir[d].Reset();
        m_statsOtherDir[d].Reset();
    }
    std::lock_guard<std::mutex> lock(m_heldMutex);
    m_heldPacketsData.clear();
    m_heldPacketsAck.clear();
}

void FaultInjector::SetSeed(uint32_t seed) {
    std::lock_guard<std::mutex> lock(m_rngMutex);
    m_seed = seed;
    m_rng.seed(m_seed);
}

static PacketClass NormalizePacketClass(PacketClass pClass) {
    int val = (int)pClass;
    if (val == 6) return PacketClass::DATA;
    if (val == 7) return PacketClass::HANDSHAKE;
    if (val == 8) return PacketClass::HANDSHAKE_ACK;
    if (val == 9) return PacketClass::DATA_ACK;
    return pClass;
}

void FaultInjector::SetDropRate(PacketClass pClass, int percent) {
    pClass = NormalizePacketClass(pClass);
    switch (pClass) {
        case PacketClass::HANDSHAKE: m_configHandshake.dropRate = percent; break;
        case PacketClass::HANDSHAKE_ACK: m_configHandshakeAck.dropRate = percent; break;
        case PacketClass::DATA: m_configData.dropRate = percent; break;
        case PacketClass::DATA_ACK: m_configDataAck.dropRate = percent; break;
        default: break;
    }
}

void FaultInjector::SetDropCount(PacketClass pClass, int count) {
    pClass = NormalizePacketClass(pClass);
    switch (pClass) {
        case PacketClass::HANDSHAKE: m_configHandshake.dropCount = count; break;
        case PacketClass::HANDSHAKE_ACK: m_configHandshakeAck.dropCount = count; break;
        case PacketClass::DATA: m_configData.dropCount = count; break;
        case PacketClass::DATA_ACK: m_configDataAck.dropCount = count; break;
        default: break;
    }
}

void FaultInjector::SetDuplicateRate(PacketClass pClass, int percent) {
    pClass = NormalizePacketClass(pClass);
    switch (pClass) {
        case PacketClass::HANDSHAKE: m_configHandshake.dupRate = percent; break;
        case PacketClass::HANDSHAKE_ACK: m_configHandshakeAck.dupRate = percent; break;
        case PacketClass::DATA: m_configData.dupRate = percent; break;
        case PacketClass::DATA_ACK: m_configDataAck.dupRate = percent; break;
        default: break;
    }
}

void FaultInjector::SetReorderRate(PacketClass pClass, int percent) {
    pClass = NormalizePacketClass(pClass);
    switch (pClass) {
        case PacketClass::DATA: m_configData.reorderRate = percent; break;
        case PacketClass::DATA_ACK: m_configDataAck.reorderRate = percent; break;
        default: break;
    }
}

void FaultInjector::SetDelay(PacketClass pClass, int minMs, int maxMs) {
    pClass = NormalizePacketClass(pClass);
    switch (pClass) {
        case PacketClass::HANDSHAKE: m_configHandshake.delayMinMs = minMs; m_configHandshake.delayMaxMs = maxMs; break;
        case PacketClass::HANDSHAKE_ACK: m_configHandshakeAck.delayMinMs = minMs; m_configHandshakeAck.delayMaxMs = maxMs; break;
        case PacketClass::DATA: m_configData.delayMinMs = minMs; m_configData.delayMaxMs = maxMs; break;
        case PacketClass::DATA_ACK: m_configDataAck.delayMinMs = minMs; m_configDataAck.delayMaxMs = maxMs; break;
        default: break;
    }
}

void FaultInjector::SetDirectionFilter(PacketDirection dir) {
    m_dirFilter = dir;
}

bool FaultInjector::ShouldProcessDirection(PacketDirection dir) const {
    if (m_dirFilter == PacketDirection::ANY) return true;
    return m_dirFilter == dir;
}

PacketClassStats& FaultInjector::GetStatsInternal(PacketClass pClass) {
    pClass = NormalizePacketClass(pClass);
    switch (pClass) {
        case PacketClass::HANDSHAKE: return m_statsHandshake;
        case PacketClass::HANDSHAKE_ACK: return m_statsHandshakeAck;
        case PacketClass::DATA: return m_statsData;
        case PacketClass::DATA_ACK: return m_statsDataAck;
        default: return m_statsOther;
    }
}

PacketClassStats* FaultInjector::GetDirStatsInternal(PacketClass pClass, PacketDirection dir) {
    pClass = NormalizePacketClass(pClass);
    int idx = (dir == PacketDirection::CLIENT_TO_HOST) ? 0 : ((dir == PacketDirection::HOST_TO_CLIENT) ? 1 : -1);
    if (idx < 0) return nullptr;
    switch (pClass) {
        case PacketClass::HANDSHAKE: return &m_statsHandshakeDir[idx];
        case PacketClass::HANDSHAKE_ACK: return &m_statsHandshakeAckDir[idx];
        case PacketClass::DATA: return &m_statsDataDir[idx];
        case PacketClass::DATA_ACK: return &m_statsDataAckDir[idx];
        default: return &m_statsOtherDir[idx];
    }
}

const PacketClassStats& FaultInjector::GetStats(PacketClass pClass) const {
    switch (pClass) {
        case PacketClass::HANDSHAKE: return m_statsHandshake;
        case PacketClass::HANDSHAKE_ACK: return m_statsHandshakeAck;
        case PacketClass::DATA: return m_statsData;
        case PacketClass::DATA_ACK: return m_statsDataAck;
        default: return m_statsOther;
    }
}

const PacketClassStats& FaultInjector::GetStats(PacketClass pClass, PacketDirection dir) const {
    int idx = (dir == PacketDirection::CLIENT_TO_HOST) ? 0 : ((dir == PacketDirection::HOST_TO_CLIENT) ? 1 : -1);
    if (idx < 0) return GetStats(pClass);
    switch (pClass) {
        case PacketClass::HANDSHAKE: return m_statsHandshakeDir[idx];
        case PacketClass::HANDSHAKE_ACK: return m_statsHandshakeAckDir[idx];
        case PacketClass::DATA: return m_statsDataDir[idx];
        case PacketClass::DATA_ACK: return m_statsDataAckDir[idx];
        default: return m_statsOtherDir[idx];
    }
}

void FaultInjector::FlushHeldPackets() {
    std::unique_lock<std::mutex> hLock(m_heldMutex);
    for (auto& held : m_heldPacketsData) {
        held.sendFn(held.data.data(), held.data.size());
        m_statsData.delivered.fetch_add(1);
    }
    m_heldPacketsData.clear();
    for (auto& held : m_heldPacketsAck) {
        held.sendFn(held.data.data(), held.data.size());
        m_statsDataAck.delivered.fetch_add(1);
    }
    m_heldPacketsAck.clear();
}

bool FaultInjector::ProcessSend(PacketClass pClass,
                               PacketDirection dir,
                               const uint8_t* data,
                               size_t len,
                               std::function<void(const uint8_t*, size_t)> sendFn) {
    PacketClassStats& stats = GetStatsInternal(pClass);
    PacketClassStats* dirStats = GetDirStatsInternal(pClass, dir);
    stats.sent.fetch_add(1);
    if (dirStats) dirStats->sent.fetch_add(1);

    if (!ShouldProcessDirection(dir)) {
        sendFn(data, len);
        stats.delivered.fetch_add(1);
        if (dirStats) dirStats->delivered.fetch_add(1);
        return true;
    }

    ClassConfig* cfg = nullptr;
    switch (pClass) {
        case PacketClass::HANDSHAKE: cfg = &m_configHandshake; break;
        case PacketClass::HANDSHAKE_ACK: cfg = &m_configHandshakeAck; break;
        case PacketClass::DATA: cfg = &m_configData; break;
        case PacketClass::DATA_ACK: cfg = &m_configDataAck; break;
        default: break;
    }

    if (!cfg) {
        sendFn(data, len);
        stats.delivered.fetch_add(1);
        if (dirStats) dirStats->delivered.fetch_add(1);
        return true;
    }

    // 1. Check deterministic drop count
    if (cfg->dropCount > 0) {
        cfg->dropCount--;
        stats.dropped.fetch_add(1);
        if (dirStats) dirStats->dropped.fetch_add(1);
        return false;
    }

    // 2. Check drop rate
    int roll = 0;
    {
        std::lock_guard<std::mutex> lock(m_rngMutex);
        roll = (int)(m_rng() % 100);
    }
    if (cfg->dropRate > 0 && roll < cfg->dropRate) {
        stats.dropped.fetch_add(1);
        if (dirStats) dirStats->dropped.fetch_add(1);
        return false;
    }

    // 3. Check forced reorder
    if (cfg->reorderRate > 0 && (pClass == PacketClass::DATA || pClass == PacketClass::DATA_ACK)) {
        int reorderRoll = 0;
        {
            std::lock_guard<std::mutex> lock(m_rngMutex);
            reorderRoll = (int)(m_rng() % 100);
        }

        std::vector<HeldPacket>& heldQueue = (pClass == PacketClass::DATA) ? m_heldPacketsData : m_heldPacketsAck;
        std::unique_lock<std::mutex> hLock(m_heldMutex);

        if (!heldQueue.empty()) {
            // Deliver current packet FIRST
            sendFn(data, len);
            stats.delivered.fetch_add(1);
            if (dirStats) dirStats->delivered.fetch_add(1);

            // Now release held packets (they arrive AFTER current -> forced reorder!)
            for (auto& held : heldQueue) {
                held.sendFn(held.data.data(), held.data.size());
                stats.delivered.fetch_add(1);
                if (dirStats) dirStats->delivered.fetch_add(1);
            }
            heldQueue.clear();
            return true;
        } else if (reorderRoll < cfg->reorderRate) {
            // Hold this packet for the next one
            HeldPacket hp;
            hp.data.assign(data, data + len);
            hp.sendFn = sendFn;
            heldQueue.push_back(std::move(hp));
            stats.reordered.fetch_add(1);
            if (dirStats) dirStats->reordered.fetch_add(1);
            return true;
        }
    }

    // 4. Delay handling
    if (cfg->delayMaxMs > 0) {
        int delayMs = cfg->delayMinMs;
        if (cfg->delayMaxMs > cfg->delayMinMs) {
            std::lock_guard<std::mutex> lock(m_rngMutex);
            delayMs += (int)(m_rng() % (cfg->delayMaxMs - cfg->delayMinMs + 1));
        }

        std::vector<uint8_t> copyData(data, data + len);
        stats.delayed.fetch_add(1);
        if (dirStats) dirStats->delayed.fetch_add(1);

        std::thread([this, &stats, dirStats, copyData, sendFn, delayMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            sendFn(copyData.data(), copyData.size());
            stats.delivered.fetch_add(1);
            if (dirStats) dirStats->delivered.fetch_add(1);
        }).detach();

        // Check duplicate even with delay
        int dupRoll = 0;
        {
            std::lock_guard<std::mutex> lock(m_rngMutex);
            dupRoll = (int)(m_rng() % 100);
        }
        if (cfg->dupRate > 0 && dupRoll < cfg->dupRate) {
            stats.duplicated.fetch_add(1);
            if (dirStats) dirStats->duplicated.fetch_add(1);
            std::thread([this, &stats, dirStats, copyData, sendFn, delayMs]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs + 10));
                sendFn(copyData.data(), copyData.size());
                stats.delivered.fetch_add(1);
                if (dirStats) dirStats->delivered.fetch_add(1);
            }).detach();
        }
        return true;
    }

    // 5. Send packet
    sendFn(data, len);
    stats.delivered.fetch_add(1);
    if (dirStats) dirStats->delivered.fetch_add(1);

    // 6. Check duplicate
    int dupRoll = 0;
    {
        std::lock_guard<std::mutex> lock(m_rngMutex);
        dupRoll = (int)(m_rng() % 100);
    }
    if (cfg->dupRate > 0 && dupRoll < cfg->dupRate) {
        stats.duplicated.fetch_add(1);
        if (dirStats) dirStats->duplicated.fetch_add(1);
        sendFn(data, len);
        stats.delivered.fetch_add(1);
        if (dirStats) dirStats->delivered.fetch_add(1);
    }

    return true;
}

void FaultInjector::PrintStats() const {
    auto printClass = [this](const char* name, PacketClass pClass) {
        const PacketClassStats& total = GetStats(pClass);
        const PacketClassStats& c2h = GetStats(pClass, PacketDirection::CLIENT_TO_HOST);
        const PacketClassStats& h2c = GetStats(pClass, PacketDirection::HOST_TO_CLIENT);
        std::cout << "  Class " << name << " [TOTAL]: sent=" << total.sent.load()
                  << ", dropped=" << total.dropped.load()
                  << ", duplicated=" << total.duplicated.load()
                  << ", delayed=" << total.delayed.load()
                  << ", reordered=" << total.reordered.load()
                  << ", delivered=" << total.delivered.load() << std::endl;
        std::cout << "    -> C2H: sent=" << c2h.sent.load() << ", drop=" << c2h.dropped.load()
                  << ", dup=" << c2h.duplicated.load() << ", deliv=" << c2h.delivered.load() << std::endl;
        std::cout << "    -> H2C: sent=" << h2c.sent.load() << ", drop=" << h2c.dropped.load()
                  << ", dup=" << h2c.duplicated.load() << ", deliv=" << h2c.delivered.load() << std::endl;
    };

    std::cout << "=== REFIX FAULT INJECTOR STATS (seed=" << m_seed << ") ===" << std::endl;
    printClass("HANDSHAKE", PacketClass::HANDSHAKE);
    printClass("HANDSHAKE_ACK", PacketClass::HANDSHAKE_ACK);
    printClass("DATA", PacketClass::DATA);
    printClass("DATA_ACK", PacketClass::DATA_ACK);
    std::cout << "==========================================================" << std::endl;
}

} // namespace ReFix
