#pragma once

#include "../core/photon_types.h"
#include <string>
#include <mutex>
#include <cstdint>

namespace ReFix::Photon::Diagnostics {

    enum class LogChannel {
        General,
        Auth,
        Transport,
        Realtime,
        Room,
        Fusion,
        Voice,
        Chat
    };

    enum class LogSeverity {
        Debug,
        Info,
        Warning,
        Error
    };

    class DiagnosticsEngine {
    public:
        static DiagnosticsEngine& Instance();

        void SetLoggingEnabled(bool enabled) { m_enabled = enabled; }
        bool IsLoggingEnabled() const { return m_enabled; }

        void SetRedactAppIds(bool redact) { m_redactAppIds = redact; }
        bool ShouldRedactAppIds() const { return m_redactAppIds; }

        void SetChannelEnabled(LogChannel ch, bool enabled);
        bool IsChannelEnabled(LogChannel ch) const;

        void Log(LogChannel ch, LogSeverity sev, const char* fmt, ...);

        // Network Metrics Tracking
        void RecordPacketSent(uint64_t bytes);
        void RecordPacketReceived(uint64_t bytes);
        void UpdateRoundTripTime(uint32_t rttMs);
        void RecordPacketLoss(float lossPercent);
        void RecordReconnectAttempt();

        NetworkMetrics GetMetrics() const;
        void ResetMetrics();

        std::string RedactSensitive(const std::string& input) const;

    private:
        DiagnosticsEngine();

        bool m_enabled = true;
        bool m_redactAppIds = true;
        bool m_channelFlags[8] = { true, true, true, true, true, true, true, true };

        mutable std::mutex m_logMutex;
        mutable std::mutex m_metricsMutex;
        NetworkMetrics m_metrics;
    };

    // Global helper macros/functions
    void LogInfo(LogChannel ch, const char* fmt, ...);
    void LogWarn(LogChannel ch, const char* fmt, ...);
    void LogError(LogChannel ch, const char* fmt, ...);
    void LogDebug(LogChannel ch, const char* fmt, ...);

} // namespace ReFix::Photon::Diagnostics
