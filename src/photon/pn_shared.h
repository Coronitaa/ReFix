#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// Photon Fusion Shared mode has no host: Photon Cloud's Fusion plugin runs the server simulation every client
// connects to (actor 0). ReFix runs that simulation in a companion process (src/fusion_shared, Fusion's own
// Simulation.Server in Shared topology) on the machine that hosts the room; a Link is the emulator's side of it.
namespace PnShared {

class Link {
public:
    using DataFn = std::function<void(int32_t targetActor, const std::string& packet, bool reliable)>;

    virtual ~Link() = default;
    virtual void Config(int32_t maxPlayers, const std::string& networkProjectConfigJson) = 0;
    virtual void Join(int32_t actor, const std::string& uniqueId) = 0;
    virtual void Leave(int32_t actor) = 0;
    virtual void Master(int32_t actor) = 0;
    virtual void Data(int32_t fromActor, const std::string& packet, bool reliable) = 0;
    // Delivers the server's packets; returns true once (the first time) the simulation reports it is running.
    virtual bool Poll(uint32_t nowMs, const DataFn& onData) = 0;
};

// Starts the companion process (path: exePath) and returns its link, or nullptr when it cannot be started.
std::unique_ptr<Link> StartProcess(const std::string& exePath, const std::string& logPath,
                                   void (*log)(const char* fmt, ...));

} // namespace PnShared
