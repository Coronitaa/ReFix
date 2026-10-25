// Unit tests for the Photon P2P emulator core (codec, transport, server roles). No network, no Steam.
// Build & run: tests\run_photon_tests.bat
#include "../src/photon/pn_crypto.h"
#include "../src/photon/pn_enet.h"
#include "../src/photon/pn_fusion.h"
#include "../src/photon/pn_proto18.h"
#include "../src/photon/pn_server.h"
#include <cstdarg>
#include <cstdio>
#include <deque>

using namespace PnProto18;

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
    CHECK(!b.Take(4, 100, p));
}

int main() {
    TestScalarsRoundTrip();
    TestSkipComplexValues();
    TestTableMerge();
    TestEnetHandshakeAndOrdering();
    TestEnetFragmentsLargeMessages();
    TestCrypto();
    TestGameServerRoomFlow();
    TestMasterRoutesJoinsToHost();
    TestCreatorEntersClosedRoom();
    TestFusionJoinCodec();
    TestFusionPluginConfirmsJoin();
    std::printf(g_failures ? "%d FAILURE(S)\n" : "ALL PHOTON TESTS PASSED\n", g_failures);
    return g_failures ? 1 : 0;
}
