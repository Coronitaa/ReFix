#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <map>
#include <string>

namespace ReFix {

struct TrackedMessageRecord {
    uint64_t id{ 0 };
    void* address{ nullptr };
    size_t size{ 0 };
    std::string site;
    int releaseCount{ 0 };
    bool released{ false };
};

class MessageTracker {
public:
    static MessageTracker& Get();

    uint64_t TrackAlloc(void* msgPtr, size_t size, const char* site);
    void TrackRelease(void* msgPtr);
    int GetReleaseCount(void* msgPtr);

    size_t GetActiveCount();
    size_t GetTotalAllocated() const { return m_totalAllocated.load(); }
    size_t GetTotalReleased() const { return m_totalReleased.load(); }
    size_t GetDoubleReleaseCount() const { return m_doubleReleaseCount.load(); }
    size_t GetLeakCount();

    void DumpStats();
    void Reset();

private:
    MessageTracker() = default;
    ~MessageTracker() = default;

    std::mutex m_mutex;
    uint64_t m_nextId{ 1 };
    std::map<void*, TrackedMessageRecord> m_records;

    std::atomic<size_t> m_totalAllocated{ 0 };
    std::atomic<size_t> m_totalReleased{ 0 };
    std::atomic<size_t> m_doubleReleaseCount{ 0 };
};

} // namespace ReFix
