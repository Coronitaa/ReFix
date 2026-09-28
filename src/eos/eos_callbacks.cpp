#include "eos_connect.h"
// =============================================================================
// ReFix EOS Online v2 - Callback & Notification Dispatcher Implementation
// =============================================================================
#include "eos_callbacks.h"

namespace ReFixEOS {

CallbackManager& CallbackManager::Get() {
    static CallbackManager s_instance;
    return s_instance;
}

CallbackManager::~CallbackManager() {
    Reset();
}

void CallbackManager::Reset() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    for (auto& item : m_queue) {
        if (item.payloadData) free(item.payloadData);
    }
    m_queue.clear();

    std::lock_guard<std::mutex> notifyLock(m_notifyMutex);
    m_subscriptions.clear();
}

size_t CallbackManager::FlushCallbacks() {
    std::vector<QueuedCallbackItem> localQueue;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_queue.empty()) return 0;
        localQueue.swap(m_queue);
    }

    ReFixEOS::LogDiagnostic("CallbackManager::FlushCallbacks: Disagree/Dispatching %u callbacks", (uint32_t)localQueue.size());

    size_t executedCount = 0;
    for (const auto& item : localQueue) {
        if (item.callbackFn && item.payloadData) {
            item.callbackFn(item.payloadData);
            executedCount++;
        }
        if (item.payloadData) {
            free(item.payloadData);
        }
    }
    return executedCount;
}

EOS_NotificationId CallbackManager::AddNotification(int32_t eventType, void* clientData, void* callbackFn) {
    if (!callbackFn) return EOS_INVALID_NOTIFICATIONID;
    std::lock_guard<std::mutex> lock(m_notifyMutex);
    NotificationSubscription sub;
    sub.notificationId = (EOS_NotificationId)(++m_nextNotificationId);
    sub.eventType = eventType;
    sub.clientData = clientData;
    sub.callbackFn = (EOS_CallbackFn)callbackFn;
    m_subscriptions.push_back(sub);
    return sub.notificationId;
}

void CallbackManager::RemoveNotification(EOS_NotificationId notifId) {
    if (notifId == EOS_INVALID_NOTIFICATIONID) return;
    std::lock_guard<std::mutex> lock(m_notifyMutex);
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it) {
        if (it->notificationId == notifId) {
            m_subscriptions.erase(it);
            break;
        }
    }
}

} // namespace ReFixEOS
