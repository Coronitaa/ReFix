#include "message_tracker.h"
#include <iostream>

namespace ReFix {

MessageTracker& MessageTracker::Get() {
    static MessageTracker s_instance;
    return s_instance;
}

uint64_t MessageTracker::TrackAlloc(void* msgPtr, size_t size, const char* site) {
    if (!msgPtr) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t id = m_nextId++;
    TrackedMessageRecord rec;
    rec.id = id;
    rec.address = msgPtr;
    rec.size = size;
    rec.site = site ? site : "unknown";
    rec.releaseCount = 0;
    rec.released = false;

    m_records[msgPtr] = rec;
    m_totalAllocated.fetch_add(1);
    return id;
}

void MessageTracker::TrackRelease(void* msgPtr) {
    if (!msgPtr) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_records.find(msgPtr);
    if (it == m_records.end()) {
        // Unknown pointer released
        m_doubleReleaseCount.fetch_add(1);
        std::cerr << "[MessageTracker] ERROR: Untracked or double-released message at " << msgPtr << std::endl;
        return;
    }

    it->second.releaseCount++;
    if (it->second.releaseCount > 1) {
        m_doubleReleaseCount.fetch_add(1);
        std::cerr << "[MessageTracker] ERROR: DOUBLE RELEASE detected! id=" << it->second.id
                  << " addr=" << msgPtr << " site=" << it->second.site
                  << " count=" << it->second.releaseCount << std::endl;
    }

    it->second.released = true;
    m_totalReleased.fetch_add(1);
    m_records.erase(it);
}

int MessageTracker::GetReleaseCount(void* msgPtr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_records.find(msgPtr);
    if (it != m_records.end()) {
        return it->second.releaseCount;
    }
    return 1; // Already released and erased
}

size_t MessageTracker::GetActiveCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_records.size();
}

size_t MessageTracker::GetLeakCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_records.size();
}

void MessageTracker::DumpStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "=== MESSAGE TRACKER STATS ===" << std::endl;
    std::cout << "  Total Allocated: " << m_totalAllocated.load() << std::endl;
    std::cout << "  Total Released:  " << m_totalReleased.load() << std::endl;
    std::cout << "  Active / Leaked: " << m_records.size() << std::endl;
    std::cout << "  Double Releases: " << m_doubleReleaseCount.load() << std::endl;
    if (!m_records.empty()) {
        std::cout << "  Leaked records details:" << std::endl;
        for (const auto& pair : m_records) {
            std::cout << "    id=" << pair.second.id << " addr=" << pair.second.address
                      << " size=" << pair.second.size << " site=" << pair.second.site << std::endl;
        }
    }
    std::cout << "=============================" << std::endl;
}

void MessageTracker::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records.clear();
    m_nextId = 1;
    m_totalAllocated = 0;
    m_totalReleased = 0;
    m_doubleReleaseCount = 0;
}

} // namespace ReFix
