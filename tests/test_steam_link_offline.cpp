// Self-check of the Steam link while Steam online services are off: nothing is sent, no Steam is loaded,
// the virtual LAN range is recognised and sinks can come and go.
#include "../src/steam_link/steam_link.h"
#include <windows.h>
#include <cassert>
#include <cstdio>

void ReFixLog(const char*, ...) {}

namespace sl = refix::steamlink;

static int g_calls = 0;
static void Sink(void*, uint32_t, uint16_t, uint16_t, const uint8_t*, size_t) { ++g_calls; }

int main() {
    assert(sl::IsVirtual(0xC6120001) && sl::IsVirtual(0xC612FFFE));
    assert(!sl::IsVirtual(0xC6130001) && !sl::IsVirtual(0xC0A80001) && !sl::IsVirtual(0x7F000001));
    assert(!sl::IsVirtual(0x0A31EF37));   // 10.49.239.55: a real LAN address seen in the wild, not the tunnel

    assert(sl::GetState() == sl::State::Off);
    const char payload[] = "hello";
    assert(!sl::Send(0xC6120001, 1, 2, payload, sizeof(payload)));   // link off: refused, not queued
    sl::Broadcast(1, 0, payload, sizeof(payload));                    // link off: no-op
    assert(sl::Friends().empty());
    assert(sl::SteamIdOf(0xC6120001) == 0);
    assert(sl::LocalSteamId() == 0);

    int ctx = 0;
    sl::AddSink(&Sink, &ctx);
    sl::RemoveSink(&Sink, &ctx);
    sl::RemoveSink(&Sink, &ctx);   // removing twice is harmless
    assert(g_calls == 0);

    sl::Disable();                 // disabling while off is harmless
    assert(sl::GetState() == sl::State::Off);
    assert(GetModuleHandleW(L"steamclient64.dll") == nullptr && GetModuleHandleW(L"refix_steamlink64.dll") == nullptr);
    std::puts("test_steam_link_offline: OK");
    return 0;
}
