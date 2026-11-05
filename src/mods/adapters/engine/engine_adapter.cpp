#include "engine_adapter.h"
#include "../../unreal_detect.h"
#include "../../refix_il2cpp.h"

#include <iostream>

namespace refix::mods {

const char* EngineTypeToString(GameEngineType type) {
    switch (type) {
        case GameEngineType::UnityMono:   return "Unity Mono";
        case GameEngineType::UnityIL2CPP: return "Unity IL2CPP";
        case GameEngineType::Unreal4:     return "Unreal Engine 4";
        case GameEngineType::Unreal5:     return "Unreal Engine 5";
        case GameEngineType::Godot:       return "Godot Engine";
        case GameEngineType::Source:      return "Source Engine";
        case GameEngineType::Source2:     return "Source 2";
        case GameEngineType::GameMaker:   return "GameMaker";
        case GameEngineType::Native:      return "Native";
        default:                          return "Unknown";
    }
}

// -----------------------------------------------------------------------------
// Unity Mono Adapter
// -----------------------------------------------------------------------------
class UnityMonoAdapter : public IEngineAdapter {
public:
    GameEngineType GetType() const override { return GameEngineType::UnityMono; }
    const char* GetName() const override { return "Unity Mono"; }

    bool Detect() override {
        hModule_ = GetModuleHandleA("mono-2.0-bdwgc.dll");
        if (!hModule_) {
            hModule_ = GetModuleHandleA("mono.dll");
        }
        return hModule_ != NULL;
    }

    bool Initialize() override {
        return hModule_ != NULL;
    }

    void Shutdown() override {
        hModule_ = NULL;
    }

    void* ResolveSymbol(const char* name) override {
        if (!hModule_ || !name) return nullptr;
        return reinterpret_cast<void*>(GetProcAddress(hModule_, name));
    }

private:
    HMODULE hModule_ = NULL;
};

// -----------------------------------------------------------------------------
// Unity IL2CPP Adapter
// -----------------------------------------------------------------------------
class UnityIL2CPPAdapter : public IEngineAdapter {
public:
    GameEngineType GetType() const override { return GameEngineType::UnityIL2CPP; }
    const char* GetName() const override { return "Unity IL2CPP"; }

    bool Detect() override {
        hModule_ = GetModuleHandleA("GameAssembly.dll");
        return hModule_ != NULL;
    }

    bool Initialize() override {
        return hModule_ != NULL;
    }

    void Shutdown() override {
        hModule_ = NULL;
    }

    void* ResolveSymbol(const char* name) override {
        if (!hModule_ || !name) return nullptr;
        return reinterpret_cast<void*>(GetProcAddress(hModule_, name));
    }

private:
    HMODULE hModule_ = NULL;
};

// -----------------------------------------------------------------------------
// Unreal Engine Adapter
// -----------------------------------------------------------------------------
class UnrealAdapter : public IEngineAdapter {
public:
    GameEngineType GetType() const override {
        return is_ue5_ ? GameEngineType::Unreal5 : GameEngineType::Unreal4;
    }
    const char* GetName() const override {
        return is_ue5_ ? "Unreal Engine 5" : "Unreal Engine 4";
    }

    bool Detect() override {
        if (UnrealDetect_IsUnrealProcess()) {
            const auto& det = UnrealDetect_Get();
            if (det.engineVersionHint.find("5.") != std::string::npos) {
                is_ue5_ = true;
            }
            return true;
        }
        // Fallback: check Unreal modules
        if (GetModuleHandleA("UnrealEngine.dll") || GetModuleHandleA("UE4Client-Win64-Shipping.exe")) {
            return true;
        }
        return false;
    }

    bool Initialize() override {
        return true;
    }

    void Shutdown() override {}

    void* ResolveSymbol(const char* name) override {
        HMODULE hMain = GetModuleHandleA(NULL);
        if (hMain && name) {
            return reinterpret_cast<void*>(GetProcAddress(hMain, name));
        }
        return nullptr;
    }

private:
    bool is_ue5_ = false;
};

// -----------------------------------------------------------------------------
// Godot Adapter
// -----------------------------------------------------------------------------
class GodotAdapter : public IEngineAdapter {
public:
    GameEngineType GetType() const override { return GameEngineType::Godot; }
    const char* GetName() const override { return "Godot Engine"; }

    bool Detect() override {
        HMODULE hMain = GetModuleHandleA(NULL);
        if (hMain) {
            // Godot typically exports godot_main or has Godot in file properties
            if (GetProcAddress(hMain, "godot_main") || GetProcAddress(hMain, "godot_open_midi_inputs")) {
                return true;
            }
        }
        return false;
    }

    bool Initialize() override { return true; }
    void Shutdown() override {}
    void* ResolveSymbol(const char* name) override {
        return nullptr;
    }
};

// -----------------------------------------------------------------------------
// Source / Source 2 Adapter
// -----------------------------------------------------------------------------
class SourceAdapter : public IEngineAdapter {
public:
    GameEngineType GetType() const override { return GameEngineType::Source; }
    const char* GetName() const override { return "Source Engine"; }

    bool Detect() override {
        return GetModuleHandleA("tier0.dll") != NULL && (GetModuleHandleA("engine.dll") != NULL || GetModuleHandleA("vstdlib.dll") != NULL);
    }

    bool Initialize() override { return true; }
    void Shutdown() override {}
    void* ResolveSymbol(const char* name) override {
        HMODULE hEngine = GetModuleHandleA("engine.dll");
        if (hEngine && name) {
            return reinterpret_cast<void*>(GetProcAddress(hEngine, name));
        }
        return nullptr;
    }
};

// -----------------------------------------------------------------------------
// Native / Custom Fallback Adapter
// -----------------------------------------------------------------------------
class NativeAdapter : public IEngineAdapter {
public:
    GameEngineType GetType() const override { return GameEngineType::Native; }
    const char* GetName() const override { return "Native Engine"; }

    bool Detect() override { return true; } // Always succeeds as fallback
    bool Initialize() override { return true; }
    void Shutdown() override {}
    void* ResolveSymbol(const char* name) override {
        HMODULE hMain = GetModuleHandleA(NULL);
        if (hMain && name) {
            return reinterpret_cast<void*>(GetProcAddress(hMain, name));
        }
        return nullptr;
    }
};

// -----------------------------------------------------------------------------
// EngineAdapterManager
// -----------------------------------------------------------------------------
EngineAdapterManager& EngineAdapterManager::GetInstance() {
    static EngineAdapterManager s_instance;
    return s_instance;
}

EngineAdapterManager::EngineAdapterManager() {
    RegisterAdapter(std::make_unique<UnityIL2CPPAdapter>());
    RegisterAdapter(std::make_unique<UnityMonoAdapter>());
    RegisterAdapter(std::make_unique<UnrealAdapter>());
    RegisterAdapter(std::make_unique<GodotAdapter>());
    RegisterAdapter(std::make_unique<SourceAdapter>());
    RegisterAdapter(std::make_unique<NativeAdapter>()); // Fallback
}

EngineAdapterManager::~EngineAdapterManager() {
    Shutdown();
}

void EngineAdapterManager::RegisterAdapter(std::unique_ptr<IEngineAdapter> adapter) {
    adapters_.push_back(std::move(adapter));
}

void EngineAdapterManager::DetectAndInitialize() {
    for (auto& adapter : adapters_) {
        if (adapter->Detect()) {
            if (adapter->Initialize()) {
                active_adapter_ = adapter.get();
                info_.type = adapter->GetType();
                info_.name = adapter->GetName();
                info_.is_ready = true;
                info_.main_module = GetModuleHandleA(NULL);
                std::cout << "[EngineAdapter] Detected and initialized engine: " << info_.name << std::endl;
                return;
            }
        }
    }
}

void EngineAdapterManager::Shutdown() {
    if (active_adapter_) {
        active_adapter_->Shutdown();
        active_adapter_ = nullptr;
    }
    info_ = EngineInfo();
}

} // namespace refix::mods
