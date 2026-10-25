#pragma once
#include <string>

// Party Animals P2P room directory for the local backend. Rooms are identified by the host's SteamID64
// (the ReFix relay routes CLIENT_QUERY targetId == roomId to that user) and published to Steam friends
// through rich presence, so the custom-game room list shows rooms hosted by friends.
namespace ReFixPARooms {
    // Serves a P2PRoomLambdaManager / SourceRoomListP2P operation. Returns false for unrelated operations;
    // on success `dataJson` is the operation's result value. Thread-safe.
    bool Handle(const std::string& operation, const std::string& query, std::string& dataJson);
}
