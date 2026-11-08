// Valve Panorama UI Adapter: declarative script injection and event interception
#pragma once

#include "../../include/refix_services.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace refix::panorama {

class PanoramaAdapter : public refix::IPanorama {
public:
    static PanoramaAdapter& Get();

    PanoramaAdapter();
    virtual ~PanoramaAdapter() override = default;

    virtual void RegisterScriptInjection(const char* target_script, const char* injected_code) override;
    virtual void RegisterOnScriptRun(PanoramaScriptHookFn fn) override;
    virtual void RunScript(const char* script) override;

    bool InitializeHooks();
    std::string GetInjectedScript(const char* script_name);

private:
    std::mutex m_mu;
    std::unordered_map<std::string, std::string> m_injections;
    std::vector<PanoramaScriptHookFn> m_runHooks;
    bool m_initialized = false;
};

} // namespace refix::panorama
