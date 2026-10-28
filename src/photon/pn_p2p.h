#pragma once
#include <string>
#include <cstdint>

// ReFix Photon P2P: replaces Photon Cloud for PUN/Realtime games with a local emulator plus Steam P2P.
//
//  * Cloud host names resolve to 127.0.0.1 (pn_dns); the NameServer/Master roles answer locally.
//  * The player who creates a room hosts the GameServer role in-process. Other players reach it through a
//    per-host loopback address (127.80.x.y) whose datagrams travel over Steam Datagram Relay (Spacewar 480).
//  * Room names resolve to their host by: own rooms, embedded SteamID64 prefix, Steam lobby owner (games
//    that name rooms after the lobby), then friends' rich presence.
// Settings: [Photon] in ReFix.ini (Backend = ReFixP2P, HostSuffixes, SteamChannel).
namespace ReFixPhotonP2P {
    // Call once Steam is initialized. Idempotent; does nothing when disabled in ReFix.ini.
    void Initialize();

    // Directly registers the SteamID hosting a room name (and its voice room counterpart).
    void RegisterRoomHost(const std::string& roomName, uint64_t hostSteamID);
}
