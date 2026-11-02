#pragma once
#include <cstdint>
#include <string>

// Party Animals local backend: a loopback HTTP server inside the proxy DLL that answers the game's
// backend (AWS Lambda) requests while it runs in login-free mode. UriHelper.NewUri is hooked so URLs of
// the configured backend hosts are rewritten to http://127.0.0.1:<port>/<host><path>.
namespace ReFixPABackend {
    struct Request {
        std::string method;
        std::string host;    // original backend host (first path segment of the rewritten URL)
        std::string path;    // original path + query
        std::string headers; // raw header block
        std::string body;
    };

    struct Response {
        int status = 0; // 0: close the connection without answering (network failure for the game)
        std::string body;
    };

    // Reads [P2P] LocalBackend* settings, starts the server and hooks UriHelper.NewUri.
    // Requires MinHook initialized and the IL2CPP domain attached. Safe to call once.
    void Initialize();

    // Loopback port the server listens on (0 when not running).
    uint16_t Port();
}
