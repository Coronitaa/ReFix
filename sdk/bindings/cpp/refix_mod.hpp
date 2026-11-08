#pragma once

#include "../../include/refix_mod_api.h"
#include <string>
#include <vector>
#include <functional>

namespace refix {

class Mod {
public:
    static void LogInfo(const std::string& msg) {
        if (s_api && s_api->log) {
            s_api->log(REFIX_LOG_INFO, msg.c_str());
        }
    }

    static void LogWarn(const std::string& msg) {
        if (s_api && s_api->log) {
            s_api->log(REFIX_LOG_WARN, msg.c_str());
        }
    }

    static void LogError(const std::string& msg) {
        if (s_api && s_api->log) {
            s_api->log(REFIX_LOG_ERROR, msg.c_str());
        }
    }

    static uint64_t GetTimeMs() {
        if (s_api && s_api->get_time_ms) {
            return s_api->get_time_ms();
        }
        return 0;
    }

    static bool StorageSet(const std::string& key, const std::vector<uint8_t>& data) {
        if (!s_api || !s_api->storage_write) return false;
        return s_api->storage_write(key.c_str(), data.data(), data.size()) == 0;
    }

    static bool StorageGet(const std::string& key, std::vector<uint8_t>& out_data) {
        if (!s_api || !s_api->storage_read) return false;
        uint8_t buf[4096];
        int32_t bytes = s_api->storage_read(key.c_str(), buf, sizeof(buf));
        if (bytes < 0) return false;
        out_data.assign(buf, buf + bytes);
        return true;
    }

    static bool ApplyPatch(const std::string& module_name,
                          uint32_t rva,
                          const std::vector<uint8_t>& expected,
                          const std::vector<uint8_t>& replacement) {
        if (!s_api || !s_api->apply_patch) return false;
        RefixDeclarativePatchDesc desc = {};
        desc.struct_size = sizeof(desc);
        desc.api_version = REFIX_MOD_API_VERSION;
        desc.target_module = module_name.c_str();
        desc.rva = rva;
        desc.expected_bytes = expected.data();
        desc.expected_bytes_len = expected.size();
        desc.patch_bytes = replacement.data();
        desc.patch_bytes_len = replacement.size();
        return s_api->apply_patch(&desc) == 0;
    }

    static bool NetSend(RefixPeerHandle peer, uint32_t channel, const std::vector<uint8_t>& data) {
        if (!s_api || !s_api->net_send) return false;
        return s_api->net_send(peer, channel, data.data(), data.size()) == 0;
    }

    static std::string GetPeerDisplayName(RefixPeerHandle peer) {
        if (!s_api || !s_api->net_get_peer_display_name) return "";
        char buf[128] = { 0 };
        s_api->net_get_peer_display_name(peer, buf, sizeof(buf));
        return std::string(buf);
    }

    static void SetHostApi(const RefixHostApi* api) {
        s_api = api;
    }

private:
    inline static const RefixHostApi* s_api = nullptr;
};

} // namespace refix
