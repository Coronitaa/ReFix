// =============================================================================
// ReFix EOS Online v2 - Thread-Safe Generic Callback & Notification Dispatcher
// =============================================================================
#pragma once

#include "eos_types.h"
#include <vector>
#include <mutex>
#include <functional>
#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace ReFixEOS {

struct QueuedCallbackItem {
    EOS_CallbackFn callbackFn;
    void* payloadData;
    size_t payloadSize;
};

struct NotificationSubscription {
    EOS_NotificationId notificationId;
    int32_t eventType;
    EOS_CallbackFn callbackFn;
    void* clientData;
};

class CallbackManager {
public:
    static CallbackManager& Get();

    // Queues an asynchronous completion callback to be dispatched during EOS_Platform_Tick
    template<typename T>
    void QueueCallback(void* fn, const T& data) {
        if (!fn) return;
        std::lock_guard<std::mutex> lock(m_queueMutex);
        QueuedCallbackItem item;
        item.callbackFn = (EOS_CallbackFn)fn;
        item.payloadSize = sizeof(T);
        item.payloadData = malloc(sizeof(T));
        if (item.payloadData) {
            memcpy(item.payloadData, &data, sizeof(T));
            m_queue.push_back(item);
        }
    }

    // Drains all pending callbacks (invoked during EOS_Platform_Tick)
    size_t FlushCallbacks();

    // Notification Registry
    EOS_NotificationId AddNotification(int32_t eventType, void* clientData, void* callbackFn);
    void RemoveNotification(EOS_NotificationId notifId);

    template<typename T>
    void DispatchNotification(int32_t eventType, const T& data) {
        std::lock_guard<std::mutex> lock(m_notifyMutex);
        for (const auto& sub : m_subscriptions) {
            if (sub.eventType == eventType && sub.callbackFn) {
                T copyData = data;
                copyData.ClientData = sub.clientData;
                QueueCallback((void*)sub.callbackFn, copyData);
            }
        }
    }

    // Reset state for testing
    void Reset();

private:
    CallbackManager() = default;
    ~CallbackManager();

    std::mutex m_queueMutex;
    std::vector<QueuedCallbackItem> m_queue;

    std::mutex m_notifyMutex;
    uint64_t m_nextNotificationId = 1000;
    std::vector<NotificationSubscription> m_subscriptions;
};

} // namespace ReFixEOS
