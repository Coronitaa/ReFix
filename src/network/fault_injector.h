#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <random>

namespace ReFix {

enum class PacketClass {
    HANDSHAKE = 7,
    HANDSHAKE_ACK = 8,
    DATA = 6,
    DATA_ACK = 9,
    OTHER = 0
};

enum class PacketDirection {
    CLIENT_TO_HOST,
    HOST_TO_CLIENT,
    ANY
};

struct PacketClassStats {
    std::atomic<uint64_t> sent{ 0 };
    std::atomic<uint64_t> dropped{ 0 };
    std::atomic<uint64_t> duplicated{ 0 };
    std::atomic<uint64_t> delayed{ 0 };
    std::atomic<uint64_t> reordered{ 0 };
    std::atomic<uint64_t> delivered{ 0 };

    void Reset() {
        sent = 0;
        dropped = 0;
        duplicated = 0;
        delayed = 0;
        reordered = 0;
        delivered = 0;
    }
};

class FaultInjector {
public:
    static FaultInjector& Get();

    void Initialize();
    void ResetStats();
    void SetSeed(uint32_t seed);
    uint32_t GetSeed() const { return m_seed; }

    // Configuration overrides for programmatic tests
    void SetDropRate(PacketClass pClass, int percent);
    void SetDropCount(PacketClass pClass, int count);
    void SetDuplicateRate(PacketClass pClass, int percent);
    void SetReorderRate(PacketClass pClass, int percent);
    void SetDelay(PacketClass pClass, int minMs, int maxMs);
    void SetDirectionFilter(PacketDirection dir);

    // Main injection entrypoint
    // Returns true if packet was delivered or scheduled, false if dropped.
    bool ProcessSend(PacketClass pClass,
                     PacketDirection dir,
                     const uint8_t* data,
                     size_t len,
                     std::function<void(const uint8_t*, size_t)> sendFn);

    const PacketClassStats& GetStats(PacketClass pClass) const;
    const PacketClassStats& GetStats(PacketClass pClass, PacketDirection dir) const;
    void FlushHeldPackets();
    void PrintStats() const;

private:
    FaultInjector();
    ~FaultInjector() = default;

    bool ShouldProcessDirection(PacketDirection dir) const;
    PacketClassStats& GetStatsInternal(PacketClass pClass);
    PacketClassStats* GetDirStatsInternal(PacketClass pClass, PacketDirection dir);

    uint32_t m_seed{ 1337 };
    std::mt19937 m_rng;
    std::mutex m_rngMutex;
    bool m_initialized{ false };

    PacketDirection m_dirFilter{ PacketDirection::ANY };

    // Per-class settings
    struct ClassConfig {
        int dropRate{ 0 };       // 0..100%
        int dropCount{ 0 };      // deterministic exact drop count
        int dupRate{ 0 };        // 0..100%
        int reorderRate{ 0 };    // 0..100%
        int delayMinMs{ 0 };
        int delayMaxMs{ 0 };
    };

    ClassConfig m_configHandshake;
    ClassConfig m_configHandshakeAck;
    ClassConfig m_configData;
    ClassConfig m_configDataAck;

    // Per-class stats (Aggregates)
    PacketClassStats m_statsHandshake;
    PacketClassStats m_statsHandshakeAck;
    PacketClassStats m_statsData;
    PacketClassStats m_statsDataAck;
    PacketClassStats m_statsOther;

    // Directional stats breakdown: [0] = CLIENT_TO_HOST, [1] = HOST_TO_CLIENT
    PacketClassStats m_statsHandshakeDir[2];
    PacketClassStats m_statsHandshakeAckDir[2];
    PacketClassStats m_statsDataDir[2];
    PacketClassStats m_statsDataAckDir[2];
    PacketClassStats m_statsOtherDir[2];

    // Reorder buffer per packet class
    struct HeldPacket {
        std::vector<uint8_t> data;
        std::function<void(const uint8_t*, size_t)> sendFn;
    };
    std::vector<HeldPacket> m_heldPacketsData;
    std::vector<HeldPacket> m_heldPacketsAck;
    std::mutex m_heldMutex;
};

} // namespace ReFix
