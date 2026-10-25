#pragma once
#include <string>

// Party Animals local backend services: answers the game's GraphQL Lambda requests that ReFix can serve
// locally (profile data), persisting player-chosen values in ReFix_Profile.ini next to the game.
namespace ReFixPAServices {
    struct Result {
        bool handled = false; // false: operation not implemented locally
        std::string body;     // full LambdaResult JSON
    };

    // body: the raw request JSON ({"Query": "...", ...}). Thread-safe.
    Result HandleRequest(const std::string& body);

    // Display name the player chose earlier (empty if never changed). Thread-safe.
    std::string StoredDisplayName();
}
