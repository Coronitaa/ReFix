// Unit tests for the Photon P2P emulator core (codec, transport, server roles). No network, no Steam.
// Build & run: tests\run_photon_tests.bat
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "../src/photon/pn_crypto.h"
#include "../src/photon/pn_enet.h"
#include "../src/photon/pn_fusion.h"
#include "../src/photon/pn_proto18.h"
#include "../src/photon/pn_server.h"
#include <cstdarg>
#include <cstdio>
#include <deque>

using namespace PnProto18;

namespace SteamP2PHook {
    void ExcludeSocket(uintptr_t) {}
    void IncludeSocket(uintptr_t) {}
}

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_failures++; } } while (0)

static void NoLog(const char*, ...) {}

// --- Codec ---------------------------------------------------------------------------------------------------

static void TestScalarsRoundTrip() {
    for (int32_t v : { 0, 1, -1, 300, -70000, 2147483647 }) {
        int64_t out;
        CHECK(AsInt(Int(v), out) && out == v);
    }
    std::string s;
    CHECK(AsString(Str("héllo"), s) && s == "héllo");
    bool b;
    CHECK(AsBool(Bool(true), b) && b);
    std::vector<int32_t> arr;
    CHECK(AsIntArray(IntArray({ 1, -2, 3000 }), arr) && arr.size() == 3 && arr[1] == -2);
}

static void TestSkipComplexValues() {
    Table inner;
    inner.Set(Byte(1), StrArray({ "a", "bc" }));
    Table outer;
    outer.Set(Str("k"), inner.Encode());
    outer.Set(Int(7), Null());
    Raw custom = Raw{ (char)kCustom, 'W', 3, 'x', 'y', 'z' };          // custom type 'W', 3 bytes
    Raw slim = Raw{ (char)(kCustomSlimFirst + 5), 2, 0x10, 0x20 };      // slim custom type, 2 bytes
    Bytes all = outer.Encode() + custom + slim + Int(9);
    Reader r(all);
    CHECK(r.ReadRaw() == outer.Encode());
    CHECK(r.ReadRaw() == custom);
    CHECK(r.ReadRaw() == slim);
    int64_t v;
    CHECK(AsInt(r.ReadRaw(), v) && v == 9);
    CHECK(r.ok && r.pos == all.size());
}

static void TestTableMerge() {
    Table t;
    t.Set(Str("a"), Int(1));
    t.Set(Str("b"), Int(2));
    Table changes;
    changes.Set(Str("a"), Int(5));
    changes.Set(Str("b"), Null());
    t.Merge(changes, true);
    CHECK(t.entries.size() == 1 && *t.Find(Str("a")) == Int(5));
    Table sub;
    sub.Set(Str("a"), Int(5));
    CHECK(t.Contains(sub));
}

// --- Transport -----------------------------------------------------------------------------------------------

static std::string BE32(int32_t v) { return std::string{ (char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v }; }

static std::string ClientCommand(uint8_t type, uint8_t channel, uint8_t flags, int32_t seq, const std::string& payload) {
    std::string c{ (char)type, (char)channel, (char)flags, 0 };
    return c + BE32((int32_t)(12 + payload.size())) + BE32(seq) + payload;
}

static std::string ClientDatagram(const std::string& commands, uint8_t count, int32_t challenge) {
    return std::string{ (char)0xFF, (char)0xFF, 0, (char)count } + BE32(1234) + BE32(challenge) + commands;
}

static void TestEnetHandshakeAndOrdering() {
    std::vector<std::string> sent;
    std::vector<std::string> delivered;
    PnEnetConnection conn(7, [&](const std::string& d) { sent.push_back(d); },
                          [&](const std::string& m, bool, uint8_t) { delivered.push_back(m); });
    std::string connect = ClientDatagram(ClientCommand(2, 255, 1, 1, std::string(32, '\0')), 1, 42);
    conn.OnDatagram((const uint8_t*)connect.data(), connect.size(), 100);
    conn.Tick(100);
    CHECK(conn.IsConnected());
    CHECK(sent.size() == 1);
    // Reply carries our challenge, an ack and a VERIFYCONNECT with peer id 7.
    CHECK(sent[0].substr(8, 4) == BE32(42));
    CHECK(sent[0].find(std::string{ 3, (char)255 }) != std::string::npos);

    // Reliable seq 2 arrives before seq 1: delivery must stay ordered.
    std::string two = ClientDatagram(ClientCommand(6, 0, 1, 2, "B"), 1, 42);
    std::string one = ClientDatagram(ClientCommand(6, 0, 1, 1, "A"), 1, 42);
    conn.OnDatagram((const uint8_t*)two.data(), two.size(), 110);
    CHECK(delivered.empty());
    conn.OnDatagram((const uint8_t*)one.data(), one.size(), 120);
    CHECK(delivered.size() == 2 && delivered[0] == "A" && delivered[1] == "B");
}

// Unreliable messages over the MTU arrive as unsequenced fragments (command 15), possibly out of order. They were
// acknowledged but dropped, so Photon Fusion's relayed connect never reached the host.
static void TestEnetReassemblesUnsequencedFragments() {
    std::vector<std::string> delivered;
    std::vector<bool> reliableFlags;
    PnEnetConnection conn(7, [](const std::string&) {},
                          [&](const std::string& m, bool reliable, uint8_t) { delivered.push_back(m); reliableFlags.push_back(reliable); });
    std::string connect = ClientDatagram(ClientCommand(2, 255, 1, 1, std::string(32, '\0')), 1, 42);
    conn.OnDatagram((const uint8_t*)connect.data(), connect.size(), 100);

    const std::string message = "0123456789";
    auto fragment = [&](int32_t number, int32_t offset, const std::string& chunk) {
        // start sequence 5, 3 fragments, total length 10
        std::string extra = BE32(5) + BE32(3) + BE32(number) + BE32((int32_t)message.size()) + BE32(offset);
        return ClientDatagram(ClientCommand(15, 1, 2, 5 + number, extra + chunk), 1, 42);
    };
    std::string f2 = fragment(2, 8, "89"), f0 = fragment(0, 0, "0123"), f1 = fragment(1, 4, "4567");
    conn.OnDatagram((const uint8_t*)f2.data(), f2.size(), 110);
    conn.OnDatagram((const uint8_t*)f0.data(), f0.size(), 111);
    CHECK(delivered.empty());
    conn.OnDatagram((const uint8_t*)f1.data(), f1.size(), 112);
    CHECK(delivered.size() == 1 && delivered[0] == message && !reliableFlags[0]);
    conn.OnDatagram((const uint8_t*)f1.data(), f1.size(), 113); // late duplicate: no second delivery
    CHECK(delivered.size() == 1);
}

static void TestEnetFragmentsLargeMessages() {
    std::vector<std::string> sent;
    PnEnetConnection conn(1, [&](const std::string& d) { sent.push_back(d); }, [](const std::string&, bool, uint8_t) {});
    std::string connect = ClientDatagram(ClientCommand(2, 255, 1, 1, std::string(32, '\0')), 1, 9);
    conn.OnDatagram((const uint8_t*)connect.data(), connect.size(), 0);
    conn.SendPayload(std::string(5000, 'x'), true, 0);
    conn.Tick(0);
    size_t fragments = 0;
    for (const auto& d : sent) {
        CHECK(d.size() <= 1200);
        for (size_t i = 12; i + 12 <= d.size();) {
            uint8_t type = (uint8_t)d[i];
            int32_t size = (int32_t)((uint8_t)d[i + 4] << 24 | (uint8_t)d[i + 5] << 16 | (uint8_t)d[i + 6] << 8 | (uint8_t)d[i + 7]);
            if (type == 8) fragments++;
            i += (size_t)size;
        }
    }
    CHECK(fragments == 5);
}

// --- Crypto ------------------------------------------------------------------------------------------------

static void TestCrypto() {
    // 4^13 mod 497 = 445
    const std::string modulus("\x01\xF1", 2), expected("\x01\xBD", 2);
    CHECK(PnCrypto::PowMod(std::string(1, 4), std::string(1, 13), modulus) == expected);
    PnDiffieHellman a, b;
    std::string ka, kb;
    CHECK(a.DeriveKey(b.PublicKey(), ka) && b.DeriveKey(a.PublicKey(), kb) && ka == kb && ka.size() == 32);
    std::string cipher, plain;
    CHECK(PnCrypto::AesCbcEncrypt(ka, "photon payload", cipher) && cipher.size() == 16);
    CHECK(PnCrypto::AesCbcDecrypt(kb, cipher, plain) && plain == "photon payload");
}

// --- Server roles --------------------------------------------------------------------------------------------

struct FakeClient {
    std::deque<std::string> inbox;
    PnServer::Emulator::SendFn Sink() {
        return [this](const std::string& m, bool, uint8_t) { inbox.push_back(m); };
    }
    // Pops messages until one of the wanted type/code shows up; returns its params.
    bool Take(uint8_t msgType, uint8_t code, Params& params, int16_t* returnCode = nullptr) {
        while (!inbox.empty()) {
            std::string m = inbox.front();
            inbox.pop_front();
            if ((uint8_t)m[1] != msgType || (uint8_t)m[2] != code) continue;
            Reader r(m.data() + 3, m.size() - 3);
            if (msgType == 3) {
                int16_t rc = (int16_t)(r.U8() | (r.U8() << 8));
                if (returnCode) *returnCode = rc;
                r.ReadRaw();
            }
            params.clear();
            return ReadParams(r, params);
        }
        return false;
    }
};

static std::string Request(uint8_t op, const Params& p) { return std::string{ (char)0xF3, 2 } + OperationRequest(op, p); }

static std::string Init(const std::string& appId) {
    std::string m(41, '\0');
    m[0] = (char)0xF3;
    m.replace(9, appId.size(), appId);
    return m;
}

static void TestGameServerRoomFlow() {
    std::vector<std::string> hosted;
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("100-r"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [&](const std::string& n, bool open) { if (open) hosted.push_back(n); };
    env.log = NoLog;
    PnServer::Emulator emu(env);

    FakeClient a, b, c;
    emu.OnConnect(1, PnServer::Role::Game, "100", a.Sink());
    emu.OnConnect(2, PnServer::Role::Game, "200", b.Sink());
    emu.OnConnect(3, PnServer::Role::Game, "300", c.Sink());
    for (uint64_t k : { 1, 2, 3 }) emu.OnMessage(k, Init("app"), true, 0);

    Table roomProps;
    roomProps.Set(Byte(255), Byte(4));
    Table nick;
    nick.Set(Byte(255), Str("A"));
    emu.OnMessage(1, Request(227, { { 255, Str("room") }, { 248, roomProps.Encode() }, { 249, nick.Encode() } }), true, 0);
    Params p;
    int16_t rc = -1;
    CHECK(a.Take(3, 227, p, &rc) && rc == 0);
    int64_t actor;
    CHECK(AsInt(p[254], actor) && actor == 1);
    CHECK(hosted.size() == 1 && hosted[0] == "room");

    // A caches an instantiate-like event, then B joins and must receive it after its join response.
    Table inst;
    inst.Set(Byte(0), Int(1001));
    emu.OnMessage(1, Request(253, { { 244, Byte(202) }, { 245, inst.Encode() }, { 247, Byte(4) } }), true, 0);
    emu.OnMessage(2, Request(226, { { 255, Str("room") } }), true, 0);
    CHECK(b.Take(3, 226, p, &rc) && rc == 0 && AsInt(p[254], actor) && actor == 2);
    CHECK(b.Take(4, 255, p));                                  // join event
    CHECK(b.Take(4, 202, p) && p.count(245) && p[245] == inst.Encode());
    CHECK(a.Take(4, 255, p) && AsInt(p[254], actor) && actor == 1); // A's own join event comes first
    CHECK(a.Take(4, 255, p) && AsInt(p[254], actor) && actor == 2);

    // B raises an event to others: only A gets it, with sender 2.
    emu.OnMessage(2, Request(253, { { 244, Byte(10) }, { 245, Str("hi") } }), false, 1);
    CHECK(a.Take(4, 10, p) && AsInt(p[254], actor) && actor == 2);
    CHECK(!b.Take(4, 10, p));

    // Removing A's cached instantiate by content filter keeps C from receiving it.
    emu.OnMessage(1, Request(253, { { 244, Byte(202) }, { 245, inst.Encode() }, { 247, Byte(6) } }), true, 0);
    emu.OnMessage(3, Request(226, { { 255, Str("room") } }), true, 0);
    CHECK(c.Take(3, 226, p, &rc) && rc == 0);
    CHECK(!c.Take(4, 202, p));

    // Master (A) leaves: B becomes master and the leave event says so.
    emu.OnDisconnect(1);
    CHECK(b.Take(4, 254, p) && AsInt(p[203], actor) && actor == 2);
}

static void TestMasterRoutesJoinsToHost() {
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string& n) { return n == "remote" ? std::string("127.80.0.1:27002") : std::string(); };
    env.newRoomName = [] { return std::string("100-r"); };
    env.joinableRooms = [] { return std::vector<std::string>{ "remote" }; };
    env.roomHosted = [](const std::string&, bool) {};
    env.log = NoLog;
    PnServer::Emulator emu(env);
    FakeClient m;
    emu.OnConnect(1, PnServer::Role::Master, "100", m.Sink());
    Params p;
    int16_t rc;
    std::string addr;
    emu.OnMessage(1, Request(226, { { 255, Str("remote") } }), true, 0);
    CHECK(m.Take(3, 226, p, &rc) && rc == 0 && AsString(p[230], addr) && addr == "127.80.0.1:27002");
    emu.OnMessage(1, Request(226, { { 255, Str("nowhere") } }), true, 0);
    CHECK(m.Take(3, 226, p, &rc) && rc == 32758);
    emu.OnMessage(1, Request(226, { { 255, Str("nowhere") }, { 215, Byte(1) } }), true, 0);
    CHECK(m.Take(3, 226, p, &rc) && rc == 0 && AsString(p[230], addr) && addr == "127.0.0.1:27002");
    emu.OnMessage(1, Request(225, {}), true, 0);
    CHECK(m.Take(3, 225, p, &rc) && rc == 0 && AsString(p[255], addr) && addr == "remote");
}

// Fusion creates its room closed (IsOpen=false) and opens it later; the creator must still enter it, while
// other players are refused until it opens.
static void TestCreatorEntersClosedRoom() {
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("100-r"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [](const std::string&, bool) {};
    env.log = NoLog;
    PnServer::Emulator emu(env);

    FakeClient a, b;
    emu.OnConnect(1, PnServer::Role::Game, "100", a.Sink());
    emu.OnConnect(2, PnServer::Role::Game, "200", b.Sink());
    for (uint64_t k : { 1, 2 }) emu.OnMessage(k, Init("app"), true, 0);

    Table closed;
    closed.Set(Byte(253), Bool(false));
    emu.OnMessage(1, Request(226, { { 255, Str("fusion") }, { 248, closed.Encode() }, { 215, Byte(1) } }), true, 0);
    Params p;
    int16_t rc = -1;
    int64_t actor;
    CHECK(a.Take(3, 226, p, &rc) && rc == 0 && AsInt(p[254], actor) && actor == 1);
    emu.OnMessage(2, Request(226, { { 255, Str("fusion") } }), true, 0);
    CHECK(b.Take(3, 226, p, &rc) && rc == 32764);
}

// Join request captured from Roadside Research (Fusion 2.0.8, protocol V1_6_0) creating a host room.
static const std::string kFusionHostJoin = std::string(
    "\x01\x0a\x02\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 28);

static void TestFusionJoinCodec() {
    PnFusion::Header h;
    PnFusion::Join j;
    CHECK(PnFusion::ParseJoin(kFusionHostJoin, h, j));
    CHECK(h.protocolVersion == 10 && h.versionMajor == 2 && h.versionMinor == 0 && h.versionBuild == 8);
    CHECK(j.type == PnFusion::JOIN_REQUEST && j.gameMode == PnFusion::GAME_MODE_CLIENT_SERVER);
    CHECK(j.peerMode == PnFusion::PEER_SERVER && j.joinRequests == 0 && j.playerRef == 0);
    CHECK(!j.uniqueId.present && !j.encryptionKey.present && !j.encryptionKeySecret.present);
    CHECK(PnFusion::BuildJoin(h, j) == kFusionHostJoin);

    // Unaligned fields after a present byte[] survive a round trip.
    j.uniqueId = { true, std::string("\x01\xff\x80", 3) };
    j.playerRef = -5;
    j.encryptionKeySecret = { true, "k" };
    PnFusion::Header h2;
    PnFusion::Join j2;
    std::string buf = PnFusion::BuildJoin(h, j);
    CHECK(buf.size() % 4 == 0);
    CHECK(PnFusion::ParseJoin(buf, h2, j2) && j2.uniqueId.bytes == j.uniqueId.bytes && j2.playerRef == -5);
    CHECK(!j2.encryptionKey.present && j2.encryptionKeySecret.bytes == "k");
}

// The emulator plays Photon Cloud's Fusion plugin: a Join request sent to the server actor is confirmed to
// the sender only, from actor 0, with its PlayerRef.
// A Fusion client that asks for the NetworkConfig: the plugin requests it from the host, forwards the host's
// response to the client and only then starts the client. ReflexiveInfo is passed between the two.
static void TestFusionClientGetsHostConfigBeforeStart() {
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("100-r"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [](const std::string&, bool) {};
    env.log = NoLog;
    PnServer::Emulator emu(env);

    FakeClient host, client;
    emu.OnConnect(1, PnServer::Role::Game, "100", host.Sink());
    emu.OnConnect(2, PnServer::Role::Game, "200", client.Sink());
    for (uint64_t k : { 1, 2 }) emu.OnMessage(k, Init("app"), true, 0);
    emu.OnMessage(1, Request(227, { { 255, Str("f") } }), true, 0);
    emu.OnMessage(2, Request(226, { { 255, Str("f") } }), true, 0);
    auto toServer = [&](uint64_t key, const std::string& buffer) {
        emu.OnMessage(key, Request(253, { { 244, Byte(100) }, { 245, ByteArray(buffer) }, { 252, IntArray({ 0 }) } }), true, 0);
    };
    Params p;
    std::string data;
    PnFusion::Header h, hostHeader;
    PnFusion::Join j;
    PnFusion::ParseJoin(kFusionHostJoin, hostHeader, j);
    toServer(1, kFusionHostJoin); // host joins as Server and is started
    host.inbox.clear();

    PnFusion::Join cj = j;
    cj.peerMode = PnFusion::PEER_CLIENT;
    cj.joinRequests = PnFusion::JOIN_REQUEST_NETWORK_CONFIG;
    toServer(2, PnFusion::BuildJoin(hostHeader, cj));
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseJoin(data, h, j) && j.type == PnFusion::JOIN_CONFIRMATION);
    CHECK(!client.Take(4, 100, p)); // no Start until the host's config arrived

    uint8_t type = 0;
    PnFusion::OptionalBytes config;
    CHECK(host.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseNetworkConfigSync(data, h, type, config));
    CHECK(type == PnFusion::SYNC_REQUEST && !config.present);

    std::string response = PnFusion::BuildNetworkConfigSync(hostHeader, PnFusion::SYNC_RESPONSE, { true, "{\"cfg\":1}" });
    toServer(1, response);
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && data == response);
    PnFusion::Start st;
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseStart(data, h, st) && st.remoteServerId == 1);

    // ReflexiveInfo (id 3) is relayed verbatim client -> host and host -> client.
    std::string reflexive(8, '\0');
    reflexive[0] = (char)PnFusion::MSG_REFLEXIVE_INFO;
    host.inbox.clear(); // drops the plugin's own ReflexiveInfo for the started client
    toServer(2, reflexive);
    CHECK(host.Take(4, 100, p) && AsBytes(p[245], data) && data == reflexive);
    toServer(1, reflexive);
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && data == reflexive);
}

// A Fusion client connects only after it has the server's ReflexiveInfo. The emulator sends one without
// addresses (so the client uses the relay) a moment after Start, never to the host, and the codec round-trips.
static void TestFusionClientGetsServerReflexiveInfo() {
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("100-r"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [](const std::string&, bool) {};
    env.log = NoLog;
    PnServer::Emulator emu(env);
    emu.Tick(1000);

    FakeClient host, client;
    emu.OnConnect(1, PnServer::Role::Game, "100", host.Sink());
    emu.OnConnect(2, PnServer::Role::Game, "200", client.Sink());
    for (uint64_t k : { 1, 2 }) emu.OnMessage(k, Init("app"), true, 0);
    emu.OnMessage(1, Request(227, { { 255, Str("f") } }), true, 0);
    emu.OnMessage(2, Request(226, { { 255, Str("f") } }), true, 0);
    auto toServer = [&](uint64_t key, const std::string& buffer) {
        emu.OnMessage(key, Request(253, { { 244, Byte(100) }, { 245, ByteArray(buffer) }, { 252, IntArray({ 0 }) } }), true, 0);
    };
    Params p;
    std::string data;
    PnFusion::Header h, hostHeader;
    PnFusion::Join j;
    PnFusion::ParseJoin(kFusionHostJoin, hostHeader, j);
    toServer(1, kFusionHostJoin);
    PnFusion::Join cj = j;
    cj.peerMode = PnFusion::PEER_CLIENT;
    host.inbox.clear();
    toServer(2, PnFusion::BuildJoin(hostHeader, cj));
    PnFusion::Start st;
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseJoin(data, h, j));
    const std::string clientId("\xC8\0\0\0\0\0\0\0", 8); // SteamID "200", little-endian
    CHECK(j.uniqueId.present && j.uniqueId.bytes == clientId); // a client cannot connect without a UniqueId
    // The host learns which actor owns that UniqueId right away.
    PnFusion::ReflexiveInfo clientInfo;
    CHECK(host.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseReflexiveInfo(data, h, clientInfo));
    CHECK(clientInfo.actorId == 2 && clientInfo.uniqueId.bytes == clientId && clientInfo.publicAddr[0] == 0);
    // ...and registers it as the client's player (index == its playerRef, never the host's 1); without this the
    // host leaves the client's connect request waiting forever.
    PnFusion::PlayerRefMapping mapping;
    CHECK(host.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParsePlayerRefMapping(data, h, mapping));
    CHECK(mapping.actorId == 2 && mapping.playerIndex == 2 && mapping.uniqueId.bytes == clientId);
    CHECK(h.protocolVersion == 10 && data.size() % 4 == 0);
    CHECK(!host.Take(4, 100, p));
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseStart(data, h, st));

    emu.Tick(1100);
    CHECK(!client.Take(4, 100, p)); // held back while the client handles Start
    emu.Tick(3000);
    PnFusion::ReflexiveInfo info;
    CHECK(client.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseReflexiveInfo(data, h, info));
    CHECK(info.actorId == 1 && info.publicAddr[2] == 0 && info.privateAddr[0] == 0 && info.natType == 0);
    CHECK(!info.uniqueId.present && h.protocolVersion == 10 && h.versionBuild == 8);
    CHECK(data.size() % 4 == 0);
    emu.Tick(6000);
    CHECK(!client.Take(4, 100, p) && !host.Take(4, 100, p)); // sent once, never to the host

    info.actorId = 7;
    info.publicAddr[1] = 0x1122334455667788ull;
    info.privateAddr[2] = 0x8000000100000000ull;
    info.natType = 3;
    info.uniqueId = { true, "12345678" };
    PnFusion::ReflexiveInfo back;
    CHECK(PnFusion::ParseReflexiveInfo(PnFusion::BuildReflexiveInfo(hostHeader, info), h, back));
    CHECK(back.actorId == 7 && back.publicAddr[1] == info.publicAddr[1] && back.privateAddr[2] == info.privateAddr[2]);
    CHECK(back.natType == 3 && back.uniqueId.bytes == "12345678");
}

static void TestFusionPluginConfirmsJoin() {
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("100-r"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [](const std::string&, bool) {};
    env.log = NoLog;
    PnServer::Emulator emu(env);

    FakeClient a, b;
    emu.OnConnect(1, PnServer::Role::Game, "100", a.Sink());
    emu.OnConnect(2, PnServer::Role::Game, "200", b.Sink());
    for (uint64_t k : { 1, 2 }) emu.OnMessage(k, Init("app"), true, 0);
    emu.OnMessage(1, Request(227, { { 255, Str("f") } }), true, 0);
    emu.OnMessage(2, Request(226, { { 255, Str("f") } }), true, 0);
    b.inbox.clear();

    emu.OnMessage(1, Request(253, { { 244, Byte(100) }, { 245, ByteArray(kFusionHostJoin) }, { 252, IntArray({ 0 }) } }), true, 0);
    Params p;
    int64_t sender = -1;
    std::string data;
    CHECK(a.Take(4, 100, p) && AsInt(p[254], sender) && sender == 0 && AsBytes(p[245], data));
    PnFusion::Header h;
    PnFusion::Join j;
    CHECK(PnFusion::ParseJoin(data, h, j) && h.protocolVersion == 10 && h.versionBuild == 8);
    CHECK(j.type == PnFusion::JOIN_CONFIRMATION && j.peerMode == PnFusion::PEER_SERVER && j.playerRef == 1);
    PnFusion::Start st;
    CHECK(a.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseStart(data, h, st));
    CHECK(st.remoteServerId == 1 && st.startRequests == 0 && h.versionBuild == 8);
    CHECK(!b.Take(4, 100, p));

    // A client joining later is started against the host's actor.
    PnFusion::Header ch;
    PnFusion::Join cj;
    PnFusion::ParseJoin(kFusionHostJoin, ch, cj);
    cj.peerMode = PnFusion::PEER_CLIENT;
    emu.OnMessage(2, Request(253, { { 244, Byte(100) }, { 245, ByteArray(PnFusion::BuildJoin(ch, cj)) }, { 252, IntArray({ 0 }) } }), true, 0);
    CHECK(b.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseJoin(data, h, j));
    CHECK(j.type == PnFusion::JOIN_CONFIRMATION && j.peerMode == PnFusion::PEER_CLIENT && j.playerRef == 2);
    CHECK(b.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseStart(data, h, st) && st.remoteServerId == 1);
    PnFusion::ReflexiveInfo info; // the host only hears which actor owns the client's UniqueId
    CHECK(a.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseReflexiveInfo(data, h, info) && info.actorId == 2);
    PnFusion::PlayerRefMapping mapping;
    CHECK(a.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParsePlayerRefMapping(data, h, mapping) && mapping.actorId == 2);
    CHECK(!a.Take(4, 100, p));
}

class MockSharedLink : public PnShared::Link {
public:
    int32_t maxPlayers = 0;
    std::string configJson;
    std::vector<std::pair<int32_t, std::string>> joins;
    std::vector<int32_t> leaves;
    std::vector<int32_t> masters;
    std::vector<std::tuple<int32_t, std::string, bool>> incomingData;
    std::vector<std::tuple<int32_t, std::string, bool>> queuedOutputs;
    bool reportRunning = false;

    void Config(int32_t mp, const std::string& json) override {
        maxPlayers = mp;
        configJson = json;
        reportRunning = true;
    }
    void Join(int32_t actor, const std::string& uniqueId) override {
        joins.push_back({ actor, uniqueId });
    }
    void Leave(int32_t actor) override {
        leaves.push_back(actor);
    }
    void Master(int32_t actor) override {
        masters.push_back(actor);
    }
    void Data(int32_t fromActor, const std::string& packet, bool reliable) override {
        incomingData.push_back({ fromActor, packet, reliable });
    }
    bool Poll(uint32_t nowMs, const DataFn& onData) override {
        (void)nowMs;
        for (const auto& item : queuedOutputs) {
            onData(std::get<0>(item), std::get<1>(item), std::get<2>(item));
        }
        queuedOutputs.clear();
        bool r = reportRunning;
        reportRunning = false;
        return r;
    }
};

static void TestFusionSharedModeProtocol() {
    MockSharedLink* rawLink = nullptr;
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("shared-room"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [](const std::string&, bool) {};
    env.startFusionShared = [&] {
        auto link = std::make_unique<MockSharedLink>();
        rawLink = link.get();
        return link;
    };
    env.log = NoLog;
    PnServer::Emulator emu(env);

    FakeClient client1, client2;
    emu.OnConnect(1, PnServer::Role::Game, "100", client1.Sink());
    emu.OnConnect(2, PnServer::Role::Game, "200", client2.Sink());
    for (uint64_t k : { 1, 2 }) emu.OnMessage(k, Init("app"), true, 0);
    emu.OnMessage(1, Request(227, { { 255, Str("shared-room") } }), true, 0);
    emu.OnMessage(2, Request(226, { { 255, Str("shared-room") } }), true, 0);

    auto toServer = [&](uint64_t key, const std::string& buffer) {
        emu.OnMessage(key, Request(253, { { 244, Byte(100) }, { 245, ByteArray(buffer) }, { 252, IntArray({ 0 }) } }), true, 0);
    };

    Params p;
    std::string data;
    PnFusion::Header h, joinHeader;
    PnFusion::Join j;
    PnFusion::ParseJoin(kFusionHostJoin, joinHeader, j);
    j.gameMode = PnFusion::GAME_MODE_SHARED;
    j.peerMode = PnFusion::PEER_CLIENT;

    // Client 1 joins in Shared Mode
    toServer(1, PnFusion::BuildJoin(joinHeader, j));

    // Link must be created and client 1 registered
    CHECK(rawLink != nullptr);
    CHECK(rawLink->joins.size() == 1 && rawLink->joins[0].first == 1);
    CHECK(rawLink->masters.size() >= 1 && rawLink->masters.back() == 1);

    // Client 1 receives JOIN_CONFIRMATION with NetworkConfig requested (bit 2)
    CHECK(client1.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseJoin(data, h, j));
    CHECK(j.type == PnFusion::JOIN_CONFIRMATION && j.playerRef == 1);
    CHECK((j.joinRequests & PnFusion::JOIN_REQUEST_NETWORK_CONFIG) != 0);
    CHECK(!client1.Take(4, 100, p)); // No Start yet!

    // Client 1 answers with NetworkProjectConfig
    std::string configJson = "{\"Simulation\":{\"PlayerCount\":8}}";
    toServer(1, PnFusion::BuildNetworkConfigSync(joinHeader, PnFusion::SYNC_RESPONSE, { true, configJson }));
    CHECK(rawLink->configJson == configJson);

    // Pump emulator: poll detects simulation is running, sends Start to Client 1
    emu.Tick(100);
    PnFusion::Start st;
    CHECK(client1.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseStart(data, h, st));
    CHECK(st.remoteServerId == PnFusion::SERVER_ACTOR && st.startRequests == PnFusion::START_REQUEST_CONNECT_TO_SHARED);

    // Client 2 joins in Shared Mode: config already sent, simulation already running
    j.type = PnFusion::JOIN_REQUEST;
    toServer(2, PnFusion::BuildJoin(joinHeader, j));
    CHECK(rawLink->joins.size() == 2 && rawLink->joins[1].first == 2);
    CHECK(client2.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseJoin(data, h, j));
    CHECK(j.type == PnFusion::JOIN_CONFIRMATION && j.playerRef == 2);
    CHECK((j.joinRequests & PnFusion::JOIN_REQUEST_NETWORK_CONFIG) == 0); // NOT requested again

    // Client 2 receives Start immediately
    CHECK(client2.Take(4, 100, p) && AsBytes(p[245], data) && PnFusion::ParseStart(data, h, st));
    CHECK(st.remoteServerId == PnFusion::SERVER_ACTOR && st.startRequests == PnFusion::START_REQUEST_CONNECT_TO_SHARED);

    // Bidirectional data transfer:
    // Client 1 raises EV_DATA (101) targeting server actor 0
    emu.OnMessage(1, Request(253, { { 244, Byte(101) }, { 245, ByteArray("c1-payload") }, { 252, IntArray({ 0 }) } }), true, 0);
    CHECK(rawLink->incomingData.size() == 1);
    CHECK(std::get<0>(rawLink->incomingData[0]) == 1 && std::get<1>(rawLink->incomingData[0]) == "c1-payload");

    // Server companion sends data to Client 1
    rawLink->queuedOutputs.push_back({ 1, "srv-payload", true });
    emu.Tick(200);
    CHECK(client1.Take(4, 101, p));
    int64_t fromActor = 0;
    CHECK(AsInt(p[254], fromActor) && fromActor == PnFusion::SERVER_ACTOR);
    CHECK(AsBytes(p[245], data) && data == "srv-payload");

    // Client 1 leaves:
    emu.OnMessage(1, Request(254, {}), true, 0);
    CHECK(rawLink->leaves.size() == 1 && rawLink->leaves[0] == 1);
    // Master transitions to Client 2
    CHECK(rawLink->masters.back() == 2);
}

static void TestFusionNetworkConfigCodec() {
    PnFusion::Header h;
    h.protocolVersion = 10;
    h.versionMajor = 2;
    h.versionMinor = 0;
    h.versionBuild = 12;

    PnFusion::OptionalBytes emptyCfg;
    std::string reqBuf = PnFusion::BuildNetworkConfigSync(h, PnFusion::SYNC_REQUEST, emptyCfg);
    PnFusion::Header parsedH;
    uint8_t type = 0;
    PnFusion::OptionalBytes parsedCfg;
    CHECK(PnFusion::ParseNetworkConfigSync(reqBuf, parsedH, type, parsedCfg));
    CHECK(type == PnFusion::SYNC_REQUEST && !parsedCfg.present);
    CHECK(((uint8_t)reqBuf[15] & 1) == 1); // isNull bit is 1

    PnFusion::OptionalBytes fullCfg = { true, "{\"Test\":123}" };
    std::string respBuf = PnFusion::BuildNetworkConfigSync(h, PnFusion::SYNC_RESPONSE, fullCfg);
    CHECK(PnFusion::ParseNetworkConfigSync(respBuf, parsedH, type, parsedCfg));
    CHECK(type == PnFusion::SYNC_RESPONSE && parsedCfg.present && parsedCfg.bytes == "{\"Test\":123}");
    CHECK(((uint8_t)respBuf[15] & 1) == 0); // isNull bit is 0
}

static void TestFusionSharedCompanionIpc() {
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
    std::string exePath = "bin\\ReFix\\FusionShared\\ReFixFusionShared.exe";
    DWORD attr = GetFileAttributesA(exePath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return; // skip if companion exe not published yet

    auto link = PnShared::StartProcess(exePath, "build\\tests\\ipc_test.log", NoLog);
    CHECK(link != nullptr);
    if (!link) return;

    link->Config(4, "{}");
    link->Join(1, "12345678");

    bool running = false;
    for (int i = 0; i < 50; i++) {
        Sleep(50);
        if (link->Poll(i * 50, [](int32_t, const std::string&, bool) {})) {
            running = true;
            break;
        }
    }
    CHECK(running);
    link->Leave(1);
    WSACleanup();
}

static void TestLobbyPopulatesPublicGameList() {
    PnServer::Environment env;
    env.localUserId = "100";
    env.masterAddress = "127.0.0.1:27001";
    env.localGameAddress = "127.0.0.1:27002";
    env.gameAddress = [](const std::string&) { return std::string(); };
    env.newRoomName = [] { return std::string("test-room"); };
    env.joinableRooms = [] { return std::vector<std::string>(); };
    env.roomHosted = [](const std::string&, bool) {};
    env.publicLobbies = []() {
        std::map<std::string, Table> lobbies;
        Table r;
        r.Set(Byte(255), Int(4));
        r.Set(Byte(253), Bool(true));
        lobbies["remote-room-123"] = r;
        return lobbies;
    };
    env.log = NoLog;
    PnServer::Emulator emu(env);

    FakeClient host, client;
    emu.OnConnect(1, PnServer::Role::Game, "100", host.Sink());
    emu.OnMessage(1, Init("app"), true, 0);

    Table hostProps;
    hostProps.Set(Byte(255), Int(4));
    hostProps.Set(Byte(253), Bool(true));
    hostProps.Set(Byte(254), Bool(true)); // visible
    emu.OnMessage(1, Request(227, { { 255, Str("phasmo-public-849201") }, { 248, hostProps.Encode() } }), true, 0);

    emu.OnConnect(2, PnServer::Role::Master, "200", client.Sink());
    emu.OnMessage(2, Init("app"), true, 0);
    emu.OnMessage(2, Request(229, {}), true, 0);

    Params respParams;
    int16_t rc = -1;
    CHECK(client.Take(3, 229, respParams, &rc) && rc == 0);

    Params evParams;
    CHECK(client.Take(4, 230, evParams));
    CHECK(evParams.count(222));

    Table gameList;
    CHECK(AsTable(evParams[222], gameList));
    CHECK(gameList.Find(Str("phasmo-public-849201")) != nullptr);
    CHECK(gameList.Find(Str("remote-room-123")) != nullptr);
}

static void TestP2PRoomDiscoveryCodec() {
    // 1. Room Query / Reply round trip
    constexpr uint8_t kRoomQuery = 3, kRoomReply = 4, kLobbyAnnounce = 5;
    uint16_t sentQueryId = 0x4A12;
    std::string roomCode = "582914";

    // Encode kRoomQuery
    std::string query;
    query.push_back((char)kRoomQuery);
    query.push_back(0); // flags
    query.push_back((char)(sentQueryId >> 8));
    query.push_back((char)(sentQueryId & 0xFF));
    query.append(roomCode);

    CHECK(query.size() == 4 + roomCode.size());
    CHECK((uint8_t)query[0] == kRoomQuery);
    uint16_t parsedQueryId = (uint16_t)(((uint8_t)query[2] << 8) | (uint8_t)query[3]);
    CHECK(parsedQueryId == sentQueryId);
    std::string parsedRoomCode = query.substr(4);
    CHECK(parsedRoomCode == roomCode);

    // Encode kRoomReply from host
    uint16_t hostGamePort = 27002;
    std::string reply;
    reply.push_back((char)kRoomReply);
    reply.push_back(0); // flags
    reply.push_back((char)(parsedQueryId >> 8));
    reply.push_back((char)(parsedQueryId & 0xFF));
    reply.push_back((char)(hostGamePort >> 8));
    reply.push_back((char)(hostGamePort & 0xFF));
    reply.append(parsedRoomCode);

    CHECK(reply.size() == 6 + roomCode.size());
    CHECK((uint8_t)reply[0] == kRoomReply);
    uint16_t replyQueryId = (uint16_t)(((uint8_t)reply[2] << 8) | (uint8_t)reply[3]);
    uint16_t replyGamePort = (uint16_t)(((uint8_t)reply[4] << 8) | (uint8_t)reply[5]);
    std::string replyRoomCode = reply.substr(6);
    CHECK(replyQueryId == sentQueryId);
    CHECK(replyGamePort == hostGamePort);
    CHECK(replyRoomCode == roomCode);

    // 2. Lobby Announce add/update with Table payload
    Table originalProps;
    originalProps.Set(Byte(255), Int(4)); // max players = 4
    originalProps.Set(Byte(253), Bool(true)); // is open
    originalProps.Set(Byte(254), Bool(true)); // is visible
    originalProps.Set(Str("map"), Str("Tanglewood"));
    std::string rawProps = originalProps.Encode();

    std::string ann;
    ann.push_back((char)kLobbyAnnounce);
    ann.push_back(1); // action 1: add/update
    ann.push_back((char)(roomCode.size() >> 8));
    ann.push_back((char)(roomCode.size() & 0xFF));
    ann.append(roomCode);
    ann.append(rawProps);

    CHECK((uint8_t)ann[0] == kLobbyAnnounce);
    uint8_t action = (uint8_t)ann[1];
    uint16_t nameLen = (uint16_t)(((uint8_t)ann[2] << 8) | (uint8_t)ann[3]);
    CHECK(action == 1);
    CHECK(nameLen == roomCode.size());
    std::string annRoomName = ann.substr(4, nameLen);
    CHECK(annRoomName == roomCode);

    std::string extractedRawProps = ann.substr(4 + nameLen);
    Table decodedProps;
    CHECK(AsTable(extractedRawProps, decodedProps));
    const Raw* maxP = decodedProps.Find(Byte(255));
    CHECK(maxP != nullptr);
    int64_t maxPlayers = 0;
    CHECK(AsInt(*maxP, maxPlayers) && maxPlayers == 4);

    const Raw* mapProp = decodedProps.Find(Str("map"));
    CHECK(mapProp != nullptr);
    std::string mapName;
    CHECK(AsString(*mapProp, mapName) && mapName == "Tanglewood");

    // 3. Lobby Announce remove
    std::string remAnn;
    remAnn.push_back((char)kLobbyAnnounce);
    remAnn.push_back(2); // action 2: remove
    remAnn.push_back((char)(roomCode.size() >> 8));
    remAnn.push_back((char)(roomCode.size() & 0xFF));
    remAnn.append(roomCode);

    CHECK((uint8_t)remAnn[0] == kLobbyAnnounce);
    CHECK((uint8_t)remAnn[1] == 2);
    uint16_t remNameLen = (uint16_t)(((uint8_t)remAnn[2] << 8) | (uint8_t)remAnn[3]);
    CHECK(remNameLen == roomCode.size());
    CHECK(remAnn.substr(4, remNameLen) == roomCode);
}

int main() {
    TestScalarsRoundTrip();
    TestSkipComplexValues();
    TestTableMerge();
    TestEnetHandshakeAndOrdering();
    TestEnetFragmentsLargeMessages();
    TestEnetReassemblesUnsequencedFragments();
    TestCrypto();
    TestGameServerRoomFlow();
    TestMasterRoutesJoinsToHost();
    TestCreatorEntersClosedRoom();
    TestFusionJoinCodec();
    TestFusionPluginConfirmsJoin();
    TestFusionClientGetsServerReflexiveInfo();
        TestFusionClientGetsHostConfigBeforeStart();
    TestFusionNetworkConfigCodec();
    TestFusionSharedModeProtocol();

    TestFusionSharedCompanionIpc();
    TestLobbyPopulatesPublicGameList();
    TestP2PRoomDiscoveryCodec();
    std::printf(g_failures ? "%d FAILURE(S)\n" : "ALL PHOTON TESTS PASSED\n", g_failures);
    return g_failures ? 1 : 0;
}
