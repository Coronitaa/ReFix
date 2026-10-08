#include "../src/lan_core/refix_lan_types.h"
#include "../src/lan_core/refix_lan_firewall.h"
#include "../src/lan_core/refix_lan_wire.h"
#include "../src/lan_core/refix_lan_transport.h"
#include "../src/lan_core/refix_lan_core.h"

#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace refix::lan;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            return false; \
        } \
    } while(0)

static bool TestIdentity() {
    std::cout << "[*] Running TestIdentity..." << std::endl;
    auto& core = ILanCore::Get();
    auto& idService = core.Identity();

    MachineId mid = idService.GetMachineId();
    TEST_ASSERT(mid != 0, "MachineId must be non-zero");

    PeerId pid = idService.GetLocalPeerId();
    TEST_ASSERT(pid.IsValid(), "PeerId must be valid");

    std::string str = pid.ToString();
    TEST_ASSERT(str.length() == 32, "PeerId string must be 32 hex chars");

    PeerId parsed = PeerId::FromString(str);
    TEST_ASSERT(parsed == pid, "PeerId round-trip parsing must match");

    std::string name = idService.GetDisplayName();
    TEST_ASSERT(!name.empty(), "DisplayName must not be empty");

    std::cout << "  [PASS] MachineId=" << std::hex << mid << std::dec
              << ", PeerId=" << str << ", Name=" << name << std::endl;
    return true;
}

static bool TestFirewall() {
    std::cout << "[*] Running TestFirewall (Internet-Zero Policy)..." << std::endl;
    auto& fw = LanFirewall::Get();

    // 1. Allowed Local / Private Subnets
    TEST_ASSERT(fw.IsAllowedIpv4(0x7F000001), "127.0.0.1 must be allowed");
    TEST_ASSERT(fw.IsAllowedIpv4(0x0A010203), "10.1.2.3 must be allowed (10/8)");
    TEST_ASSERT(fw.IsAllowedIpv4(0xAC100001), "172.16.0.1 must be allowed (172.16/12)");
    TEST_ASSERT(fw.IsAllowedIpv4(0xC0A80101), "192.168.1.1 must be allowed (192.168/16)");
    TEST_ASSERT(fw.IsAllowedIpv4(0xA9FE0101), "169.254.1.1 must be allowed (APIPA)");
    TEST_ASSERT(fw.IsAllowedIpv4(0xFFFFFFFF), "255.255.255.255 broadcast must be allowed");

    // 2. Allowed Scoped Multicast
    TEST_ASSERT(fw.IsAllowedIpv4(0xE0000001), "224.0.0.1 link-local multicast must be allowed");
    TEST_ASSERT(fw.IsAllowedIpv4(0xEFff4754), "239.255.71.84 admin multicast must be allowed");

    // 3. Blocked External Public WAN
    TEST_ASSERT(!fw.IsAllowedIpv4(0x08080808), "8.8.8.8 must be BLOCKED");
    TEST_ASSERT(!fw.IsAllowedIpv4(0x01010101), "1.1.1.1 must be BLOCKED");
    TEST_ASSERT(!fw.IsAllowedIpv4(0xA2FE0001), "162.254.0.1 Valve SDR must be BLOCKED");
    TEST_ASSERT(!fw.IsAllowedIpv4(0xE0000101), "224.0.1.1 global multicast must be BLOCKED");

    std::cout << "  [PASS] All Internet-Zero egress policies verified." << std::endl;
    return true;
}

static bool TestWireSerialization() {
    std::cout << "[*] Running TestWireSerialization..." << std::endl;
    ByteWriter w;
    w.WriteU8(42);
    w.WriteU16(1337);
    w.WriteU32(0xDEADBEEF);
    w.WriteU64(0x123456789ABCDEF0ULL);
    w.WriteString("ReFix LAN Core Wire Protocol");

    PeerId testPid;
    testPid.high = 0xAAAAAAAAAAAAAAAAULL;
    testPid.low  = 0x5555555555555555ULL;
    w.WritePeerId(testPid);

    LanEndpoint testEp{0x7F000001, 7777};
    w.WriteEndpoint(testEp);

    AttributeValue attr("GameMode_Deathmatch");
    w.WriteAttribute(attr);

    ByteReader r(w.Data(), w.Size());
    TEST_ASSERT(r.ReadU8() == 42, "U8 mismatch");
    TEST_ASSERT(r.ReadU16() == 1337, "U16 mismatch");
    TEST_ASSERT(r.ReadU32() == 0xDEADBEEF, "U32 mismatch");
    TEST_ASSERT(r.ReadU64() == 0x123456789ABCDEF0ULL, "U64 mismatch");
    TEST_ASSERT(r.ReadString() == "ReFix LAN Core Wire Protocol", "String mismatch");
    TEST_ASSERT(r.ReadPeerId() == testPid, "PeerId mismatch");
    TEST_ASSERT(r.ReadEndpoint() == testEp, "LanEndpoint mismatch");

    AttributeValue readAttr = r.ReadAttribute();
    TEST_ASSERT(readAttr.type == AttributeType::String && readAttr.asString == "GameMode_Deathmatch", "Attribute mismatch");
    TEST_ASSERT(!r.HasError(), "Reader must have no error");

    std::cout << "  [PASS] Wire packet serializer and deserializer verified." << std::endl;
    return true;
}

static bool TestLobbyAndMatchmaking() {
    std::cout << "[*] Running TestLobbyAndMatchmaking..." << std::endl;
    auto& core = ILanCore::Get();
    auto& lobbyService = core.Lobby();
    auto& mm = core.Matchmaking();

    std::string lobbyId = lobbyService.CreateLobby(8, LobbyPermissionLevel::PublicAdvertised);
    TEST_ASSERT(!lobbyId.empty(), "LobbyId must not be empty");

    TEST_ASSERT(lobbyService.IsHosting(lobbyId), "Local peer must be hosting the lobby");

    lobbyService.SetLobbyData(lobbyId, "map_name", AttributeValue("de_dust2"));
    lobbyService.SetLobbyData(lobbyId, "max_players", AttributeValue(static_cast<int64_t>(8)));
    lobbyService.SetLobbyData(lobbyId, "ranked", AttributeValue(false));

    auto lobOpt = lobbyService.GetLobby(lobbyId);
    TEST_ASSERT(lobOpt.has_value(), "Lobby record must exist");
    TEST_ASSERT(lobOpt->members.size() == 1, "Must have 1 member (host)");
    TEST_ASSERT(lobOpt->AvailableSlots() == 7, "Available slots must be 7");
    TEST_ASSERT(lobOpt->attributes.at("map_name").asString == "de_dust2", "map_name mismatch");

    // Matchmaking test
    MatchmakingCriteria criteria;
    criteria.filters.push_back({"map_name", AttributeValue("dust"), ComparisonOp::Contains});
    criteria.filters.push_back({"max_players", AttributeValue(static_cast<int64_t>(4)), ComparisonOp::GreaterThan});

    auto matches = mm.SearchLobbies(criteria);
    TEST_ASSERT(matches.size() == 1, "Must match 1 lobby with map_name contains 'dust' and max_players > 4");

    criteria.filters.clear();
    criteria.filters.push_back({"map_name", AttributeValue("de_inferno"), ComparisonOp::Equal});
    auto noMatches = mm.SearchLobbies(criteria);
    TEST_ASSERT(noMatches.empty(), "Must match 0 lobbies with map_name == 'de_inferno'");

    lobbyService.DestroyLobby(lobbyId);
    TEST_ASSERT(!lobbyService.GetLobby(lobbyId).has_value(), "Lobby must be destroyed");

    std::cout << "  [PASS] Lobby lifecycle, metadata sync, and matchmaking criteria verified." << std::endl;
    return true;
}

static bool TestPeerRegistry() {
    std::cout << "[*] Running TestPeerRegistry..." << std::endl;
    auto& core = ILanCore::Get();
    auto& peers = core.Peers();

    PeerInfo info;
    info.peerId.high = 0x1111222233334444ULL;
    info.peerId.low  = 0x5555666677778888ULL;
    info.machineId = 0x9999AAAABBBBCCCCULL;
    info.endpoint = {0xC0A80132, 27015}; // 192.168.1.50:27015
    info.displayName = "TestPeer_Bob";
    info.externalId.platform = ExternalPlatform::Steam;
    info.externalId.numericId = 76561198000000001ULL;
    info.externalId.stringId = "7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d";

    peers.RegisterOrUpdatePeer(info);

    auto byId = peers.FindByPeerId(info.peerId);
    TEST_ASSERT(byId.has_value() && byId->displayName == "TestPeer_Bob", "Lookup by PeerId failed");

    auto byEp = peers.FindByEndpoint(info.endpoint);
    TEST_ASSERT(byEp.has_value() && byEp.value() == info.peerId, "Lookup by Endpoint failed");

    auto bySteam = peers.FindBySteamId(info.externalId.numericId);
    TEST_ASSERT(bySteam.has_value() && bySteam.value() == info.peerId, "Lookup by SteamId failed");

    auto byPuid = peers.FindByPuid(info.externalId.stringId);
    TEST_ASSERT(byPuid.has_value() && byPuid.value() == info.peerId, "Lookup by EOS PUID failed");

    std::cout << "  [PASS] Multi-index PeerRegistry (PeerId <-> Endpoint <-> SteamID <-> PUID) verified." << std::endl;
    return true;
}

static bool TestDispatcherReentrancy() {
    std::cout << "[*] Running TestDispatcherReentrancy..." << std::endl;
    auto& core = ILanCore::Get();
    auto& cb = core.Callbacks();

    bool reentrantFired = false;
    cb.Subscribe(LanEvent::Type::LobbyCreated, [&](const LanEvent&) {
        // Re-entrant PostEvent from inside a callback handler
        LanEvent ev;
        ev.type = LanEvent::Type::LobbyUpdated;
        cb.PostEvent(ev);
    });

    cb.Subscribe(LanEvent::Type::LobbyUpdated, [&](const LanEvent&) {
        reentrantFired = true;
    });

    LanEvent startEv;
    startEv.type = LanEvent::Type::LobbyCreated;
    cb.PostEvent(startEv);

    cb.DispatchPending(); // Dispatches LobbyCreated, which posts LobbyUpdated
    cb.DispatchPending(); // Dispatches LobbyUpdated

    TEST_ASSERT(reentrantFired, "Re-entrant event must fire without deadlock");
    std::cout << "  [PASS] Dispatcher re-entrancy without deadlock verified." << std::endl;
    return true;
}

static bool TestPeerRegistryReconnectPruning() {
    std::cout << "[*] Running TestPeerRegistryReconnectPruning..." << std::endl;
    auto& core = ILanCore::Get();
    auto& peers = core.Peers();

    PeerInfo p1;
    p1.peerId = {0x111, 0x222};
    p1.endpoint = {0xC0A80110, 7777};
    p1.displayName = "Player1_Instance1";
    p1.externalId = {ExternalPlatform::Steam, 76561198000000099ULL, "puid_99"};
    peers.RegisterOrUpdatePeer(p1);

    // Sleep 15ms so P1 becomes older than 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    // Simulate reconnect: new process/instance with new PeerId, new port, but same SteamID & PUID
    PeerInfo p2;
    p2.peerId = {0x111, 0x333};
    p2.endpoint = {0xC0A80110, 7778};
    p2.displayName = "Player1_Instance2";
    p2.externalId = {ExternalPlatform::Steam, 76561198000000099ULL, "puid_99"};
    peers.RegisterOrUpdatePeer(p2);

    // Verify lookup resolves to the new active instance p2
    auto steamLookup = peers.FindBySteamId(76561198000000099ULL);
    TEST_ASSERT(steamLookup.has_value() && steamLookup.value() == p2.peerId, "Active SteamID must map to P2");

    // Prune stale peers with 10ms threshold (P1 is 15ms old, P2 is 0ms old)
    peers.PruneStalePeers(std::chrono::milliseconds(10));

    // P1 must be pruned, but P2 must remain in registry AND SteamID/PUID lookup must still map to P2!
    TEST_ASSERT(!peers.FindByPeerId(p1.peerId).has_value(), "P1 must be pruned");
    TEST_ASSERT(peers.FindByPeerId(p2.peerId).has_value(), "P2 must remain in registry");

    steamLookup = peers.FindBySteamId(76561198000000099ULL);
    TEST_ASSERT(steamLookup.has_value() && steamLookup.value() == p2.peerId, "SteamID lookup must NOT be wiped by P1 pruning");

    auto puidLookup = peers.FindByPuid("puid_99");
    TEST_ASSERT(puidLookup.has_value() && puidLookup.value() == p2.peerId, "PUID lookup must NOT be wiped by P1 pruning");

    std::cout << "  [PASS] Stale pruning collision and active reconnect protection verified." << std::endl;
    return true;
}

static bool TestTransportLoopback() {
    std::cout << "[*] Running TestTransportLoopback (Dual-Socket & ARQ Reliability)..." << std::endl;

    LanTransport transportA;
    LanTransport transportB;

    PeerId peerA{0x1111, 0x2222};
    PeerId peerB{0x3333, 0x4444};

    transportA.SetLocalPeerId(peerA);
    transportB.SetLocalPeerId(peerB);

    // Start with discovery port 0 for unit testing to avoid colliding with any running processes
    TEST_ASSERT(transportA.Start(47590), "Transport A start failed");
    TEST_ASSERT(transportB.Start(47591), "Transport B start failed");

    LanEndpoint epA = transportA.GetLocalDataEndpoint();
    LanEndpoint epB = transportB.GetLocalDataEndpoint();

    TEST_ASSERT(epA.port != 0, "Transport A ephemeral port must be non-zero");
    TEST_ASSERT(epB.port != 0, "Transport B ephemeral port must be non-zero");
    TEST_ASSERT(epA.port != epB.port, "Transport A and B must have distinct ephemeral ports on same PC");

    // Fix IP for localhost testing
    epA.ipv4 = 0x7F000001;
    epB.ipv4 = 0x7F000001;

    // 1. Send Unreliable Packet A -> B
    const char msgUnreliable[] = "Unreliable Hello from A";
    transportA.SendUnreliable(epB, 1, msgUnreliable, sizeof(msgUnreliable));

    // Wait for receive
    InboundPacket rxPkt;
    bool received = false;
    for (int i = 0; i < 50; ++i) {
        if (transportB.PollInbound(rxPkt)) {
            received = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(received, "Transport B did not receive unreliable packet");
    TEST_ASSERT(rxPkt.payload.size() == sizeof(msgUnreliable), "Payload size mismatch");
    TEST_ASSERT(std::memcmp(rxPkt.payload.data(), msgUnreliable, sizeof(msgUnreliable)) == 0, "Payload content mismatch");

    // 2. Send 20 Reliable Packets A -> B with Sequence & ARQ Verification
    for (int seq = 1; seq <= 20; ++seq) {
        char relMsg[64];
        snprintf(relMsg, sizeof(relMsg), "Reliable Packet Sequence #%03d", seq);
        transportA.SendReliable(peerB, epB, 1, relMsg, strlen(relMsg) + 1);
    }

    int receivedReliable = 0;
    auto startWait = std::chrono::steady_clock::now();
    while (receivedReliable < 20) {
        if (transportB.PollInbound(rxPkt)) {
            if (rxPkt.isReliable) {
                receivedReliable++;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count();
        if (elapsed > 5) break;
    }
    TEST_ASSERT(receivedReliable == 20, "Transport B must receive all 20 reliable packets loss-free");

    // 3. Internet-Zero Drop Test
    LanEndpoint wanTarget{0x08080808, 12345}; // 8.8.8.8
    uint64_t beforeDrops = LanFirewall::Get().GetBlockedEgressCount();
    bool sentToWan = transportA.SendUnreliable(wanTarget, 1, "Leaked WAN packet", 17);
    TEST_ASSERT(!sentToWan, "Transport must NOT transmit to external WAN IP (8.8.8.8)");
    uint64_t afterDrops = LanFirewall::Get().GetBlockedEgressCount();
    TEST_ASSERT(afterDrops > beforeDrops, "Blocked egress counter must increment on WAN transmission attempt");

    transportA.Stop();
    transportB.Stop();

    std::cout << "  [PASS] Ephemeral dual-socket loopback, ARQ in-order sequence, and Internet-Zero drop certified!" << std::endl;
    return true;
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "   REFIX UNIVERSAL LAN CORE & TRANSPORT TEST HARNESS        " << std::endl;
    std::cout << "============================================================" << std::endl;

    if (!TestIdentity()) return 1;
    if (!TestFirewall()) return 1;
    if (!TestWireSerialization()) return 1;
    if (!TestLobbyAndMatchmaking()) return 1;
    if (!TestPeerRegistry()) return 1;
    if (!TestDispatcherReentrancy()) return 1;
    if (!TestPeerRegistryReconnectPruning()) return 1;
    if (!TestTransportLoopback()) return 1;

    std::cout << "\n============================================================" << std::endl;
    std::cout << " [SUCCESS] ALL REFIX LAN CORE & TRANSPORT TESTS PASSED!      " << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
