#include "panorama_adapter.h"
#include <windows.h>

namespace refix::panorama {

PanoramaAdapter& PanoramaAdapter::Get() {
    static PanoramaAdapter s_instance;
    return s_instance;
}

PanoramaAdapter::PanoramaAdapter() {
    refix::GetRefixServices().panorama = this;
}

void PanoramaAdapter::RegisterScriptInjection(const char* target_script, const char* injected_code) {
    if (!target_script || !injected_code) return;
    std::lock_guard<std::mutex> lock(m_mu);
    m_injections[target_script] = injected_code;
}

void PanoramaAdapter::RegisterOnScriptRun(PanoramaScriptHookFn fn) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_runHooks.push_back(std::move(fn));
}

void PanoramaAdapter::RunScript(const char* script) {
    if (!script) return;
    // Invoke registered hooks
    std::vector<PanoramaScriptHookFn> hooks;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        hooks = m_runHooks;
    }
    for (const auto& h : hooks) {
        if (h) h();
    }
}

bool PanoramaAdapter::InitializeHooks() {
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_initialized) return true;
    m_initialized = true;
    return true;
}

std::string PanoramaAdapter::GetInjectedScript(const char* script_name) {
    if (!script_name) return "";
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_injections.find(script_name);
    if (it != m_injections.end()) {
        return it->second;
    }
    return "";
}

} // namespace refix::panorama
