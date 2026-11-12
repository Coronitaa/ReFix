// Optional Steam online services for ReFix LAN ("ReFix Online").
// ReFix LAN never needs Steam. When the player turns Steam online services on, this module starts the Steam
// client if it is closed, loads Valve's steam_api64.dll a second time under its own name as Spacewar (480) and
// uses it for two things only: the Steam friends list and a P2P tunnel. The game keeps talking to ReFix's
// emulator; LanCore and the overlay presence send their datagrams through the tunnel, where every Steam friend
// that runs ReFix shows up as a virtual LAN address 198.18.x.y.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace refix::steamlink {

enum class State { Off, Starting, Online, Failed };

bool SteamInstalled();               // a Steam client is installed for this Windows user
void SetLocalGame(uint32_t appId);   // published to friends so they see which game you play
void SetLocalGameName(const std::string& utf8Name);   // its display name (window title), also published
void SetSteamOverlay(bool on);       // ReFix.ini [Overlay] BlockSteamOverlay=false: load Steam's overlay too
void Enable();                       // asynchronous; State() goes Starting -> Online or Failed
void Disable();                      // back to LAN only
State GetState();
enum class Step { None, OpeningSteam, SigningIn, Connecting };
Step CurrentStep();                  // what Starting is waiting for
std::string LastError();             // UTF-8, English (the overlay translates the common ones)
uint64_t LocalSteamId();

// Virtual UDP over Steam P2P.
// 198.18.0.0/16 (RFC 2544 benchmarking range): never a real LAN. 10.49.0.0/16 was, and a PC on such a LAN sent
// its own LAN traffic into the tunnel. Virtual addresses never cross the wire, so older ReFix peers are unaffected.
constexpr uint32_t kVirtualNet = 0xC6120000;   // 198.18.0.0/16, host byte order
constexpr uint32_t kVirtualMask = 0xFFFF0000;
inline bool IsVirtual(uint32_t ipHostOrder) { return (ipHostOrder & kVirtualMask) == kVirtualNet; }

// Datagram to one friend. False when the link is off or the address is unknown.
bool Send(uint32_t virtIp, uint16_t srcPort, uint16_t dstPort, const void* data, size_t len);
// Datagram to every friend running ReFix. dstPort 0 = "any discovery socket" on the other side.
void Broadcast(uint16_t srcPort, uint16_t dstPort, const void* data, size_t len);

// Inbound datagrams are handed to every sink on the link thread; a sink filters by dstPort and must not block.
using Sink = void (*)(void* ctx, uint32_t fromIp, uint16_t fromPort, uint16_t dstPort, const uint8_t* data, size_t len);
void AddSink(Sink sink, void* ctx);
void RemoveSink(Sink sink, void* ctx);

// Bridge: a PC with Steam online services on joins its LAN players and its Steam friends in one network.
// Each friend gets a LAN-facing UDP port on this PC (so LAN-only players reach the friend through it), and each
// LAN player behind a friend gets a virtual address here ("relay"), reached through that friend.
// LanCore discovery broadcasts are repeated both ways, so for LanCore everyone is on one LAN.
using LanRewriter = bool (*)(uint8_t* wire, size_t len, uint32_t ip, uint16_t port);  // set the advertised endpoint
using SocketCallback = void (*)(uintptr_t socket, bool created);
void SetLanSide(uint32_t lanIp, LanRewriter rewriter);   // LanTransport: its LAN address and payload rewriter
void SetSocketCallback(SocketCallback cb);                // bridge sockets are ReFix's own (Winsock hooks skip them)
void ForwardFromLan(uint32_t ip, uint16_t port, const void* wire, size_t len, uint16_t dstPort = 0);   // LAN discovery -> friends
bool BridgeEndpoint(uint32_t virtIp, uint16_t port, uint32_t& lanIp, uint16_t& lanPort);  // friend -> LAN address
bool IsBridgePort(uint16_t port);                         // a datagram we repeated on the LAN ourselves
// LAN player behind a friend (or behind the gateway of a relayed sender) -> virtual address. The friend's own
// LAN endpoint (see NoteAdvertised) maps back to the friend itself.
uint32_t RelayAddress(uint32_t viaVirtIp, uint32_t lanIp, uint16_t lanPort);
void NoteAdvertised(uint32_t virtIp, uint32_t lanIp, uint16_t lanPort);  // a friend's own LAN data endpoint

struct Friend {
    uint64_t steamId = 0;
    std::string name;                // UTF-8 persona name
    int personaState = 0;            // 0 offline, 1 online, 2 busy, 3 away, 4 snooze, 5 trade, 6 play
    bool playing = false;            // in some game
    bool refix = false;              // runs ReFix with Steam online services on
    uint32_t appId = 0;              // game it plays through ReFix (0 = unknown)
    std::string gameName;            // that game's name as the friend's ReFix publishes it (UTF-8)
    uint32_t steamGameId = 0;        // AppID of what Steam says the friend plays (480 for ReFix)
    int avatar = 0;                  // Steam image handle of the friend's medium avatar (0 = not loaded yet)
    uint32_t virtIp = 0;             // its address on the tunnel, 0 when not a ReFix peer
};
std::vector<Friend> Friends();       // empty while the link is not Online
uint64_t SteamIdOf(uint32_t virtIp); // 0 when unknown
// RGBA8 pixels of a Steam image handle (Friend::avatar). False while Steam has not downloaded it.
bool AvatarRgba(int image, int& width, int& height, std::vector<uint8_t>& rgba);

} // namespace refix::steamlink
