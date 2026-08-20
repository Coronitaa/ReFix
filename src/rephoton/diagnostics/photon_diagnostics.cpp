#include "photon_diagnostics.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <regex>

namespace ReFix::Photon::Diagnostics {

    static const char* GetChannelPrefix(LogChannel ch) {
        switch (ch) {
            case LogChannel::Auth:      return "[RePhoton/Auth]";
            case LogChannel::Transport: return "[RePhoton/Transport]";
            case LogChannel::Realtime:  return "[RePhoton/Realtime]";
            case LogChannel::Room:      return "[RePhoton/Room]";
            case LogChannel::Fusion:    return "[RePhoton/Fusion]";
            case LogChannel::Voice:     return "[RePhoton/Voice]";
            case LogChannel::Chat:      return "[RePhoton/Chat]";
            default:                    return "[RePhoton]";
        }
    }

    static const char* GetSeverityPrefix(LogSeverity sev) {
        switch (sev) {
            case LogSeverity::Debug:   return "DEBUG";
            case LogSeverity::Warning: return "WARN";
            case LogSeverity::Error:   return "ERROR";
            default:                   return "INFO";
        }
    }

    DiagnosticsEngine& DiagnosticsEngine::Instance() {
        static DiagnosticsEngine s_instance;
        return s_instance;
    }

    DiagnosticsEngine::DiagnosticsEngine() {
        for (int i = 0; i < 8; ++i) m_channelFlags[i] = true;
    }

    void DiagnosticsEngine::SetChannelEnabled(LogChannel ch, bool enabled) {
        int idx = static_cast<int>(ch);
        if (idx >= 0 && idx < 8) {
            m_channelFlags[idx] = enabled;
        }
    }

    bool DiagnosticsEngine::IsChannelEnabled(LogChannel ch) const {
        int idx = static_cast<int>(ch);
        if (idx >= 0 && idx < 8) {
            return m_channelFlags[idx];
        }
        return false;
    }

    std::string DiagnosticsEngine::RedactSensitive(const std::string& input) const {
        if (!m_redactAppIds || input.empty()) return input;

        // Regex for UUID/GUID pattern: 8-4-4-4-12 hex chars
        // Replace with [REDACTED_APPID]
        static const std::regex guidRegex("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}");
        return std::regex_replace(input, guidRegex, "[REDACTED_APPID]");
    }

    void DiagnosticsEngine::Log(LogChannel ch, LogSeverity sev, const char* fmt, ...) {
        if (!m_enabled || !IsChannelEnabled(ch)) return;

        va_list args;
        va_start(args, fmt);
        char rawBuf[2048];
        vsprintf_s(rawBuf, sizeof(rawBuf), fmt, args);
        va_end(args);

        std::string processed = RedactSensitive(rawBuf);

        std::lock_guard<std::mutex> lock(m_logMutex);

        HWND hConsole = GetConsoleWindow();
        if (hConsole) {
            printf("%s [%s] %s\n", GetChannelPrefix(ch), GetSeverityPrefix(sev), processed.c_str());
            fflush(stdout);
        }

        // Output to DebugView / Visual Studio debugger
        char dbgBuf[2560];
        sprintf_s(dbgBuf, sizeof(dbgBuf), "%s [%s] %s\n", GetChannelPrefix(ch), GetSeverityPrefix(sev), processed.c_str());
        OutputDebugStringA(dbgBuf);

        // Persistent file log
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string logPath(exePath);
        size_t pos = logPath.find_last_of("\\/");
        if (pos != std::string::npos) logPath = logPath.substr(0, pos + 1) + "RePhoton.log";

        FILE* f = nullptr;
        fopen_s(&f, logPath.c_str(), "a");
        if (f) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s [%s] %s\n",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                    GetChannelPrefix(ch), GetSeverityPrefix(sev), processed.c_str());
            fclose(f);
        }
    }

    void DiagnosticsEngine::RecordPacketSent(uint64_t bytes) {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.packetsSent++;
        m_metrics.bytesSent += bytes;
    }

    void DiagnosticsEngine::RecordPacketReceived(uint64_t bytes) {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.packetsReceived++;
        m_metrics.bytesReceived += bytes;
    }

    void DiagnosticsEngine::UpdateRoundTripTime(uint32_t rttMs) {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.roundTripTimeMs = rttMs;
    }

    void DiagnosticsEngine::RecordPacketLoss(float lossPercent) {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.packetLossPercent = lossPercent;
    }

    void DiagnosticsEngine::RecordReconnectAttempt() {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.reconnectAttempts++;
    }

    NetworkMetrics DiagnosticsEngine::GetMetrics() const {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        return m_metrics;
    }

    void DiagnosticsEngine::ResetMetrics() {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics = NetworkMetrics();
    }

    void LogInfo(LogChannel ch, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[2048];
        vsprintf_s(buf, sizeof(buf), fmt, args);
        va_end(args);
        DiagnosticsEngine::Instance().Log(ch, LogSeverity::Info, "%s", buf);
    }

    void LogWarn(LogChannel ch, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[2048];
        vsprintf_s(buf, sizeof(buf), fmt, args);
        va_end(args);
        DiagnosticsEngine::Instance().Log(ch, LogSeverity::Warning, "%s", buf);
    }

    void LogError(LogChannel ch, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[2048];
        vsprintf_s(buf, sizeof(buf), fmt, args);
        va_end(args);
        DiagnosticsEngine::Instance().Log(ch, LogSeverity::Error, "%s", buf);
    }

    void LogDebug(LogChannel ch, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[2048];
        vsprintf_s(buf, sizeof(buf), fmt, args);
        va_end(args);
        DiagnosticsEngine::Instance().Log(ch, LogSeverity::Debug, "%s", buf);
    }

} // namespace ReFix::Photon::Diagnostics
