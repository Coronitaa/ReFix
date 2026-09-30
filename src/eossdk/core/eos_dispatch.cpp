#include "eos_dispatch.h"
#include "refix_log.h"

namespace refix {

Dispatcher& Dispatcher::Get() { static Dispatcher d; return d; }

void Dispatcher::Post(std::function<void()> fn) {
    if (!fn) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(std::move(fn));
}

// A misbehaving title callback must not take the whole process down through our
// tick. MSVC forbids SEH in a frame that owns unwindable objects, so the guard
// lives in its own frame that only holds a raw pointer.
static void InvokeGuarded(std::function<void()>* fn) {
#if defined(_MSC_VER)
    __try { (*fn)(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        RFLOG(Core, "Dispatcher: exception inside a queued callback (ignored)");
    }
#else
    (*fn)();
#endif
}

void Dispatcher::PostAfter(uint32_t delayMs, std::function<void()> fn) {
    if (!fn) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timed.emplace_back(NowMs() + delayMs, std::move(fn));
}

size_t Dispatcher::Drain() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pending.swap(m_queue);
        const uint64_t now = NowMs();
        for (size_t i = 0; i < m_timed.size(); ) {
            if (m_timed[i].first <= now) {
                pending.push_back(std::move(m_timed[i].second));
                m_timed.erase(m_timed.begin() + i);
            } else {
                ++i;
            }
        }
    }
    for (auto& fn : pending) InvokeGuarded(&fn);
    return pending.size();
}

EOS_NotificationId Dispatcher::Register(NotifyKind kind, void* fn, void* clientData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    NotifyHandler h;
    h.Id = m_nextId++;
    h.Kind = kind;
    h.Fn = fn;
    h.ClientData = clientData;
    m_handlers.push_back(h);
    return h.Id;
}

void Dispatcher::Unregister(EOS_NotificationId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < m_handlers.size(); i++) {
        if (m_handlers[i].Id == id) { m_handlers.erase(m_handlers.begin() + i); return; }
    }
}

std::vector<NotifyHandler> Dispatcher::Handlers(NotifyKind kind) const {
    std::vector<NotifyHandler> out;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& h : m_handlers) if (h.Kind == kind && h.Fn) out.push_back(h);
    return out;
}

void Dispatcher::PostGenericCompletion(void* completionDelegate, void* clientData, ER result) {
    if (!completionDelegate) return;
    auto fn = (void (EOS_CALL*)(const void*))completionDelegate;
    Post([fn, clientData, result]() {
        // Large enough for any EOS callback info; the trailing bytes stay zero.
        struct alignas(16) GenericInfo { uint8_t bytes[512]; } info{};
        std::memset(&info, 0, sizeof(info));
        std::memcpy(info.bytes + 0, &result, sizeof(result));
        std::memcpy(info.bytes + 8, &clientData, sizeof(clientData));
        fn(&info);
    });
}

} // namespace refix
