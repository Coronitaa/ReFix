#pragma once

#include <string>
#include <vector>
#include <memory>
#include <windows.h>

namespace refix::mods {

enum class GameEngineType {
    Unknown = 0,
    Native,
    UnityMono,
    UnityIL2CPP,
    Unreal4,
    Unreal5,
    Godot,
    Source,
    Source2,
    GameMaker
};

const char* EngineTypeToString(GameEngineType type);

struct EngineInfo {
    GameEngineType type = GameEngineType::Unknown;
    std::string name = "Unknown";
    std::string version = "Unknown";
    HMODULE main_module = NULL;
    bool is_ready = false;
};

class IEngineAdapter {
public:
    virtual ~IEngineAdapter() = default;
    virtual GameEngineType GetType() const = 0;
    virtual const char* GetName() const = 0;
    virtual bool Detect() = 0;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void* ResolveSymbol(const char* name) = 0;
};

class EngineAdapterManager {
public:
    static EngineAdapterManager& GetInstance();

    EngineAdapterManager();
    ~EngineAdapterManager();

    void RegisterAdapter(std::unique_ptr<IEngineAdapter> adapter);
    void DetectAndInitialize();
    void Shutdown();

    IEngineAdapter* GetActiveAdapter() const { return active_adapter_; }
    const EngineInfo& GetEngineInfo() const { return info_; }

private:
    std::vector<std::unique_ptr<IEngineAdapter>> adapters_;
    IEngineAdapter* active_adapter_ = nullptr;
    EngineInfo info_;
};

} // namespace refix::mods
