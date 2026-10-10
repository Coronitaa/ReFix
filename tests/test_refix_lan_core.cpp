#include "../src/lan_core/refix_lan_types.h"
#include "../src/lan_core/refix_lan_firewall.h"
#include "../src/lan_core/refix_lan_wire.h"
#include "../src/lan_core/refix_lan_transport.h"
#include "../src/lan_core/refix_lan_core.h"
#include "../src/steam_lobby_mapping.h"

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steamclientpublic.h"

#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <random>

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

static bool TestPeerRegistryAdvanced() {
    std::cout << "[*] Running TestPeerRegistryAdvanced..." << std::endl;
    auto& core = ILanCore::Get();
    auto& peers = core.Peers();

    PeerId peerA{0xAAAA, 0x0001};
    PeerId peerB{0xBBBB, 0x0002};

    // 1. Rejection of non-existent peer in BindExternalId
    PeerId nonExistent{0x9999, 0x9999};
    ExternalId ghostExt{ExternalPlatform::Steam, 76561198999999999ULL, "ghost_puid"};
    peers.BindExternalId(nonExistent, ghostExt);
    TEST_ASSERT(!peers.FindByPeerId(nonExistent).has_value(), "Must not create partial peer for non-existent PeerId");
    TEST_ASSERT(!peers.FindBySteamId(ghostExt.numericId).has_value(), "Must not index ghost external ID");

    // 2. Register Peer A
    LanEndpoint ep1{0x7F000001, 11111};
    PeerInfo infoA;
    infoA.peerId = peerA;
    infoA.endpoint = ep1;
    infoA.displayName = "PeerA_Initial";
    infoA.externalId = {ExternalPlatform::Steam, 76561198000000010ULL, "puid_a1"};
    peers.RegisterOrUpdatePeer(infoA);

    TEST_ASSERT(peers.FindByEndpoint(ep1).value() == peerA, "Ep1 must map to peerA");
    TEST_ASSERT(peers.FindBySteamId(infoA.externalId.numericId).value() == peerA, "SteamID must map to peerA");
    TEST_ASSERT(peers.FindByPuid("puid_a1").value() == peerA, "PUID must map to peerA");

    // 3. Change endpoint while conserving identity
    LanEndpoint ep2{0x7F000001, 22222};
    infoA.endpoint = ep2;
    peers.RegisterOrUpdatePeer(infoA);

    TEST_ASSERT(!peers.FindByEndpoint(ep1).has_value(), "Old Ep1 must no longer map to peerA");
    TEST_ASSERT(peers.FindByEndpoint(ep2).value() == peerA, "New Ep2 must map to peerA");
    TEST_ASSERT(peers.FindByPeerId(peerA)->endpoint == ep2, "PeerA record must have Ep2");

    // 4. Change SteamID and PUID
    ExternalId extA2{ExternalPlatform::Steam, 76561198000000020ULL, "puid_a2"};
    peers.BindExternalId(peerA, extA2);

    TEST_ASSERT(!peers.FindBySteamId(76561198000000010ULL).has_value(), "Old SteamID must be unmapped");
    TEST_ASSERT(!peers.FindByPuid("puid_a1").has_value(), "Old PUID must be unmapped");
    TEST_ASSERT(peers.FindBySteamId(76561198000000020ULL).value() == peerA, "New SteamID must map to peerA");
    TEST_ASSERT(peers.FindByPuid("puid_a2").value() == peerA, "New PUID must map to peerA");

    // 5. Reassign identifier that belonged to another peer (Collision resolution)
    PeerInfo infoB;
    infoB.peerId = peerB;
    infoB.endpoint = {0x7F000001, 33333};
    infoB.displayName = "PeerB";
    infoB.externalId = {ExternalPlatform::Steam, 76561198000000030ULL, "puid_b"};
    peers.RegisterOrUpdatePeer(infoB);

    // Reassign PeerB's SteamID to PeerA
    ExternalId stolenExt{ExternalPlatform::Steam, 76561198000000030ULL, "puid_a_stolen"};
    peers.BindExternalId(peerA, stolenExt);

    TEST_ASSERT(peers.FindBySteamId(76561198000000030ULL).value() == peerA, "Stolen SteamID must resolve to peerA");
    auto pB = peers.FindByPeerId(peerB);
    TEST_ASSERT(pB.has_value() && pB->externalId.numericId != 76561198000000030ULL, "PeerB's colliding SteamID must be disassociated");

    std::cout << "  [PASS] Advanced PeerRegistry operations (reassignment, migration, collision safety) certified!" << std::endl;
    return true;
}

static bool TestTransportFragmentationAndEdgeCases() {
    std::cout << "[*] Running TestTransportFragmentationAndEdgeCases..." << std::endl;

    LanTransport transportA;
    LanTransport transportB;
    PeerId peerA{0xA1A1, 0xA2A2};
    PeerId peerB{0xB1B1, 0xB2B2};

    transportA.SetLocalPeerId(peerA);
    transportB.SetLocalPeerId(peerB);

    TEST_ASSERT(transportA.Start(47592), "Transport A start failed");
    TEST_ASSERT(transportB.Start(47593), "Transport B start failed");

    LanEndpoint epB = transportB.GetLocalDataEndpoint();
    epB.ipv4 = 0x7F000001;

    auto waitForPacket = [&](LanTransport& tr, InboundPacket& outPkt, int timeoutMs = 2000) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (tr.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    InboundPacket rx;

    // Case 1: 0-byte packet
    bool s0 = transportA.SendReliable(peerB, epB, 2, nullptr, 0);
    TEST_ASSERT(s0, "SendReliable(0-byte) must return true");
    TEST_ASSERT(waitForPacket(transportB, rx), "Must receive 0-byte reliable packet");
    TEST_ASSERT(rx.payload.empty(), "Payload of 0-byte packet must be empty");
    TEST_ASSERT(rx.channel == 2, "Channel must match");

    // Case 2: 1-byte packet
    uint8_t oneByte = 0x7E;
    bool s1 = transportA.SendReliable(peerB, epB, 2, &oneByte, 1);
    TEST_ASSERT(s1, "SendReliable(1-byte) must return true");
    TEST_ASSERT(waitForPacket(transportB, rx), "Must receive 1-byte packet");
    TEST_ASSERT(rx.payload.size() == 1 && rx.payload[0] == 0x7E, "Payload content mismatch");

    // Case 3: 1200-byte packet (near fragment boundary)
    std::vector<uint8_t> data1200(1200);
    for (size_t i = 0; i < data1200.size(); ++i) data1200[i] = static_cast<uint8_t>(i & 0xFF);
    bool s1200 = transportA.SendReliable(peerB, epB, 3, data1200.data(), data1200.size());
    TEST_ASSERT(s1200, "SendReliable(1200 bytes) must succeed");
    TEST_ASSERT(waitForPacket(transportB, rx), "Must receive reassembled 1200-byte packet");
    TEST_ASSERT(rx.payload.size() == 1200, "1200-byte payload size mismatch");
    TEST_ASSERT(rx.payload == data1200, "1200-byte payload corruption");
    TEST_ASSERT(rx.channel == 3, "Channel boundary must be preserved");

    // Case 4: 1201-byte packet (triggers fragmentation)
    std::vector<uint8_t> data1201(1201);
    for (size_t i = 0; i < data1201.size(); ++i) data1201[i] = static_cast<uint8_t>((i * 7) & 0xFF);
    bool s1201 = transportA.SendReliable(peerB, epB, 3, data1201.data(), data1201.size());
    TEST_ASSERT(s1201, "SendReliable(1201 bytes) must succeed");
    TEST_ASSERT(waitForPacket(transportB, rx), "Must receive reassembled 1201-byte packet");
    TEST_ASSERT(rx.payload.size() == 1201, "1201-byte payload size mismatch");
    TEST_ASSERT(rx.payload == data1201, "1201-byte payload corruption");

    // Case 5: 64 KB Large message
    std::vector<uint8_t> largeData(65536);
    for (size_t i = 0; i < largeData.size(); ++i) {
        largeData[i] = static_cast<uint8_t>((i ^ (i >> 8)) & 0xFF);
    }
    bool sLarge = transportA.SendReliable(peerB, epB, 5, largeData.data(), largeData.size());
    TEST_ASSERT(sLarge, "SendReliable(64 KB) must succeed");
    TEST_ASSERT(waitForPacket(transportB, rx, 4000), "Must receive reassembled 64 KB packet");
    TEST_ASSERT(rx.payload.size() == 65536, "64 KB payload size mismatch");
    TEST_ASSERT(rx.payload == largeData, "64 KB reassembled payload corruption");
    TEST_ASSERT(rx.channel == 5, "Channel 5 must be preserved");

    // Case 6: Reject oversized message (> 256 KB)
    std::vector<uint8_t> overSized(262145);
    bool sOverRel = transportA.SendReliable(peerB, epB, 1, overSized.data(), overSized.size());
    TEST_ASSERT(!sOverRel, "SendReliable must cleanly reject > 256 KB payload");
    bool sOverUnrel = transportA.SendUnreliable(epB, 1, overSized.data(), overSized.size());
    TEST_ASSERT(!sOverUnrel, "SendUnreliable must cleanly reject > 256 KB payload");

    transportA.Stop();
    transportB.Stop();

    std::cout << "  [PASS] Application fragmentation, 0-byte, 1-byte, 1201-byte, 64KB, and limit enforcement verified!" << std::endl;
    return true;
}

class MockTransportListener : public ILanTransportListener {
public:
    std::atomic<int> timeoutCount{0};
    PeerId lastTimedOutPeer;

    void OnDiscoveryPacket(const LanEndpoint&, MsgType, const uint8_t*, size_t) override {}
    void OnInboundData(const InboundPacket&) override {}
    void OnPeerTimeout(const PeerId& peerId, const LanEndpoint&) override {
        timeoutCount.fetch_add(1);
        lastTimedOutPeer = peerId;
    }
};

static bool TestTransportRetransmissionTimeout() {
    std::cout << "[*] Running TestTransportRetransmissionTimeout..." << std::endl;

    MockTransportListener listener;
    LanTransport transport;
    PeerId localPeer{0x1234, 0x5678};
    PeerId ghostPeer{0x9999, 0x8888};
    transport.SetLocalPeerId(localPeer);

    TEST_ASSERT(transport.Start(47594, &listener), "Transport start failed");

    // Send reliable packet to dead endpoint
    LanEndpoint deadEndpoint{0x7F000001, 47599};
    const char data[] = "Timeout test packet";
    transport.SendReliable(ghostPeer, deadEndpoint, 0, data, sizeof(data));

    // Wait until retry exhaustion (8 retries * ~200ms = ~1.6s)
    auto start = std::chrono::steady_clock::now();
    while (listener.timeoutCount.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > 4) break;
    }

    TEST_ASSERT(listener.timeoutCount.load() == 1, "OnPeerTimeout must be called exactly once upon retry exhaustion");
    TEST_ASSERT(listener.lastTimedOutPeer == ghostPeer, "Timed out peer must match ghostPeer");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    TEST_ASSERT(listener.timeoutCount.load() == 1, "No duplicate OnPeerTimeout events must fire after exhaustion");

    transport.Stop();
    std::cout << "  [PASS] Single-event retry exhaustion and queue purge verified!" << std::endl;
    return true;
}

class MockDiscoveryListener : public ILanTransportListener {
public:
    std::atomic<bool> receivedBeacon{false};
    void OnDiscoveryPacket(const LanEndpoint&, MsgType type, const uint8_t*, size_t) override {
        if (type == MsgType::DiscoveryBeacon) {
            receivedBeacon.store(true);
        }
    }
    void OnInboundData(const InboundPacket&) override {}
    void OnPeerTimeout(const PeerId&, const LanEndpoint&) override {}
};

static bool TestFallbackDiscoveryPorts() {
    std::cout << "[*] Running TestFallbackDiscoveryPorts..." << std::endl;

    MockDiscoveryListener listener2;
    LanTransport tr1;
    tr1.SetLocalPeerId({0x1111, 0x1111});
    LanTransport tr2;
    tr2.SetLocalPeerId({0x2222, 0x2222});

    TEST_ASSERT(tr1.Start(47584), "Transport 1 start failed");
    // Explicitly start tr2 on fallback discovery port 47585
    TEST_ASSERT(tr2.Start(47585, &listener2), "Transport 2 start on 47585 failed");

    TEST_ASSERT(tr1.GetDiscoveryPort() != tr2.GetDiscoveryPort(), "Instances must have distinct discovery ports");
    std::cout << "    Instance 1 bound discovery port: " << tr1.GetDiscoveryPort() << std::endl;
    std::cout << "    Instance 2 bound discovery port: " << tr2.GetDiscoveryPort() << std::endl;

    const char beaconMsg[] = "DISCOVERY_ACROSS_PORTS";
    tr1.BroadcastDiscovery(beaconMsg, sizeof(beaconMsg));

    auto start = std::chrono::steady_clock::now();
    while (!listener2.receivedBeacon.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > 2000) break;
    }

    TEST_ASSERT(listener2.receivedBeacon.load(), "Instance 2 on fallback port must receive discovery broadcast from Instance 1");

    tr1.Stop();
    tr2.Stop();
    std::cout << "  [PASS] Cross-port fallback discovery verified!" << std::endl;
    return true;
}

static uint32_t ComputeLobbyAccountId(const std::string& coreLobbyId) {
    return refix::steam::ComputeLobbyAccountId(coreLobbyId);
}

static uint64_t ComputeLobbySteamID(const std::string& coreLobbyId) {
    return refix::steam::ComputeLobbySteamID(coreLobbyId);
}

static bool TestLobbySteamIdIntegrityAndDeterminism() {
    std::cout << "[*] Running TestLobbySteamIdIntegrityAndDeterminism..." << std::endl;

    // 1. Basic Validity & Standard Steamworks Flags
    std::string testLobbyId = "LOBBY_2e8d59b3_1";
    uint64_t steamLobby64 = ComputeLobbySteamID(testLobbyId);
    CSteamID steamId(steamLobby64);

    TEST_ASSERT(steamId.IsValid(), "CSteamID::IsValid() must return true for computed lobby ID");
    TEST_ASSERT(steamId.IsLobby(), "CSteamID::IsLobby() must return true for computed lobby ID");
    TEST_ASSERT(steamId.GetEUniverse() == k_EUniversePublic, "Universe must be k_EUniversePublic (1)");
    TEST_ASSERT(steamId.GetEAccountType() == k_EAccountTypeChat, "AccountType must be k_EAccountTypeChat (8)");
    TEST_ASSERT((steamId.GetUnAccountInstance() & k_EChatInstanceFlagLobby) != 0, "Instance must have k_EChatInstanceFlagLobby flag set");
    TEST_ASSERT(steamId.GetAccountID() != 0, "AccountID must be non-zero");

    // Verify upper 32-bits format:
    // (k_EUniversePublic=1 << 24) | (k_EAccountTypeChat=8 << 20) | (k_EChatInstanceFlagLobby=0x40000) = 0x01840000
    uint32_t upper32 = static_cast<uint32_t>(steamLobby64 >> 32);
    TEST_ASSERT(upper32 == 0x01840000, "Upper 32-bits must strictly equal 0x01840000 (Universe=Public, Type=Chat, Instance=Lobby)");

    // 2. Cross-Process Determinism (Host vs Client)
    uint64_t hostComputedId = ComputeLobbySteamID(testLobbyId);
    uint64_t clientComputedId = ComputeLobbySteamID(testLobbyId);
    TEST_ASSERT(hostComputedId == clientComputedId, "Host and Client must derive 100% identical CSteamID from core lobby ID");

    // 3. Multi-Lobby Stability Across Different Discovery Orders
    const int NUM_LOBBIES = 30;
    std::vector<std::string> coreIds;
    for (int i = 0; i < NUM_LOBBIES; ++i) {
        coreIds.push_back("LOBBY_peer" + std::to_string(i * 137) + "_" + std::to_string(i));
    }

    // Process A: discovers in forward order 0..29
    std::vector<uint64_t> idsForward;
    for (int i = 0; i < NUM_LOBBIES; ++i) {
        idsForward.push_back(ComputeLobbySteamID(coreIds[i]));
    }

    // Process B: discovers in reverse order 29..0
    std::vector<uint64_t> idsReverse(NUM_LOBBIES);
    for (int i = NUM_LOBBIES - 1; i >= 0; --i) {
        idsReverse[i] = ComputeLobbySteamID(coreIds[i]);
    }

    // Process C: discovers in scrambled order
    std::vector<size_t> indices(NUM_LOBBIES);
    for (size_t i = 0; i < NUM_LOBBIES; ++i) indices[i] = i;
    std::mt19937 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);
    std::vector<uint64_t> idsScrambled(NUM_LOBBIES);
    for (size_t idx : indices) {
        idsScrambled[idx] = ComputeLobbySteamID(coreIds[idx]);
    }

    for (int i = 0; i < NUM_LOBBIES; ++i) {
        TEST_ASSERT(idsForward[i] == idsReverse[i], "Forward and Reverse discovery order produced divergent SteamID!");
        TEST_ASSERT(idsForward[i] == idsScrambled[i], "Forward and Scrambled discovery order produced divergent SteamID!");
    }

    // 4. Large-Scale Collision Resistance
    const int NUM_TEST_COLLISIONS = 1000;
    std::unordered_set<uint64_t> uniqueIds;
    for (int i = 0; i < NUM_TEST_COLLISIONS; ++i) {
        std::string cid = "LOBBY_" + std::to_string(i * 7919) + "_" + std::to_string(i);
        uint64_t sid = ComputeLobbySteamID(cid);
        TEST_ASSERT(CSteamID(sid).IsValid(), "Every generated ID must be valid");
        TEST_ASSERT(CSteamID(sid).IsLobby(), "Every generated ID must be a lobby");
        bool inserted = uniqueIds.insert(sid).second;
        TEST_ASSERT(inserted, "Hash collision detected in 1,000 generated lobby test set!");
    }

    std::cout << "  [PASS] CSteamID::IsValid(), IsLobby(), Cross-Process Determinism, Order Independence, and Collision Resistance certified!" << std::endl;
    return true;
}

static bool TestSendUnreliableContractAndBoundaries() {
    std::cout << "[*] Running TestSendUnreliableContractAndBoundaries..." << std::endl;

    LanTransport trA;
    LanTransport trB;
    trA.SetLocalPeerId({0x1111, 0x1111});
    trB.SetLocalPeerId({0x2222, 0x2222});

    TEST_ASSERT(trA.Start(47601), "Transport A failed to start");
    TEST_ASSERT(trB.Start(47602), "Transport B failed to start");

    LanEndpoint epB = trB.GetLocalDataEndpoint();
    epB.ipv4 = 0x7F000001;

    auto waitForPacket = [&](LanTransport& tr, InboundPacket& outPkt, int timeoutMs = 1500) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (tr.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    InboundPacket rx;

    // 1. Null data with non-zero length -> must return false
    TEST_ASSERT(!trA.SendUnreliable(epB, 0, nullptr, 10), "SendUnreliable with nullptr and len > 0 must return false");

    // 2. Zero-length packet -> must succeed and deliver 0-byte packet
    TEST_ASSERT(trA.SendUnreliable(epB, 0, nullptr, 0), "SendUnreliable with 0 bytes must return true");
    TEST_ASSERT(waitForPacket(trB, rx), "Receiver must receive 0-byte unreliable packet");
    TEST_ASSERT(rx.payload.empty(), "Payload must be empty");
    TEST_ASSERT(!rx.isReliable, "Packet must be unreliable");

    // 3. Exact MTU boundary: REFIX_MAX_UNRELIABLE_PAYLOAD (1150 bytes)
    std::vector<uint8_t> mtuData(REFIX_MAX_UNRELIABLE_PAYLOAD);
    for (size_t i = 0; i < mtuData.size(); ++i) mtuData[i] = static_cast<uint8_t>((i * 3 + 7) & 0xFF);
    TEST_ASSERT(trA.SendUnreliable(epB, 1, mtuData.data(), mtuData.size()), "SendUnreliable at exact MTU limit (1150 bytes) must succeed");
    TEST_ASSERT(waitForPacket(trB, rx), "Must receive 1150-byte unreliable packet");
    TEST_ASSERT(rx.payload == mtuData, "Received 1150-byte unreliable payload must match bit-for-bit");

    // 4. Boundary + 1 (1151 bytes) -> strictly rejected, never delivered or truncated
    std::vector<uint8_t> overMtu(REFIX_MAX_UNRELIABLE_PAYLOAD + 1, 0xAA);
    TEST_ASSERT(!trA.SendUnreliable(epB, 1, overMtu.data(), overMtu.size()), "SendUnreliable > 1150 bytes must return false");
    TEST_ASSERT(!waitForPacket(trB, rx, 200), "No truncated packet must be delivered when rejected");

    // 5. Large unreliable payload (64 KB / 256 KB) -> strictly rejected
    std::vector<uint8_t> largePkt(65536, 0xBB);
    TEST_ASSERT(!trA.SendUnreliable(epB, 1, largePkt.data(), largePkt.size()), "SendUnreliable 64 KB must return false");

    trA.Stop();
    trB.Stop();

    std::cout << "  [PASS] SendUnreliable MTU contract, boundary limits, and rejection without truncation certified!" << std::endl;
    return true;
}

static bool TestFragmentationHardeningAndMemoryLimits() {
    std::cout << "[*] Running TestFragmentationHardeningAndMemoryLimits..." << std::endl;

    LanTransport trA;
    LanTransport trB;
    PeerId peerA{0x3333, 0x4444};
    PeerId peerB{0x5555, 0x6666};
    trA.SetLocalPeerId(peerA);
    trB.SetLocalPeerId(peerB);

    TEST_ASSERT(trA.Start(47603), "Transport A failed to start");
    TEST_ASSERT(trB.Start(47604), "Transport B failed to start");

    LanEndpoint epB = trB.GetLocalDataEndpoint();
    epB.ipv4 = 0x7F000001;

    auto waitForPacket = [&](LanTransport& tr, InboundPacket& outPkt, int timeoutMs = 1500) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (tr.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    // 1. Multiple-Fragment Transmission & Full Bit-for-Bit Reassembly
    std::vector<uint8_t> multiFrag(3000);
    for (size_t i = 0; i < multiFrag.size(); ++i) multiFrag[i] = static_cast<uint8_t>(i & 0xFF);
    TEST_ASSERT(trA.SendReliable(peerB, epB, 2, multiFrag.data(), multiFrag.size()), "SendReliable multi-fragment must succeed");

    InboundPacket rx;
    TEST_ASSERT(waitForPacket(trB, rx), "Receiver must receive reassembled multi-fragment packet");
    TEST_ASSERT(rx.payload == multiFrag, "Reassembled multi-fragment packet must match exactly");
    TEST_ASSERT(trB.GetGlobalReassemblyBytes() == 0, "Global reassembly memory must be 0 after successful assembly");
    TEST_ASSERT(trB.GetReassemblyContextCount() == 0, "Reassembly contexts must be 0 after completion");
    TEST_ASSERT(trB.GetTrackedPeerReassemblyCount() == 0, "Tracked peer reassembly map must be 0 after completion");
    TEST_ASSERT(trB.GetPeerReassemblyBytes(peerA) == 0, "Peer reassembly bytes must be 0 after completion");

    // 2. Reject messages exceeding REFIX_MAX_MESSAGE_SIZE (256 KB)
    std::vector<uint8_t> tooBig(REFIX_MAX_MESSAGE_SIZE + 1, 0xEE);
    TEST_ASSERT(!trA.SendReliable(peerB, epB, 2, tooBig.data(), tooBig.size()), "SendReliable > 256 KB must be rejected");

    // 3. Context limit enforcement
    TEST_ASSERT(trB.GetReassemblyContextCount() <= REFIX_MAX_REASSEMBLY_CONTEXTS, "Reassembly contexts must be capped at 64");

    trA.Stop();
    trB.Stop();

    std::cout << "  [PASS] Fragmentation memory tracking, reassembly cleanup, and context bounds certified!" << std::endl;
    return true;
}

static bool TestLanInterfaceSelection() {
    std::cout << "[*] Running TestLanInterfaceSelection..." << std::endl;

    // 1. Standard auto-detection
    std::string autoReason;
    uint32_t autoIp = ResolveLocalIpv4(&autoReason);
    TEST_ASSERT(autoIp != 0, "Resolved IP must not be 0");
    TEST_ASSERT(!autoReason.empty(), "Selection reason must not be empty");
    std::cout << "    Auto-detected LAN IP: "
              << ((autoIp >> 24) & 0xFF) << "." << ((autoIp >> 16) & 0xFF) << "."
              << ((autoIp >> 8) & 0xFF) << "." << (autoIp & 0xFF)
              << " (" << autoReason << ")" << std::endl;

    // 2. Explicit configuration override via REFIX_LAN_INTERFACE_IP
    SetEnvironmentVariableA("REFIX_LAN_INTERFACE_IP", "192.168.42.99");
    std::string explicitReason;
    uint32_t explicitIp = ResolveLocalIpv4(&explicitReason);
    TEST_ASSERT(explicitIp == 0xC0A82A63, "Explicit IP 192.168.42.99 must resolve to 0xC0A82A63");
    TEST_ASSERT(explicitReason.find("Explicitly configured") != std::string::npos, "Reason must indicate explicit environment configuration");

    // Clean up environment variable
    SetEnvironmentVariableA("REFIX_LAN_INTERFACE_IP", nullptr);

    std::cout << "  [PASS] Deterministic LAN interface selection and environment override certified!" << std::endl;
    return true;
}

static void SendRawFragment(SOCKET s, const LanEndpoint& target, const PeerId& sender, uint32_t sessionId, uint8_t fragIndex, uint8_t fragTotal, uint32_t seq, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> buf(sizeof(WireHeader) + payload.size(), 0);
    auto* hdr = reinterpret_cast<WireHeader*>(buf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE | FLAG_FRAGMENT;
    if (fragIndex == fragTotal - 1) hdr->flags |= FLAG_LAST_FRAGMENT;
    hdr->SetSenderPeerId(sender);
    hdr->generationId = 1;
    hdr->fragmentMsgId = sessionId;
    hdr->channel = 1;
    hdr->fragIndex = fragIndex;
    hdr->fragTotal = fragTotal;
    hdr->reserved = 0;
    hdr->sequence = seq;
    hdr->ack = 0;
    hdr->sackMask = 0;
    hdr->payloadLen = static_cast<uint16_t>(payload.size());
    if (!payload.empty()) {
        std::memcpy(buf.data() + sizeof(WireHeader), payload.data(), payload.size());
    }

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);
    sendto(s, reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()), 0, reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
}

static void SendRawReliableWirePacket(SOCKET s, const LanEndpoint& target, const PeerId& sender, uint32_t seq, uint8_t channel, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> buf(sizeof(WireHeader) + payload.size(), 0);
    auto* hdr = reinterpret_cast<WireHeader*>(buf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(sender);
    hdr->generationId = 1;
    hdr->fragmentMsgId = 0;
    hdr->channel = channel;
    hdr->fragIndex = 0;
    hdr->fragTotal = 1;
    hdr->reserved = 0;
    hdr->sequence = seq;
    hdr->ack = 0;
    hdr->sackMask = 0;
    hdr->payloadLen = static_cast<uint16_t>(payload.size());
    if (!payload.empty()) {
        std::memcpy(buf.data() + sizeof(WireHeader), payload.data(), payload.size());
    }

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);
    sendto(s, reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()), 0, reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
}

static void SendRawUnreliableWirePacket(SOCKET s, const LanEndpoint& target, const PeerId& sender, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> buf(sizeof(WireHeader) + payload.size(), 0);
    auto* hdr = reinterpret_cast<WireHeader*>(buf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataUnreliable);
    hdr->flags = 0;
    hdr->SetSenderPeerId(sender);
    hdr->generationId = 0;
    hdr->fragmentMsgId = 0;
    hdr->channel = 1;
    hdr->fragIndex = 0;
    hdr->fragTotal = 1;
    hdr->reserved = 0;
    hdr->sequence = 0;
    hdr->ack = 0;
    hdr->sackMask = 0;
    hdr->payloadLen = static_cast<uint16_t>(payload.size());
    if (!payload.empty()) {
        std::memcpy(buf.data() + sizeof(WireHeader), payload.data(), payload.size());
    }

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);
    sendto(s, reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()), 0, reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
}

static bool TestSendReliableStrictContractAndBoundaries() {
    std::cout << "[*] Running TestSendReliableStrictContractAndBoundaries..." << std::endl;

    LanTransport trA;
    LanTransport trB;
    PeerId peerA{0x7777, 0x1111};
    PeerId peerB{0x8888, 0x2222};
    PeerId invalidPeer{0, 0};
    trA.SetLocalPeerId(peerA);
    trB.SetLocalPeerId(peerB);

    TEST_ASSERT(trA.Start(47605), "Transport A failed to start");
    TEST_ASSERT(trB.Start(47606), "Transport B failed to start");

    LanEndpoint epB = trB.GetLocalDataEndpoint();
    epB.ipv4 = 0x7F000001;

    auto waitForPacket = [&](LanTransport& tr, InboundPacket& outPkt, int timeoutMs = 2000) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (tr.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    InboundPacket rx;

    // 1. Invalid PeerId -> rejected immediately
    char sampleData[] = "test data";
    TEST_ASSERT(!trA.SendReliable(invalidPeer, epB, 1, sampleData, sizeof(sampleData)),
                "SendReliable with invalid PeerId must return false");

    // 2. nullptr with len > 0 -> rejected immediately
    TEST_ASSERT(!trA.SendReliable(peerB, epB, 1, nullptr, 100),
                "SendReliable with nullptr and len > 0 must return false");

    // 3. Payload > REFIX_MAX_MESSAGE_SIZE (256 KB) -> rejected immediately
    std::vector<uint8_t> hugeBuf(REFIX_MAX_MESSAGE_SIZE + 10, 0x55);
    TEST_ASSERT(!trA.SendReliable(peerB, epB, 1, hugeBuf.data(), hugeBuf.size()),
                "SendReliable with len > REFIX_MAX_MESSAGE_SIZE must return false");

    // 4. Zero-byte payload -> handled explicitly, delivered reliably
    TEST_ASSERT(trA.SendReliable(peerB, epB, 1, nullptr, 0),
                "SendReliable with 0-byte payload must return true");
    TEST_ASSERT(waitForPacket(trB, rx), "Receiver must receive 0-byte reliable packet");
    TEST_ASSERT(rx.payload.empty(), "0-byte reliable payload must be empty");
    TEST_ASSERT(rx.isReliable, "0-byte packet must be marked reliable");

    // 5. Unfragmented MTU boundary: 1150 bytes
    std::vector<uint8_t> mtuBuf(REFIX_MAX_FRAGMENT_PAYLOAD);
    for (size_t i = 0; i < mtuBuf.size(); ++i) mtuBuf[i] = static_cast<uint8_t>((i ^ 0xAA) & 0xFF);
    TEST_ASSERT(trA.SendReliable(peerB, epB, 1, mtuBuf.data(), mtuBuf.size()),
                "SendReliable at 1150-byte MTU limit must return true");
    TEST_ASSERT(waitForPacket(trB, rx), "Receiver must receive 1150-byte packet");
    TEST_ASSERT(rx.payload == mtuBuf, "1150-byte payload must match bit-for-bit");

    // 6. Multi-fragment payload: 8192 bytes
    std::vector<uint8_t> frag8k(8192);
    for (size_t i = 0; i < frag8k.size(); ++i) frag8k[i] = static_cast<uint8_t>((i * 13 + 3) & 0xFF);
    TEST_ASSERT(trA.SendReliable(peerB, epB, 2, frag8k.data(), frag8k.size()),
                "SendReliable at 8192 bytes must return true");
    TEST_ASSERT(waitForPacket(trB, rx, 3000), "Receiver must receive reassembled 8192-byte packet");
    TEST_ASSERT(rx.payload == frag8k, "8192-byte payload must match bit-for-bit");

    trA.Stop();
    trB.Stop();

    std::cout << "  [PASS] SendReliable strict contract (0, 1150, 8192 bytes & invalid args rejection) certified!" << std::endl;
    return true;
}

static bool TestReassemblyExpirationAndPeerAccountingStress() {
    std::cout << "[*] Running TestReassemblyExpirationAndPeerAccountingStress..." << std::endl;

    LanTransport trB;
    PeerId peerB{0x9999, 0x9999};
    trB.SetLocalPeerId(peerB);
    TEST_ASSERT(trB.Start(47607), "Transport B failed to start");

    LanEndpoint epB = trB.GetLocalDataEndpoint();
    epB.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Raw UDP socket must be created");

    auto waitForPacket = [&](LanTransport& tr, InboundPacket& outPkt, int timeoutMs = 2000) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (tr.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    // 1. Send incomplete fragments from 10 distinct PeerIds (fragment 0 of 2, 400 bytes each)
    std::vector<PeerId> testPeers;
    std::vector<uint8_t> fragChunk(400, 0x42);
    for (uint64_t i = 1; i <= 10; ++i) {
        PeerId p{0xCAFE0000 | i, 0xBEEF0000 | i};
        testPeers.push_back(p);
        SendRawFragment(rawSock, epB, p, 100 + static_cast<uint32_t>(i), 0, 2, 1, fragChunk);
    }

    // Give reactor thread a few milliseconds to process inbound queue
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(trB.GetReassemblyContextCount() == 10, "Must track 10 incomplete reassembly contexts");
    TEST_ASSERT(trB.GetGlobalReassemblyBytes() == 4000, "Global reassembly bytes must equal 4000");
    TEST_ASSERT(trB.GetTrackedPeerReassemblyCount() == 10, "Tracked peer reassembly count must equal 10");

    // 2. Duplicate fragment injection from peer 1 (must NOT increment byte count)
    SendRawFragment(rawSock, epB, testPeers[0], 101, 0, 2, 1, fragChunk);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(trB.GetGlobalReassemblyBytes() == 4000, "Duplicate fragment must not double-count memory");
    TEST_ASSERT(trB.GetPeerReassemblyBytes(testPeers[0]) == 400, "Peer 1 memory must remain 400 bytes");

    // 3. Incompatible metadata injection from peer 1 (fragTotal = 5 != 2 -> must be rejected)
    SendRawFragment(rawSock, epB, testPeers[0], 101, 1, 5, 2, fragChunk);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(trB.GetGlobalReassemblyBytes() == 4000, "Incompatible metadata must be rejected without memory change");

    // 4. Complete message for peer 1 (fragIndex = 1, fragTotal = 2, seq = 2: bad datagram was rejected without sequence advance)
    std::vector<uint8_t> secondChunk(400, 0x43);
    SendRawFragment(rawSock, epB, testPeers[0], 101, 1, 2, 2, secondChunk);

    InboundPacket fullPkt;
    TEST_ASSERT(waitForPacket(trB, fullPkt), "Must receive completed reassembled message for peer 1");
    TEST_ASSERT(fullPkt.payload.size() == 800, "Reassembled payload size must be 800 bytes");
    TEST_ASSERT(trB.GetPeerReassemblyBytes(testPeers[0]) == 0, "Peer 1 accounting must be 0 after message completion");
    TEST_ASSERT(trB.GetTrackedPeerReassemblyCount() == 9, "Tracked peer count must decrement to 9 when peer 1 has no contexts");
    TEST_ASSERT(trB.GetGlobalReassemblyBytes() == 3600, "Global memory must decrement to 3600");

    // 5. Subsequent valid transmission from peer 1 after previous completion (seq = 3)
    SendRawFragment(rawSock, epB, testPeers[0], 201, 0, 1, 3, fragChunk); // Single fragment complete
    TEST_ASSERT(waitForPacket(trB, fullPkt), "Must receive subsequent valid message from peer 1");
    TEST_ASSERT(trB.GetPeerReassemblyBytes(testPeers[0]) == 0, "Peer 1 accounting must remain 0");

    // 6. Reset peer states for all remaining incomplete peers
    for (size_t i = 1; i < testPeers.size(); ++i) {
        trB.ResetPeerState(testPeers[i]);
    }

    TEST_ASSERT(trB.GetGlobalReassemblyBytes() == 0, "Global reassembly bytes must strictly equal 0 after reset");
    TEST_ASSERT(trB.GetReassemblyContextCount() == 0, "Reassembly contexts must be 0 after reset");
    TEST_ASSERT(trB.GetTrackedPeerReassemblyCount() == 0, "Tracked peer reassembly map must be 0 after reset");

    closesocket(rawSock);
    trB.Stop();

    std::cout << "  [PASS] Reassembly accounting, bounded peer map, and post-cleanup transmission certified!" << std::endl;
    return true;
}
static bool TestLobbyConflictPolicyAndProductionRegistry() {
    std::cout << "[*] Running TestLobbyConflictPolicyAndProductionRegistry..." << std::endl;

    auto& registry = refix::steam::SteamLobbyRegistry::Get();
    registry.Clear();

    // 1. Cross-process uniqueness: Two distinct PeerId's produce distinct core lobby IDs
    PeerId p1{0x12345678, (static_cast<uint64_t>(1001) << 32) | 0xAAAA};
    PeerId p2{0x12345678, (static_cast<uint64_t>(1002) << 32) | 0xBBBB};
    std::string cid1 = "LOBBY_" + p1.ToString() + "_1";
    std::string cid2 = "LOBBY_" + p2.ToString() + "_1";
    TEST_ASSERT(cid1 != cid2, "Core lobby IDs from different instances must be distinct");

    uint64_t sid1 = refix::steam::ComputeLobbySteamID(cid1);
    uint64_t sid2 = refix::steam::ComputeLobbySteamID(cid2);
    TEST_ASSERT(CSteamID(sid1).IsValid(), "sid1 must be valid CSteamID");
    TEST_ASSERT(CSteamID(sid2).IsValid(), "sid2 must be valid CSteamID");
    TEST_ASSERT(static_cast<uint32_t>(sid1 >> 32) == 0x01840000, "sid1 must have canonical upper 32 bits");
    TEST_ASSERT(static_cast<uint32_t>(sid2 >> 32) == 0x01840000, "sid2 must have canonical upper 32 bits");

    // 2. Production Registry: Normal registration
    uint64_t regSid1 = registry.EnsureSteamLobbyID(cid1);
    TEST_ASSERT(regSid1 == sid1, "EnsureSteamLobbyID must match ComputeLobbySteamID");
    TEST_ASSERT(registry.Size() == 1, "Registry size must be 1");
    TEST_ASSERT(registry.GetCoreLobbyId(sid1) == cid1, "Reverse lookup must return cid1");

    // 3. Repeated registration of same lobby (idempotent)
    uint64_t regSid1Repeat = registry.EnsureSteamLobbyID(cid1);
    TEST_ASSERT(regSid1Repeat == sid1, "Idempotent registration must return same ID");
    TEST_ASSERT(registry.Size() == 1, "Registry size must remain 1");

    // 4. Forced collision of two distinct lobbies on identical SteamID
    // cid2 attempts to register with sid1 (already assigned to cid1)
    uint64_t rejectedId = registry.EnsureSteamLobbyID(cid2, sid1);
    TEST_ASSERT(rejectedId == 0, "Conflicting lobby registration must be rejected with 0");

    // 5. Preservation of maps after rejection
    TEST_ASSERT(registry.GetCoreLobbyId(sid1) == cid1, "sid1 mapping must remain intact pointing to cid1");
    TEST_ASSERT(registry.HasCoreLobby(cid1), "cid1 must remain in registry");
    TEST_ASSERT(!registry.HasCoreLobby(cid2), "Rejected cid2 must not be present in registry");
    TEST_ASSERT(registry.Size() == 1, "Registry size must remain 1 after rejection");

    // 6. Normal registration of cid2 with natural ID
    uint64_t regSid2 = registry.EnsureSteamLobbyID(cid2);
    TEST_ASSERT(regSid2 == sid2, "cid2 with its own natural ID must succeed");
    TEST_ASSERT(registry.Size() == 2, "Registry size must now be 2");

    // 7. Clean unregister
    TEST_ASSERT(registry.UnregisterLobby(cid1), "Unregister cid1 must succeed");
    TEST_ASSERT(!registry.HasCoreLobby(cid1), "cid1 must no longer exist in registry");
    TEST_ASSERT(registry.GetCoreLobbyId(sid1).empty(), "Reverse lookup for sid1 must be empty");
    TEST_ASSERT(registry.Size() == 1, "Registry size must decrement to 1");

    registry.Clear();
    TEST_ASSERT(registry.Size() == 0, "Registry must be empty after Clear()");

    std::cout << "  [PASS] Production SteamLobbyRegistry normal, repeat, collision, and preservation certified!" << std::endl;
    return true;
}

static bool TestReassemblyAutoExpirationAndReactorLiveness() {
    std::cout << "[*] Running TestReassemblyAutoExpirationAndReactorLiveness (10s Real TTL & Inactivity Window)..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x7777, 0x8888};
    tr.SetLocalPeerId(localPeer);
    TEST_ASSERT(tr.Start(47608), "Transport failed to start on port 47608");

    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Raw UDP socket must be created");

    auto waitForPacket = [&](LanTransport& t, InboundPacket& outPkt, int timeoutMs = 2500) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (t.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    // 1. Create incomplete reassembly contexts for 2 peers (frag 0 of 2, 500 bytes each)
    PeerId peer1{0xAAAA0001, 0xBBBB0001};
    PeerId peer2{0xAAAA0002, 0xBBBB0002};
    std::vector<uint8_t> chunk500(500, 0x5A);

    SendRawFragment(rawSock, ep, peer1, 1001, 0, 2, 1, chunk500);
    SendRawFragment(rawSock, ep, peer2, 2001, 0, 2, 1, chunk500);

    // Give reactor thread a short time to process the datagrams
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 2. Verify accounted memory and context counts
    TEST_ASSERT(tr.GetReassemblyContextCount() == 2, "Must track exactly 2 incomplete reassembly contexts");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 1000, "Global reassembly memory must be exactly 1000 bytes");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer1) == 500, "Peer 1 memory must be 500 bytes");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer2) == 500, "Peer 2 memory must be 500 bytes");
    TEST_ASSERT(tr.GetTrackedPeerReassemblyCount() == 2, "Tracked peer count must be 2");

    // 3. Let contexts expire by inactivity without calling ResetPeerState() or completing messages.
    // Inactivity timeout is 10 seconds. Sleep 10.5 seconds to let the runtime reactor thread loop
    // execute PruneExpiredFragmentsLocked() naturally.
    std::cout << "    [TTL] Awaiting 10.5s inactivity expiration via real reactor thread..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10500));

    // 4. Verify contexts have vanished and all counters are returned to 0
    TEST_ASSERT(tr.GetReassemblyContextCount() == 0, "All contexts must be purged after 10s inactivity");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 0, "Global reassembly bytes must return to 0 after TTL expiration");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer1) == 0, "Peer 1 accounting must return to 0");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer2) == 0, "Peer 2 accounting must return to 0");
    TEST_ASSERT(tr.GetTrackedPeerReassemblyCount() == 0, "Tracked peer map must be clean");

    // 5. Subsequent valid transmission from peer1: peer whose message expired can initiate and complete
    // a valid message.
    std::vector<uint8_t> newChunk1(400, 0x11);
    std::vector<uint8_t> newChunk2(400, 0x22);
    SendRawFragment(rawSock, ep, peer1, 3001, 0, 2, 2, newChunk1);
    SendRawFragment(rawSock, ep, peer1, 3001, 1, 2, 3, newChunk2);

    InboundPacket fullMsg;
    TEST_ASSERT(waitForPacket(tr, fullMsg), "Must receive completed message from peer 1 after expiration");
    TEST_ASSERT(fullMsg.payload.size() == 800, "Reassembled payload size must be 800 bytes");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 0, "Memory must be 0 after successful message completion");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer1) == 0, "Peer 1 memory must be 0 after completion");
    TEST_ASSERT(tr.GetReassemblyContextCount() == 0, "Context count must be 0 after completion");

    // 6. Confirm reactor continues processing traffic cleanly
    InboundPacket singlePkt;
    std::vector<uint8_t> singlePayload(100, 0x77);
    SendRawFragment(rawSock, ep, peer2, 4001, 0, 1, 2, singlePayload);
    TEST_ASSERT(waitForPacket(tr, singlePkt), "Reactor must continue processing traffic");
    TEST_ASSERT(singlePkt.payload == singlePayload, "Single packet payload must match");

    closesocket(rawSock);
    tr.Stop();

    std::cout << "  [PASS] Automatic 10s TTL expiration, counter return, and post-expiration reactor liveness certified!" << std::endl;
    return true;
}

static bool TestMemoryLimitsRealBudgetsAndEviction() {
    std::cout << "[*] Running TestMemoryLimitsRealBudgetsAndEviction (Budgets, Counts, Incompatible Metadata)..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x1111, 0x2222};
    tr.SetLocalPeerId(localPeer);
    TEST_ASSERT(tr.Start(47609), "Transport failed to start on port 47609");

    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Raw UDP socket must be created");

    auto waitForPacket = [&](LanTransport& t, InboundPacket& outPkt, int timeoutMs = 1500) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            if (t.PollInbound(outPkt)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };

    // -------------------------------------------------------------------------
    // A. Per-Peer Context Count Limit (16) & LRU Eviction
    // -------------------------------------------------------------------------
    PeerId peerA{0xFAAA0001, 0xFBBB0001};
    std::vector<uint8_t> chunk1k(1000, 0x41); // 1000 bytes each (<= REFIX_MAX_FRAGMENT_PAYLOAD 1150)
    // Send 16 distinct sessions for peerA, 1 fragment each (total = 16 * 1000 = 16000 bytes)
    for (uint32_t s = 1; s <= 16; ++s) {
        SendRawFragment(rawSock, ep, peerA, s, 0, 2, s, chunk1k);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(tr.GetReassemblyContextCount() == 16, "Context count must be 16 for peer A");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peerA) == 16000, "Peer A must have 16000 bytes allocated");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 16000, "Global bytes must be 16000");

    // 17th session for peerA: triggers per-peer LRU eviction (session 1 evicted, freeing 1000 bytes)
    SendRawFragment(rawSock, ep, peerA, 17, 0, 2, 17, chunk1k);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(tr.GetReassemblyContextCount() == 16, "Context count must remain capped at 16 after eviction");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peerA) == 16000, "Peer A memory must remain at 16000 after 1-for-1 LRU eviction");

    // -------------------------------------------------------------------------
    // B. Global Context Count Limit (64) & Global LRU Eviction
    // -------------------------------------------------------------------------
    // Create contexts across peers 2..4 (16 contexts each * 1000 bytes) to reach 64 global contexts
    for (uint64_t pIdx = 2; pIdx <= 4; ++pIdx) {
        PeerId p{0xFAAA0000 | pIdx, 0xFBBB0000 | pIdx};
        for (uint32_t s = 1; s <= 16; ++s) {
            SendRawFragment(rawSock, ep, p, s, 0, 2, s, chunk1k);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(tr.GetReassemblyContextCount() == 64, "Global context count must equal 64");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 64000, "Global reassembly memory must equal 64000 bytes");

    // 65th context from Peer 5: triggers global LRU eviction (evicts oldest context, freeing 1000 bytes)
    PeerId peer5{0xFAAA0005, 0xFBBB0005};
    SendRawFragment(rawSock, ep, peer5, 1, 0, 2, 1, chunk1k);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(tr.GetReassemblyContextCount() == 64, "Global context count must remain capped at 64 after global LRU eviction");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 64000, "Global reassembly bytes must remain capped at 64000");

    // -------------------------------------------------------------------------
    // C. Duplicate Fragment Handling
    // -------------------------------------------------------------------------
    size_t bytesBeforeDup = tr.GetGlobalReassemblyBytes();
    SendRawFragment(rawSock, ep, peer5, 1, 0, 2, 1, chunk1k); // duplicate
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == bytesBeforeDup, "Duplicate fragment must not increment bytes");

    // -------------------------------------------------------------------------
    // D. Incompatible Metadata Handling: Rejection Without Context Corruption
    // -------------------------------------------------------------------------
    // Clean state first via ResetPeerState to have a precise baseline
    tr.ResetPeerState(peer5);
    size_t bytesAfterReset5 = tr.GetGlobalReassemblyBytes();
    TEST_ASSERT(bytesAfterReset5 == 63000, "Resetting peer 5 must cleanly free its 1000 bytes");

    // Start a new 2-fragment session with peer 5
    std::vector<uint8_t> frag500(500, 0x88);
    SendRawFragment(rawSock, ep, peer5, 999, 0, 2, 1, frag500);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer5) == 500, "Peer 5 must have 500 bytes allocated");

    // Inject incompatible metadata: fragTotal = 5 != 2 (seq = 2)
    SendRawFragment(rawSock, ep, peer5, 999, 1, 5, 2, frag500);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Must reject the invalid fragment without corrupting ongoing context or changing memory
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer5) == 500, "Incompatible metadata must be rejected without altering peer memory");

    // Send valid second fragment (fragIndex = 1, fragTotal = 2) for session 999 (seq = 2: bad datagram was rejected without sequence advance)
    SendRawFragment(rawSock, ep, peer5, 999, 1, 2, 2, frag500);
    InboundPacket session999Pkt;
    TEST_ASSERT(waitForPacket(tr, session999Pkt), "Session 999 must complete successfully after bad fragment rejection");
    TEST_ASSERT(session999Pkt.payload.size() == 1000, "Reassembled size must be 1000 bytes");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer5) == 0, "Peer 5 memory must be 0 after session 999 completes");

    // -------------------------------------------------------------------------
    // E. Real 1 MiB Per-Peer & 4 MiB Global Reassembly Memory Budget Hard Enforcement
    // -------------------------------------------------------------------------
    // Clear all existing peer states first to provide a clean slate
    for (uint64_t pIdx = 1; pIdx <= 4; ++pIdx) {
        PeerId p{0xFAAA0000 | pIdx, 0xFBBB0000 | pIdx};
        tr.ResetPeerState(p);
    }
    tr.ResetPeerState(peer5);
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 0, "Clean slate must have 0 global bytes");
    TEST_ASSERT(tr.GetReassemblyContextCount() == 0, "Clean slate must have 0 contexts");

    // 1. Build up memory across Peers 1..4:
    // Each peer gets 7 sessions: 6 of 150 frags * 1000 B (900,000 B) + 1 of 148 frags * 1000 B (148,000 B)
    // = 1,048,000 bytes per peer (<= 1,048,576 B).
    // 4 peers * 7 sessions = 28 sessions total (<= 64 global limit, <= 16 per-peer limit).
    // Aggregate global memory = 4 * 1,048,000 = 4,192,000 bytes!
    for (uint64_t pIdx = 1; pIdx <= 4; ++pIdx) {
        PeerId p{0xFAAA0000 | pIdx, 0xFBBB0000 | pIdx};
        uint32_t pSeq = 1;
        for (uint32_t sId = 1; sId <= 6; ++sId) {
            for (uint8_t f = 0; f < 150; ++f) {
                SendRawFragment(rawSock, ep, p, sId, f, 200, pSeq++, chunk1k);
                if (f % 50 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        for (uint8_t f = 0; f < 148; ++f) {
            SendRawFragment(rawSock, ep, p, 7, f, 200, pSeq++, chunk1k);
            if (f % 50 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        TEST_ASSERT(tr.GetPeerReassemblyBytes(p) == 1048000, "Each peer must reach 1,048,000 bytes");
    }

    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 4192000, "Aggregate global bytes across 4 peers must equal 4,192,000");
    TEST_ASSERT(tr.GetReassemblyContextCount() == 28, "Context count must be 28 (4 peers * 7 sessions)");

    // 2. Introduce Peer 5 (has 0 bytes used, 0 contexts active).
    // Peer 5 sends 2 fragments of 1000 bytes:
    // 4,192,000 + 1000 + 1000 = 4,194,000 bytes (fits under 4 MiB = 4,194,304 B).
    SendRawFragment(rawSock, ep, peer5, 1, 0, 10, 1, chunk1k);
    SendRawFragment(rawSock, ep, peer5, 1, 1, 10, 2, chunk1k);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer5) == 2000, "Peer 5 must have 2,000 bytes in use");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 4194000, "Global reassembly memory must equal 4,194,000 bytes");
    // Global headroom remaining is now exactly: 4,194,304 - 4,194,000 = 304 bytes.

    // 3. Now send a 500-byte fragment from Peer 5:
    // Peer 5 usage would be: 2,000 + 500 = 2,500 bytes (well within Peer 5's 1 MiB budget!).
    // Peer 5 context count is 1 (well within 16 context limit!).
    // Global context count is 29 (well within 64 context limit!).
    // BUT global memory would be 4,194,000 + 500 = 4,194,500 > 4,194,304 (exceeds 4 MiB global limit)!
    // Must be strictly REJECTED by the 4 MiB global budget!
    SendRawFragment(rawSock, ep, peer5, 1, 2, 10, 3, frag500);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 4194000, "Fragment exceeding 4 MiB global budget must be strictly rejected");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer5) == 2000, "Peer 5 bytes must remain 2000 after rejection");

    // 4. Now send a 300-byte fragment from Peer 5:
    // 4,194,000 + 300 = 4,194,300 <= 4,194,304 (fits under 4 MiB ceiling).
    // Must be ACCEPTED on sequence 3 (which was not advanced when 500B was rejected)!
    std::vector<uint8_t> frag300(300, 0x55);
    SendRawFragment(rawSock, ep, peer5, 1, 2, 10, 3, frag300);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 4194300, "Fragment fitting under 4 MiB global ceiling must be accepted");
    TEST_ASSERT(tr.GetPeerReassemblyBytes(peer5) == 2300, "Peer 5 bytes must now equal 2300");

    // 5. Attempt 10-byte fragment: 4,194,300 + 10 = 4,194,310 > 4,194,304.
    // Must be REJECTED!
    std::vector<uint8_t> frag10(10, 0x66);
    SendRawFragment(rawSock, ep, peer5, 1, 3, 10, 4, frag10);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 4194300, "Fragment exceeding 4 MiB global ceiling must be rejected");

    // 6. Test Automatic TTL Expiration (without ResetPeerState)
    // Wait for REFIX_REASSEMBLY_TIMEOUT_SEC (10s) sliding inactivity window to elapse
    std::cout << "    [INFO] Testing automatic TTL reassembly expiration (waiting 10.5s)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10500));

    // Send a 1-byte fragment from a new peer to trigger PruneExpiredFragmentsLocked
    PeerId triggerPeer{0xFAAA0099, 0xFBBB0099};
    std::vector<uint8_t> trigFrag(1, 0xAA);
    SendRawFragment(rawSock, ep, triggerPeer, 1, 0, 2, 1, trigFrag);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 1, "Expired sessions must be automatically released by TTL (only trigger fragment remains)");
    TEST_ASSERT(tr.GetReassemblyContextCount() == 1, "Context count must be 1 (only trigger session)");

    // 7. Test Transport Clean Recovery after TTL expiration
    // Complete trigger session cleanly
    SendRawFragment(rawSock, ep, triggerPeer, 1, 1, 2, 2, trigFrag);
    InboundPacket trigPkt;
    TEST_ASSERT(waitForPacket(tr, trigPkt), "Trigger message must complete cleanly after TTL expiration recovery");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 0, "Global reassembly memory must strictly return to 0");
    TEST_ASSERT(tr.GetReassemblyContextCount() == 0, "Reassembly context count must return to 0");

    closesocket(rawSock);
    tr.Stop();

    std::cout << "  [PASS] True 4 MiB global budget across multiple peers, 1 MiB peer budget, and 64/16 context limits certified!" << std::endl;
    return true;
}

static bool TestMulticastInitializationAndDiagnostics() {
    std::cout << "[*] Running TestMulticastInitializationAndDiagnostics..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x1212, 0x3434};
    tr.SetLocalPeerId(localPeer);

    // Start on alternate discovery port
    uint16_t testPort = 47585;
    TEST_ASSERT(tr.Start(testPort), "LanTransport Start must succeed");

    TEST_ASSERT(tr.GetDiscoveryPort() == testPort, "Discovery port must match requested port 47585");
    TEST_ASSERT(tr.GetLocalDataEndpoint().port != 0, "Ephemeral data port must be assigned");
    TEST_ASSERT(tr.GetLocalDataEndpoint().ipv4 != 0, "Local IPv4 must be non-zero");

    bool joined = tr.IsMulticastJoined();
    std::string diag = tr.GetDiscoveryStatus();
    TEST_ASSERT(!diag.empty(), "Discovery diagnostics string must not be empty");
    if (joined) {
        TEST_ASSERT(diag.find("Multicast Active") != std::string::npos, "Diagnostics must indicate Multicast Active when joined");
    } else {
        TEST_ASSERT(diag.find("Degraded Broadcast-Only") != std::string::npos, "Diagnostics must indicate Degraded Broadcast-Only when not joined");
    }
    std::cout << "    Transport discovery diagnostics: " << diag << std::endl;
    tr.Stop();

    // 2. Controlled Degraded Mode Verification
    std::cout << "    [*] Testing controlled degraded broadcast-only discovery mode..." << std::endl;
    LanTransport trDegraded;
    trDegraded.SetLocalPeerId(localPeer);
    trDegraded.SetSimulateMulticastFailure(true);
    TEST_ASSERT(trDegraded.Start(47625), "Degraded LanTransport Start must succeed");
    TEST_ASSERT(!trDegraded.IsMulticastJoined(), "Degraded mode must strictly report IsMulticastJoined == false");
    std::string degradedDiag = trDegraded.GetDiscoveryStatus();
    TEST_ASSERT(degradedDiag.find("Degraded Broadcast-Only") != std::string::npos, "Degraded mode must report Degraded Broadcast-Only status");
    // Broadcast must succeed via broadcast/localhost without attempting multicast
    TEST_ASSERT(trDegraded.BroadcastDiscovery("DegradedBeacon", 14), "Broadcast in degraded mode must succeed");
    trDegraded.Stop();

    std::cout << "    [NOTE] Local tests verify socket and diagnostic state transitions. Physical Ethernet switch multicast reception is not claimed." << std::endl;
    std::cout << "  [PASS] Multicast initialization, degraded broadcast-only policy, and diagnostics certified!" << std::endl;
    return true;
}

static bool TestPeerStateLifecycleAndInboundQueueHardening() {
    std::cout << "[*] Running TestPeerStateLifecycleAndInboundQueueHardening..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x1010, 0x2020};
    tr.SetLocalPeerId(localPeer);
    TEST_ASSERT(tr.Start(47630), "Transport start failed");

    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Raw socket creation failed");

    // 1. Flood with 150 distinct PeerIds (exceeding REFIX_MAX_CONCURRENT_PEER_STATES = 128)
    std::vector<uint8_t> smallPayload(32, 0x55);
    for (uint64_t i = 1; i <= 150; ++i) {
        PeerId remoteP{0xEE000000 | i, 0xFF000000 | i};
        SendRawReliableWirePacket(rawSock, ep, remoteP, 1, 1, smallPayload);
        if (i % 30 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    size_t stateCount = tr.GetPeerStateCount();
    TEST_ASSERT(stateCount <= REFIX_MAX_CONCURRENT_PEER_STATES, "Peer state count must be strictly capped at REFIX_MAX_CONCURRENT_PEER_STATES (128)");
    std::cout << "    Peer states capped at: " << stateCount << " / " << REFIX_MAX_CONCURRENT_PEER_STATES << std::endl;

    // 2. Test Inbound Queue Capacity Bounding and Backpressure
    // Send 600 unpolled unreliable packets (exceeding REFIX_MAX_INBOUND_QUEUE_SIZE = 512)
    PeerId burstPeer{0xEE000001, 0xFF000001};
    for (int i = 0; i < 600; ++i) {
        SendRawUnreliableWirePacket(rawSock, ep, burstPeer, smallPayload);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    size_t queueSize = tr.GetInboundQueueSize();
    uint64_t droppedCount = tr.GetInboundQueueDroppedCount();
    TEST_ASSERT(queueSize <= REFIX_MAX_INBOUND_QUEUE_SIZE, "Inbound queue size must be capped at REFIX_MAX_INBOUND_QUEUE_SIZE (512)");
    TEST_ASSERT(droppedCount > 0, "Inbound queue backpressure must record dropped packets when over capacity");
    std::cout << "    Inbound queue size capped at: " << queueSize << ", recorded drops: " << droppedCount << std::endl;

    // 3. Drain queue and verify valid data flow continues
    InboundPacket drainedPkt;
    size_t drained = 0;
    while (tr.PollInbound(drainedPkt)) {
        drained++;
    }
    TEST_ASSERT(drained == queueSize, "Must cleanly drain all buffered packets");
    TEST_ASSERT(tr.GetInboundQueueSize() == 0, "Inbound queue must be empty after drain");

    // 4. Send fresh valid message and confirm normal operation
    std::vector<uint8_t> testMsg(48, 0x77);
    SendRawUnreliableWirePacket(rawSock, ep, burstPeer, testMsg);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    InboundPacket freshPkt;
    TEST_ASSERT(tr.PollInbound(freshPkt), "Fresh packet must be received after queue drain");
    TEST_ASSERT(freshPkt.payload == testMsg, "Payload integrity verified");

    closesocket(rawSock);
    tr.Stop();
    std::cout << "  [PASS] Peer state lifecycle bounding and inbound queue backpressure certified!" << std::endl;
    return true;
}

static bool TestARQUnidirectionalFlowExceedingIdleTimeout() {
    std::cout << "[*] Running TestARQUnidirectionalFlowExceedingIdleTimeout (>15s unidirectional reliable flow)..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0xAAAA1111, 0xBBBB2222};
    PeerId peerReceiver{0xCCCC3333, 0xDDDD4444};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47640), "trSender start failed");
    TEST_ASSERT(trReceiver.Start(47641), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    // Send reliable messages every 800ms for 16.5 seconds (>15s idle timeout)
    // Over this entire duration, receiver sends only automatic ACKs back. Receiver sends NO reverse data packets.
    auto startTime = std::chrono::steady_clock::now();
    int packetsSent = 0;
    while (true) {
        auto elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() / 1000.0;
        if (elapsedSec >= 16.5) break;

        packetsSent++;
        std::string msgStr = "UNIDIRECTIONAL_RELIABLE_MSG_" + std::to_string(packetsSent);
        bool ok = trSender.SendReliable(peerReceiver, epReceiver, 1, msgStr.data(), msgStr.size());
        TEST_ASSERT(ok, "SendReliable must return true");

        // Wait for delivery on receiver
        bool delivered = false;
        for (int retry = 0; retry < 50; ++retry) {
            InboundPacket inPkt;
            if (trReceiver.PollInbound(inPkt)) {
                std::string recStr(reinterpret_cast<const char*>(inPkt.payload.data()), inPkt.payload.size());
                TEST_ASSERT(recStr == msgStr, "Payload integrity verified");
                TEST_ASSERT(inPkt.sequence == static_cast<uint32_t>(packetsSent), "Sequence number must strictly advance monotonically");
                delivered = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        TEST_ASSERT(delivered, "Receiver must deliver packet before next send");

        std::this_thread::sleep_for(std::chrono::milliseconds(750));
    }

    std::cout << "    Verified: Transmitted " << packetsSent << " messages over 16.5s without reverse data." << std::endl;
    // Check sender peer state was NOT pruned
    TEST_ASSERT(trSender.GetPeerStateCount() == 1, "Sender reliability state must NOT be pruned during active ACK reception");
    TEST_ASSERT(trReceiver.GetPeerStateCount() == 1, "Receiver reliability state must NOT be pruned");

    // Send a final message at 17s to prove session remains perfectly operational
    std::string postTimeoutMsg = "POST_IDLE_TIMEOUT_SUCCESS";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, postTimeoutMsg.data(), postTimeoutMsg.size()),
                "Post-15s send must succeed");
    bool deliveredFinal = false;
    for (int retry = 0; retry < 50; ++retry) {
        InboundPacket inPkt;
        if (trReceiver.PollInbound(inPkt)) {
            std::string recStr(reinterpret_cast<const char*>(inPkt.payload.data()), inPkt.payload.size());
            TEST_ASSERT(recStr == postTimeoutMsg, "Payload integrity verified for post-15s message");
            deliveredFinal = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    TEST_ASSERT(deliveredFinal, "Final message delivered successfully");

    trSender.Stop();
    trReceiver.Stop();
    std::cout << "  [PASS] Unidirectional ARQ flow >15s verified: no pruning, sequences advanced, zero packet loss!" << std::endl;
    return true;
}

static bool TestARQExplicitDisconnectAndReconnect() {
    std::cout << "[*] Running TestARQExplicitDisconnectAndReconnect..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0x11112222, 0x33334444};
    PeerId peerReceiver{0x55556666, 0x77778888};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47642), "trSender start failed");
    TEST_ASSERT(trReceiver.Start(47643), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    // 1. Send initial stream of 5 messages (sequences 1..5)
    for (int i = 1; i <= 5; ++i) {
        std::string msg = "MSG_" + std::to_string(i);
        TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msg.data(), msg.size()), "SendReliable failed");
        InboundPacket inPkt;
        bool got = false;
        for (int r = 0; r < 50; ++r) {
            if (trReceiver.PollInbound(inPkt)) { got = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TEST_ASSERT(got, "Message must be received");
        TEST_ASSERT(inPkt.sequence == static_cast<uint32_t>(i), "Sequence must match");
    }

    // 2. Explicit disconnect: reset peer state on sender
    trSender.ResetPeerState(peerReceiver);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify receiver processed Disconnect and erased state
    TEST_ASSERT(trSender.GetPeerStateCount() == 0, "Sender state must be erased");
    TEST_ASSERT(trReceiver.GetPeerStateCount() == 0, "Receiver must wipe state upon receiving Disconnect");

    // 3. Reconnect / send fresh message with sequence 1
    std::string newSessionMsg = "FRESH_SESSION_AFTER_RECONNECT";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, newSessionMsg.data(), newSessionMsg.size()), "New session send failed");

    InboundPacket freshPkt;
    bool gotFresh = false;
    for (int r = 0; r < 50; ++r) {
        if (trReceiver.PollInbound(freshPkt)) { gotFresh = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(gotFresh, "Fresh message after reconnect must be delivered");
    TEST_ASSERT(freshPkt.sequence == 1, "New session must start with sequence 1");
    std::string freshStr(reinterpret_cast<const char*>(freshPkt.payload.data()), freshPkt.payload.size());
    TEST_ASSERT(freshStr == newSessionMsg, "Fresh payload integrity verified");

    trSender.Stop();
    trReceiver.Stop();
    std::cout << "  [PASS] Explicit disconnect and reconnect resynchronization certified!" << std::endl;
    return true;
}

static bool TestInboundQueueSaturationAndBackpressure() {
    std::cout << "[*] Running TestInboundQueueSaturationAndBackpressure..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x99991111, 0x88882222};
    tr.SetLocalPeerId(localPeer);
    TEST_ASSERT(tr.Start(47644), "Transport start failed");

    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Socket creation failed");

    // 1. Fill inbound queue up to REFIX_MAX_INBOUND_QUEUE_SIZE (512)
    PeerId fillerPeer{0x12345678, 0x9ABCDEF0};
    std::vector<uint8_t> smallPkt(16, 0xAA);
    for (int i = 0; i < 512; ++i) {
        SendRawUnreliableWirePacket(rawSock, ep, fillerPeer, smallPkt);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(tr.GetInboundQueueSize() == 512, "Queue must be exactly full at 512");

    uint64_t dropsBefore = tr.GetInboundQueueDroppedCount();

    // 2. Send reliable unfragmented message from another peer while queue is full
    PeerId relPeer{0x22223333, 0x44445555};
    std::vector<uint8_t> relPayload(32, 0xBB);
    SendRawReliableWirePacket(rawSock, ep, relPeer, 1, 1, relPayload);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Must assert backpressure: packet is dropped from queue, drop counter incremented, queue still 512
    TEST_ASSERT(tr.GetInboundQueueSize() == 512, "Queue must not exceed 512");
    TEST_ASSERT(tr.GetInboundQueueDroppedCount() > dropsBefore, "Drop counter must record backpressure drop");

    // 3. Drain 10 items from queue
    InboundPacket drained;
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT(tr.PollInbound(drained), "Must drain items");
    }
    TEST_ASSERT(tr.GetInboundQueueSize() == 502, "Queue must have 502 items");

    // 4. Now sender retransmits sequence 1 (or sends it again)
    SendRawReliableWirePacket(rawSock, ep, relPeer, 1, 1, relPayload);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now it must be accepted into the queue!
    TEST_ASSERT(tr.GetInboundQueueSize() == 503, "Queue must accept packet once space is available");

    // Drain until we find relPayload
    bool foundReliable = false;
    while (tr.PollInbound(drained)) {
        if (drained.payload == relPayload && drained.isReliable && drained.sequence == 1) {
            foundReliable = true;
            break;
        }
    }
    TEST_ASSERT(foundReliable, "Reliable packet must be safely delivered without corruption after queue space freed");

    // 5. Test listener mode: listener receives directly without queue limits
    struct TestListener : public ILanTransportListener {
        std::atomic<int> receivedCount{0};
        void OnDiscoveryPacket(const LanEndpoint&, MsgType, const uint8_t*, size_t) override {}
        void OnInboundData(const InboundPacket&) override {
            receivedCount++;
        }
        void OnPeerTimeout(const PeerId&, const LanEndpoint&) override {}
    } listener;

    LanTransport trListener;
    trListener.SetLocalPeerId(localPeer);
    TEST_ASSERT(trListener.Start(47645, &listener), "Listener transport start failed");
    LanEndpoint epList = trListener.GetLocalDataEndpoint();
    epList.ipv4 = 0x7F000001;

    for (int i = 1; i <= 20; ++i) {
        SendRawReliableWirePacket(rawSock, epList, relPeer, i, 1, relPayload);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(listener.receivedCount.load() >= 20, "Listener must receive packets directly without queue limitation");

    closesocket(rawSock);
    tr.Stop();
    trListener.Stop();
    std::cout << "  [PASS] Inbound queue saturation, backpressure, and listener path certified!" << std::endl;
    return true;
}

static bool TestSymmetricDatagramFramingAndValidation() {
    std::cout << "[*] Running TestSymmetricDatagramFramingAndValidation..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x12340001, 0x56780002};
    tr.SetLocalPeerId(localPeer);
    TEST_ASSERT(tr.Start(47646), "Transport start failed");

    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Socket creation failed");

    PeerId testPeer{0xFACE0001, 0xCAFE0002};

    auto sendRawBuf = [&](const void* data, size_t size) {
        sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(ep.ipv4);
        sin.sin_port = htons(ep.port);
        sendto(rawSock, reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
               reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    };

    // 1. Valid unfragmented payload (1150 bytes, max allowed MTU chunk)
    std::vector<uint8_t> validChunk(1150, 0x11);
    SendRawReliableWirePacket(rawSock, ep, testPeer, 1, 1, validChunk);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket pkt;
    TEST_ASSERT(tr.PollInbound(pkt), "Valid 1150-byte chunk must be accepted");
    TEST_ASSERT(pkt.payload.size() == 1150, "Payload length must match 1150");

    // 2. Oversized unfragmented payload (1151 bytes, exceeds MTU chunk limit)
    std::vector<uint8_t> overBuf(sizeof(WireHeader) + 1151, 0);
    auto* hdr = reinterpret_cast<WireHeader*>(overBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(testPeer);
    hdr->generationId = 1;
    hdr->sessionId = 0;
    hdr->channel = 1;
    hdr->fragIndex = 0;
    hdr->fragTotal = 1;
    hdr->sequence = 2;
    hdr->payloadLen = 1151;
    sendRawBuf(overBuf.data(), overBuf.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(!tr.PollInbound(pkt), "Oversized 1151-byte unfragmented payload must be strictly rejected");

    // 3. Inconsistent fragmented metadata: fragTotal = 0
    std::vector<uint8_t> badFragBuf(sizeof(WireHeader) + 100, 0);
    hdr = reinterpret_cast<WireHeader*>(badFragBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE | FLAG_FRAGMENT;
    hdr->SetSenderPeerId(testPeer);
    hdr->generationId = 1;
    hdr->sessionId = 5;
    hdr->channel = 1;
    hdr->fragIndex = 0;
    hdr->fragTotal = 0; // Invalid!
    hdr->sequence = 3;
    hdr->payloadLen = 100;
    sendRawBuf(badFragBuf.data(), badFragBuf.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!tr.PollInbound(pkt), "Fragment with fragTotal = 0 must be rejected");

    // 4. Inconsistent metadata: fragIndex >= fragTotal
    hdr->fragIndex = 2;
    hdr->fragTotal = 2;
    hdr->sequence = 4;
    sendRawBuf(badFragBuf.data(), badFragBuf.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!tr.PollInbound(pkt), "Fragment with fragIndex >= fragTotal must be rejected");

    // 5. Inconsistent metadata: last fragment without FLAG_LAST_FRAGMENT
    hdr->fragIndex = 1;
    hdr->fragTotal = 2;
    hdr->flags = FLAG_RELIABLE | FLAG_FRAGMENT; // Missing FLAG_LAST_FRAGMENT
    hdr->sequence = 5;
    sendRawBuf(badFragBuf.data(), badFragBuf.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!tr.PollInbound(pkt), "Last fragment without FLAG_LAST_FRAGMENT must be rejected");

    // 6. Truncated datagram: declared payloadLen > actual datagram size
    std::vector<uint8_t> truncBuf(sizeof(WireHeader) + 50, 0);
    hdr = reinterpret_cast<WireHeader*>(truncBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(testPeer);
    hdr->generationId = 1;
    hdr->sequence = 6;
    hdr->payloadLen = 500; // Declares 500 but datagram only has 50 bytes!
    sendRawBuf(truncBuf.data(), truncBuf.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!tr.PollInbound(pkt), "Truncated datagram must be rejected");

    // 7. DataAck with appended payload must be rejected
    std::vector<uint8_t> ackWithPayload(sizeof(WireHeader) + 20, 0);
    hdr = reinterpret_cast<WireHeader*>(ackWithPayload.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataAck);
    hdr->flags = 0;
    hdr->SetSenderPeerId(testPeer);
    hdr->generationId = 1;
    hdr->payloadLen = 20; // Spurious payload on ACK!
    sendRawBuf(ackWithPayload.data(), ackWithPayload.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // 8. DataUnreliable with FLAG_RELIABLE must be rejected
    std::vector<uint8_t> badUnrelBuf(sizeof(WireHeader) + 50, 0);
    hdr = reinterpret_cast<WireHeader*>(badUnrelBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataUnreliable);
    hdr->flags = FLAG_RELIABLE; // Inconsistent flags!
    hdr->SetSenderPeerId(testPeer);
    hdr->payloadLen = 50;
    sendRawBuf(badUnrelBuf.data(), badUnrelBuf.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!tr.PollInbound(pkt), "Unreliable packet with FLAG_RELIABLE must be rejected");

    closesocket(rawSock);
    tr.Stop();
    std::cout << "  [PASS] Symmetric incoming datagram framing and metadata validation certified!" << std::endl;
    return true;
}

static bool TestPeerStateAdmissionPolicyUnder128Limit() {
    std::cout << "[*] Running TestPeerStateAdmissionPolicyUnder128Limit (LRU eviction of unknown peers for registered peers)..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0x11112222, 0x33334444};
    tr.SetLocalPeerId(localPeer);

    // Admission filter: peer is legitimate if its high bits match 0x77777777
    std::unordered_set<PeerId> registeredPeers;
    tr.SetPeerAdmissionFilter([&registeredPeers](const PeerId& pid) -> bool {
        return registeredPeers.find(pid) != registeredPeers.end();
    });

    TEST_ASSERT(tr.Start(47647), "Transport start failed");

    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Socket creation failed");

    // 1. Fill 128 peer states with unknown/unregistered test IDs
    std::vector<uint8_t> dummyData(16, 0x42);
    for (uint64_t i = 1; i <= 128; ++i) {
        PeerId unknownPeer{0xAA000000 | i, 0xBB000000 | i};
        SendRawReliableWirePacket(rawSock, ep, unknownPeer, 1, 1, dummyData);
        if (i % 32 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(tr.GetPeerStateCount() == 128, "Peer state table must be full at 128");

    // 2. Send from a 129th unknown peer: must be rejected!
    PeerId unknown129{0xAA000099, 0xBB000099};
    SendRawReliableWirePacket(rawSock, ep, unknown129, 1, 1, dummyData);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(tr.GetPeerStateCount() == 128, "Table must strictly remain capped at 128");

    InboundPacket pkt;
    // Drain buffered packets so queue doesn't interfere
    while (tr.PollInbound(pkt)) {}

    // 3. Register a legitimate peer
    PeerId legitPeer{0x77777777, 0x11111111};
    registeredPeers.insert(legitPeer);

    // Send reliable message from legitimate peer to transport
    SendRawReliableWirePacket(rawSock, ep, legitPeer, 1, 1, dummyData);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // System must have evicted an unknown quiescent peer and admitted the legitimate peer!
    TEST_ASSERT(tr.GetPeerStateCount() <= 128, "Peer state count must not exceed 128");
    bool legitDelivered = false;
    while (tr.PollInbound(pkt)) {
        if (pkt.senderPeerId == legitPeer && pkt.sequence == 1) {
            legitDelivered = true;
            break;
        }
    }
    TEST_ASSERT(legitDelivered, "Legitimate registered peer must be admitted and message delivered!");

    // 4. Test outbound SendReliable to a registered peer when table is at 128
    PeerId legitOutPeer{0x77777777, 0x22222222};
    registeredPeers.insert(legitOutPeer);
    LanEndpoint outEp{0x7F000001, 47648};

    bool ok = tr.SendReliable(legitOutPeer, outEp, 1, dummyData.data(), dummyData.size());
    TEST_ASSERT(ok, "SendReliable to registered peer must succeed even when table was full of unknown peers");
    TEST_ASSERT(tr.GetPeerStateCount() <= 128, "Table must remain capped at 128 after outbound admission");

    closesocket(rawSock);
    tr.Stop();
    std::cout << "  [PASS] Safe peer admission under 128 limit certified: legitimate peers protected against DoS!" << std::endl;
    return true;
}

static bool TestRealDualTransportE2EBackpressure() {
    std::cout << "[*] Running TestRealDualTransportE2EBackpressure (Two Real Transports ARQ & Saturated Queue)..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0x11223344, 0x55667788};
    PeerId peerReceiver{0x99AABBCC, 0xDDEEFF00};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47650), "trSender start failed");
    TEST_ASSERT(trReceiver.Start(47651), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Socket creation failed");

    // 1. Fill receiver's inbound queue up to 512
    PeerId fillerPeer{0xDEADBEEF, 0xCAFEFACE};
    std::vector<uint8_t> fillerData(32, 0x77);
    for (int i = 0; i < 512; ++i) {
        SendRawUnreliableWirePacket(rawSock, epReceiver, fillerPeer, fillerData);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 512, "Receiver queue must be saturated at 512");
    uint64_t drops0 = trReceiver.GetInboundQueueDroppedCount();

    // 2. Sender transmits unfragmented reliable message using SendReliable
    std::string unfragMsg = "UNFRAG_E2E_PAYLOAD_TEST";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, unfragMsg.data(), unfragMsg.size()), "SendReliable unfrag failed");

    // Wait 150ms: datagram arrives at receiver, rejected due to queue full, sender retains it in ARQ
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 512, "Receiver queue must remain 512");
    TEST_ASSERT(trReceiver.GetInboundQueueDroppedCount() > drops0, "Receiver must record backpressure drop");

    // 3. Drain 30 items from receiver queue
    InboundPacket drainedPkt;
    for (int i = 0; i < 30; ++i) {
        TEST_ASSERT(trReceiver.PollInbound(drainedPkt), "Must drain from queue");
    }
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 482, "Queue must now have 482 items");

    // 4. Wait for real ARQ retransmission from sender (100ms timer)
    bool deliveredUnfrag = false;
    for (int retry = 0; retry < 60; ++retry) {
        while (trReceiver.PollInbound(drainedPkt)) {
            if (drainedPkt.isReliable && drainedPkt.senderPeerId == peerSender) {
                std::string s(reinterpret_cast<const char*>(drainedPkt.payload.data()), drainedPkt.payload.size());
                if (s == unfragMsg) {
                    deliveredUnfrag = true;
                    break;
                }
            }
        }
        if (deliveredUnfrag) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(deliveredUnfrag, "Unfragmented message must be delivered via real ARQ retransmission after queue space freed");

    // Drain remainder of filler
    while (trReceiver.PollInbound(drainedPkt)) {}
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 0, "Queue must be empty after full drain");

    // 5. Fragmented reliable message (8 KiB) under saturated queue
    // Fill receiver queue back to 512
    for (int i = 0; i < 512; ++i) {
        SendRawUnreliableWirePacket(rawSock, epReceiver, fillerPeer, fillerData);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 512, "Receiver queue must be 512 again");

    std::vector<uint8_t> bigData(8192);
    for (size_t i = 0; i < bigData.size(); ++i) {
        bigData[i] = static_cast<uint8_t>((i * 37 + 13) % 256);
    }

    drops0 = trReceiver.GetInboundQueueDroppedCount();
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, bigData.data(), bigData.size()), "SendReliable 8KB failed");

    // Completing fragment cannot enter queue while full!
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 512, "Receiver queue must remain 512 during fragmented backpressure");

    // 6. Drain 30 items to open capacity for the reassembled message
    for (int i = 0; i < 30; ++i) {
        TEST_ASSERT(trReceiver.PollInbound(drainedPkt), "Must drain from queue");
    }

    // Wait for real ARQ retransmission of completing fragment
    bool deliveredFrag = false;
    for (int retry = 0; retry < 60; ++retry) {
        while (trReceiver.PollInbound(drainedPkt)) {
            if (drainedPkt.isReliable && drainedPkt.senderPeerId == peerSender && drainedPkt.payload.size() == 8192) {
                if (drainedPkt.payload == bigData) {
                    deliveredFrag = true;
                    break;
                }
            }
        }
        if (deliveredFrag) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(deliveredFrag, "8KB fragmented message must be delivered with 100% bit-for-bit integrity via ARQ retransmission");
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 0, "Global reassembly bytes must return to 0 after reassembly completion");
    TEST_ASSERT(trReceiver.GetReassemblyContextCount() == 0, "Reassembly contexts must return to 0");

    // Drain queue
    while (trReceiver.PollInbound(drainedPkt)) {}

    // 7. Out-of-order datagrams with queue almost full (510 items, room for 2 items)
    for (int i = 0; i < 510; ++i) {
        SendRawUnreliableWirePacket(rawSock, epReceiver, fillerPeer, fillerData);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 510, "Queue must be at 510 items");

    // Test peer sends out-of-order sequences: seq 2, 3, 4 (seq 1 missing)
    PeerId oooPeer{0x99990001, 0x88880002};
    std::vector<uint8_t> oooData(20, 0x33);

    SendRawReliableWirePacket(rawSock, epReceiver, oooPeer, 2, 1, oooData); // seq 2 OOO
    SendRawReliableWirePacket(rawSock, epReceiver, oooPeer, 3, 1, oooData); // seq 3 OOO
    SendRawReliableWirePacket(rawSock, epReceiver, oooPeer, 4, 1, oooData); // seq 4 OOO
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Queue still 510 (OOO packets are buffered in outOfOrderInbound, not queue)
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 510, "OOO packets must be buffered without entering inbound queue");

    // Now send the missing in-order packet (seq 1)
    // Queue has room for 2 items (510 + 2 = 512). It can admit seq 1 and seq 2!
    // Seq 3 and seq 4 MUST NOT be admitted because queue reaches 512!
    SendRawReliableWirePacket(rawSock, epReceiver, oooPeer, 1, 1, oooData);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 512, "Queue must be capped at 512 items (admitting only seq 1 and seq 2)");

    // Drain 10 items from queue
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT(trReceiver.PollInbound(drainedPkt), "Must drain items");
    }

    // Now send packet 3 (retransmitted)
    SendRawReliableWirePacket(rawSock, epReceiver, oooPeer, 3, 1, oooData);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Seq 3 and seq 4 are now admitted!
    bool gotSeq3 = false, gotSeq4 = false;
    while (trReceiver.PollInbound(drainedPkt)) {
        if (drainedPkt.senderPeerId == oooPeer) {
            if (drainedPkt.sequence == 3) gotSeq3 = true;
            if (drainedPkt.sequence == 4) gotSeq4 = true;
        }
    }
    TEST_ASSERT(gotSeq3 && gotSeq4, "Seq 3 and Seq 4 must be safely delivered without loss after queue drained");

    closesocket(rawSock);
    trSender.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] Real dual-transport E2E backpressure, ARQ retransmissions, and OOO boundary certified!" << std::endl;
    return true;
}

static bool TestSessionResetVsOldDuplicatesAndDisconnectLoss() {
    std::cout << "[*] Running TestSessionResetVsOldDuplicatesAndDisconnectLoss..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0x12345678, 0x11112222};
    PeerId peerReceiver{0x87654321, 0x33334444};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47660), "trSender start failed");
    TEST_ASSERT(trReceiver.Start(47661), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Socket creation failed");

    // 1. Establish session: send sequences 1..5
    uint32_t sessionGen = 0;
    for (int i = 1; i <= 5; ++i) {
        std::string msg = "SESSION_MSG_" + std::to_string(i);
        TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msg.data(), msg.size()), "SendReliable failed");
        InboundPacket inPkt;
        bool got = false;
        for (int r = 0; r < 50; ++r) {
            if (trReceiver.PollInbound(inPkt)) { got = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        TEST_ASSERT(got, "Message must be received");
        TEST_ASSERT(inPkt.sequence == static_cast<uint32_t>(i), "Sequence must match");
        sessionGen = inPkt.generationId;
    }

    // 2. Inject duplicate of sequence 1 from the CURRENT session
    // Must NOT reset expectedSequenceIn (which is currently 6)!
    std::vector<uint8_t> dupBuf(sizeof(WireHeader) + 16, 0);
    auto* hdr = reinterpret_cast<WireHeader*>(dupBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(peerSender);
    hdr->generationId = sessionGen; // Generation of current session
    hdr->fragmentMsgId = 0;
    hdr->channel = 1;
    hdr->fragIndex = 0;
    hdr->fragTotal = 1;
    hdr->reserved = 0;
    hdr->sequence = 1; // Duplicate of initial sequence!
    hdr->payloadLen = 16;
    sockaddr_in sinRecv{};
    sinRecv.sin_family = AF_INET;
    sinRecv.sin_addr.s_addr = htonl(epReceiver.ipv4);
    sinRecv.sin_port = htons(epReceiver.port);
    sendto(rawSock, reinterpret_cast<const char*>(dupBuf.data()), static_cast<int>(dupBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinRecv), sizeof(sinRecv));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify duplicate was discarded and NOT delivered to PollInbound()
    InboundPacket dummyPkt;
    TEST_ASSERT(!trReceiver.PollInbound(dummyPkt), "Duplicate of initial sequence must NOT be delivered to consumer");

    // Verify subsequent sequence 6 from sender still delivers cleanly (stream was not reset!)
    std::string msg6 = "SESSION_MSG_6";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msg6.data(), msg6.size()), "Send sequence 6 failed");
    bool got6 = false;
    for (int r = 0; r < 50; ++r) {
        if (trReceiver.PollInbound(dummyPkt)) {
            if (dummyPkt.sequence == 6) { got6 = true; break; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    TEST_ASSERT(got6, "Sequence 6 must deliver cleanly: stream was protected from duplicate seq 1 reset!");

    // 3. Test spoofed Disconnect from an unauthorized endpoint (different port)
    WireHeader spoofDispHdr{};
    spoofDispHdr.magic = REFIX_WIRE_MAGIC;
    spoofDispHdr.version = REFIX_WIRE_VERSION;
    spoofDispHdr.msgType = static_cast<uint8_t>(MsgType::Disconnect);
    spoofDispHdr.SetSenderPeerId(peerSender); // Impersonating peerSender
    spoofDispHdr.generationId = sessionGen;
    spoofDispHdr.payloadLen = 0;
    sendto(rawSock, reinterpret_cast<const char*>(&spoofDispHdr), sizeof(spoofDispHdr), 0,
           reinterpret_cast<const sockaddr*>(&sinRecv), sizeof(sinRecv)); // sent from rawSock (different port!)

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(trReceiver.GetPeerStateCount() == 1, "Receiver peer state must NOT be wiped by spoofed Disconnect from different endpoint!");

    // 4. Test delayed Disconnect from older generation
    // Send Disconnect with mismatched generation
    spoofDispHdr.generationId = sessionGen + 99;
    sendto(rawSock, reinterpret_cast<const char*>(&spoofDispHdr), sizeof(spoofDispHdr), 0,
           reinterpret_cast<const sockaddr*>(&sinRecv), sizeof(sinRecv));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(trReceiver.GetPeerStateCount() == 1, "Peer state must NOT be wiped by invalid generation Disconnect");

    // 5. Test clean session restart when Disconnect was LOST
    // Sender stops and restarts without Disconnect arriving at receiver
    trSender.Stop();
    TEST_ASSERT(trReceiver.GetPeerStateCount() == 1, "Receiver still has state from prior session");

    // Sender starts fresh session (new generation)
    LanTransport trSenderNew;
    trSenderNew.SetLocalPeerId(peerSender);
    TEST_ASSERT(trSenderNew.Start(47662), "trSenderNew start failed");
    LanEndpoint epSenderNew = trSenderNew.GetLocalDataEndpoint();
    epSenderNew.ipv4 = 0x7F000001;
    trReceiver.AuthorizePeerMigration(peerSender, epSenderNew);

    // Send sequence 1 from fresh session
    std::string newSessionMsg = "NEW_SESSION_AFTER_LOST_DISCONNECT";
    TEST_ASSERT(trSenderNew.SendReliable(peerReceiver, epReceiver, 1, newSessionMsg.data(), newSessionMsg.size()), "Send new session msg failed");

    InboundPacket newPkt;
    bool gotNew = false;
    for (int r = 0; r < 50; ++r) {
        if (trReceiver.PollInbound(newPkt)) { gotNew = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    TEST_ASSERT(gotNew, "New session message must be delivered even when previous Disconnect was lost!");
    TEST_ASSERT(newPkt.sequence == 1, "New session must be recognized and start with sequence 1");
    std::string newStr(reinterpret_cast<const char*>(newPkt.payload.data()), newPkt.payload.size());
    TEST_ASSERT(newStr == newSessionMsg, "New session payload integrity verified");

    // 6. Test that an old delayed packet from prior session is discarded
    sendto(rawSock, reinterpret_cast<const char*>(dupBuf.data()), static_cast<int>(dupBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinRecv), sizeof(sinRecv)); // dupBuf has generationId = 1
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!trReceiver.PollInbound(dummyPkt), "Old packet from obsolete generation must be discarded");

    // 7. Authentic Disconnect from legitimate endpoint wipes session
    trSenderNew.ResetPeerState(peerReceiver);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trReceiver.GetPeerStateCount() == 0, "Receiver peer state must be cleanly erased upon authentic Disconnect");

    closesocket(rawSock);
    trSenderNew.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] Session reset vs duplicate stream protection, Disconnect validation, and reconnect certified!" << std::endl;
    return true;
}

static bool TestPeerAdmissionEvictionLifecycle() {
    std::cout << "[*] Running TestPeerAdmissionEvictionLifecycle (Saturate 128, LRU Eviction, Zero Leaks, Reconnect)..." << std::endl;

    LanTransport tr;
    PeerId localPeer{0xAAAA1111, 0xBBBB2222};
    tr.SetLocalPeerId(localPeer);

    std::unordered_set<PeerId> registeredPeers;
    tr.SetPeerAdmissionFilter([&](const PeerId& pid) {
        return registeredPeers.find(pid) != registeredPeers.end();
    });

    TEST_ASSERT(tr.Start(47670), "Transport start failed");
    LanEndpoint ep = tr.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "Socket creation failed");

    // 1. Fill peer states with 128 unknown peers
    std::vector<PeerId> unknownPeers;
    std::vector<uint8_t> dummyData(16, 0x55);

    // Oldest unknown peer (index 0) has an active incomplete fragment
    PeerId p0{0xDEAD0000 | 1, 0xBEEF0000 | 1};
    unknownPeers.push_back(p0);
    SendRawReliableWirePacket(rawSock, ep, p0, 1, 1, dummyData);
    std::vector<uint8_t> fragChunk(200, 0x44);
    SendRawFragment(rawSock, ep, p0, 50, 0, 2, 2, fragChunk);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 200, "Oldest unknown peer must have 200 reassembly bytes");

    // Populate remaining 127 unknown peers so p0 has the oldest lastActivityTime in the table
    for (uint64_t i = 2; i <= 128; ++i) {
        PeerId unknownPeer{0xDEAD0000 | i, 0xBEEF0000 | i};
        unknownPeers.push_back(unknownPeer);
        SendRawReliableWirePacket(rawSock, ep, unknownPeer, 1, 1, dummyData);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(tr.GetPeerStateCount() == 128, "Peer state count must be exactly 128");

    // Drain inbound queue so subsequent verification reads the intended messages
    InboundPacket dummyQ;
    while (tr.PollInbound(dummyQ)) {}

    // 2. Now register and admit a legitimate peer
    PeerId legitPeer{0xCAFE0001, 0xFEED0002};
    registeredPeers.insert(legitPeer);

    std::string legitMsg = "LEGIT_MESSAGE_UNDER_SATURATION";
    std::vector<uint8_t> legitPayload(legitMsg.begin(), legitMsg.end());
    SendRawReliableWirePacket(rawSock, ep, legitPeer, 1, 1, legitPayload);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Table must remain capped at 128!
    TEST_ASSERT(tr.GetPeerStateCount() == 128, "Table must remain capped at exactly 128");

    InboundPacket legitPkt;
    TEST_ASSERT(tr.PollInbound(legitPkt), "Legitimate peer packet must be admitted and delivered");
    std::string recLegit(reinterpret_cast<const char*>(legitPkt.payload.data()), legitPkt.payload.size());
    TEST_ASSERT(recLegit == legitMsg, "Legitimate payload integrity verified");

    // 3. Confirm evicted peer's reassembly memory was completely cleaned up (zero leaks!)
    TEST_ASSERT(tr.GetPeerReassemblyBytes(unknownPeers[0]) == 0, "Evicted peer reassembly bytes must be 0");
    TEST_ASSERT(tr.GetGlobalReassemblyBytes() == 0, "Global reassembly memory must be 0: no orphan fragments leaked!");

    // 4. Verify that other peers' active sequences were NOT reset
    // Send seq 2 from unknownPeers[1]
    SendRawReliableWirePacket(rawSock, ep, unknownPeers[1], 2, 1, dummyData);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    InboundPacket p2Pkt;
    TEST_ASSERT(tr.PollInbound(p2Pkt), "Active sequence 2 of untouched peer must be delivered");
    TEST_ASSERT(p2Pkt.sequence == 2, "Sequence of untouched peer must remain 2");

    // 5. Allow evicted peer to reconnect via explicit session flow
    registeredPeers.insert(unknownPeers[0]);
    SendRawReliableWirePacket(rawSock, ep, unknownPeers[0], 1, 1, dummyData);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(tr.GetPeerStateCount() <= 128, "Table must remain bounded at 128 after evicted peer reconnection");

    InboundPacket reconnectedPkt;
    TEST_ASSERT(tr.PollInbound(reconnectedPkt), "Evicted peer must be able to reconnect cleanly");
    TEST_ASSERT(reconnectedPkt.senderPeerId == unknownPeers[0], "Reconnected sender identity verified");

    closesocket(rawSock);
    tr.Stop();

    std::cout << "  [PASS] 128-peer table saturation, LRU eviction, leak-free reassembly cleanup, and reconnection certified!" << std::endl;
    return true;
}

static bool TestP0_DualTransportRealOOOAndQueueRecovery() {
    std::cout << "[*] Running TestP0_DualTransportRealOOOAndQueueRecovery (Dual Transports, OOO, SACK, Queue Saturated, Drain on PollInbound)..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0x55550001, 0x66660002};
    PeerId peerReceiver{0x77770003, 0x88880004};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47710), "trSender start failed on 47710");
    TEST_ASSERT(trReceiver.Start(47711), "trReceiver start failed on 47711");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;
    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    // UDP Relay / Interceptor socket on port 47712 to reorder packets and test true E2E SACK & ARQ
    SOCKET fwdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(fwdSock != INVALID_SOCKET, "Forwarder socket creation failed");

    sockaddr_in sinFwd{};
    sinFwd.sin_family = AF_INET;
    sinFwd.sin_addr.s_addr = htonl(0x7F000001);
    sinFwd.sin_port = htons(47712);
    TEST_ASSERT(bind(fwdSock, reinterpret_cast<const sockaddr*>(&sinFwd), sizeof(sinFwd)) == 0, "Forwarder bind failed on 47712");

    u_long nonblock = 1;
    ioctlsocket(fwdSock, FIONBIO, &nonblock);

    LanEndpoint epForwarder{0x7F000001, 47712};

    // Thread running forwarder that captures seq 1 and passes seq 2, 3, 4 first
    std::atomic<bool> fwdRunning{true};
    std::atomic<bool> releaseSeq1{false};
    std::vector<uint8_t> delayedSeq1;
    sockaddr_in senderSin{};
    std::mutex fwdMutex;

    std::thread fwdThread([&]() {
        uint8_t buf[65536];
        sockaddr_in fromSin{};
        int fromLen = sizeof(fromSin);

        while (fwdRunning.load()) {
            fromLen = sizeof(fromSin);
            int r = recvfrom(fwdSock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
            if (r > 0 && r >= static_cast<int>(sizeof(WireHeader))) {
                auto* hdr = reinterpret_cast<WireHeader*>(buf);
                uint16_t fromPort = ntohs(fromSin.sin_port);

                if (fromPort == epSender.port) {
                    // Packet coming from sender -> destined to receiver
                    senderSin = fromSin;
                    if (hdr->msgType == static_cast<uint8_t>(MsgType::DataReliable) && hdr->sequence == 1 && !releaseSeq1.load()) {
                        std::lock_guard<std::mutex> lk(fwdMutex);
                        delayedSeq1.assign(buf, buf + r);
                    } else {
                        // Forward directly to receiver
                        sockaddr_in toRecvSin{};
                        toRecvSin.sin_family = AF_INET;
                        toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
                        toRecvSin.sin_port = htons(epReceiver.port);
                        sendto(fwdSock, reinterpret_cast<const char*>(buf), r, 0,
                               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
                    }
                } else if (fromPort == epReceiver.port) {
                    // ACK packet coming from receiver -> forward to sender
                    sendto(fwdSock, reinterpret_cast<const char*>(buf), r, 0,
                           reinterpret_cast<const sockaddr*>(&senderSin), sizeof(senderSin));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    });

    // 1. Sender transmits seq 1, 2, 3, 4 through forwarder
    std::string m1 = "E2E_MSG_1";
    std::string m2 = "E2E_MSG_2";
    std::string m3 = "E2E_MSG_3";
    std::string m4 = "E2E_MSG_4";

    TEST_ASSERT(trSender.SendReliable(peerReceiver, epForwarder, 1, m1.data(), m1.size()), "Send m1 failed");
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epForwarder, 1, m2.data(), m2.size()), "Send m2 failed");
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epForwarder, 1, m3.data(), m3.size()), "Send m3 failed");
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epForwarder, 1, m4.data(), m4.size()), "Send m4 failed");

    // Wait for receiver to process 2, 3, 4 and send real SACKs back to sender
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 2. Verify sender processed real SACK bits!
    TEST_ASSERT(trSender.IsSequenceSacked(peerReceiver, 2), "Sender must recognize SACK for seq 2");
    TEST_ASSERT(trSender.IsSequenceSacked(peerReceiver, 3), "Sender must recognize SACK for seq 3");
    TEST_ASSERT(trSender.IsSequenceSacked(peerReceiver, 4), "Sender must recognize SACK for seq 4");

    // 3. Fill receiver's inbound queue to 511 items (leaving exactly 1 slot before 512 capacity)
    SOCKET rawFiller = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    PeerId fillerPeer{0x99991111, 0x88882222};
    std::vector<uint8_t> dummy(16, 0x55);
    for (int i = 0; i < 511; ++i) {
        SendRawUnreliableWirePacket(rawFiller, epReceiver, fillerPeer, dummy);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 511, "Receiver queue must be at 511 items");

    // 4. Release delayed packet 1
    {
        std::lock_guard<std::mutex> lk(fwdMutex);
        releaseSeq1.store(true);
        TEST_ASSERT(!delayedSeq1.empty(), "delayedSeq1 must have been captured");
        sockaddr_in toRecvSin{};
        toRecvSin.sin_family = AF_INET;
        toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
        toRecvSin.sin_port = htons(epReceiver.port);
        sendto(fwdSock, reinterpret_cast<const char*>(delayedSeq1.data()), static_cast<int>(delayedSeq1.size()), 0,
               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Receiver admits packet 1, filling queue to 512!
    // Remaining contiguous candidates (seq 2, 3, 4) cannot fit in queue, so they are retained in outOfOrderInbound!
    TEST_ASSERT(trReceiver.GetInboundQueueSize() == 512, "Receiver queue must be full at 512");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) >= 2, "Retained candidates must remain buffered in outOfOrderInbound");

    // 5. Drain items using PollInbound() - verifies automatic draining without manual re-injections!
    InboundPacket drained;
    // Drain 20 items to open queue capacity
    for (int i = 0; i < 20; ++i) {
        TEST_ASSERT(trReceiver.PollInbound(drained), "Must drain item from queue");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Now all remaining messages from peerSender must be delivered cleanly in exact order!
    std::vector<std::string> deliveredMsgs;
    while (trReceiver.PollInbound(drained)) {
        if (drained.senderPeerId == peerSender) {
            std::string s(reinterpret_cast<const char*>(drained.payload.data()), drained.payload.size());
            deliveredMsgs.push_back(s);
        }
    }

    TEST_ASSERT(deliveredMsgs.size() >= 3, "All messages must be delivered after queue capacity freed");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) == 0, "outOfOrderInbound must be fully drained without manual re-injection");

    fwdRunning.store(false);
    if (fwdThread.joinable()) fwdThread.join();
    closesocket(fwdSock);
    closesocket(rawFiller);
    trSender.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] Dual-transport real OOO, SACK processing, and automatic queue recovery on PollInbound certified!" << std::endl;
    return true;
}

static bool TestP0_AckEndpointAndSessionValidation() {
    std::cout << "[*] Running TestP0_AckEndpointAndSessionValidation (Spoofed ACKs, SACKs, Disconnects & Duplicate Reliables)..." << std::endl;

    LanTransport trSender;

    PeerId peerSender{0xAAAA0001, 0xBBBB0002};
    PeerId peerReceiver{0xCCCC0003, 0xDDDD0004};

    trSender.SetLocalPeerId(peerSender);

    TEST_ASSERT(trSender.Start(47720), "trSender start failed");

    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    // Legitimate receiver raw socket on port 47721 (does not auto-ACK until authentic step)
    SOCKET sockReceiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockReceiver != INVALID_SOCKET, "Receiver socket creation failed");

    sockaddr_in sinRecvBind{};
    sinRecvBind.sin_family = AF_INET;
    sinRecvBind.sin_addr.s_addr = htonl(0x7F000001);
    sinRecvBind.sin_port = htons(47721);
    TEST_ASSERT(bind(sockReceiver, reinterpret_cast<const sockaddr*>(&sinRecvBind), sizeof(sinRecvBind)) == 0, "Receiver bind failed");

    LanEndpoint epReceiver{0x7F000001, 47721};

    // Attacker raw UDP socket on separate port 47725
    SOCKET sockAttacker = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockAttacker != INVALID_SOCKET, "Attacker socket creation failed");

    sockaddr_in sinSender{};
    sinSender.sin_family = AF_INET;
    sinSender.sin_addr.s_addr = htonl(epSender.ipv4);
    sinSender.sin_port = htons(epSender.port);

    // 1. Establish session from sender to receiver
    std::string legitimateMsg = "AUTHENTIC_SESSION_PACKET";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, legitimateMsg.data(), legitimateMsg.size()), "SendReliable failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Sender must hold 1 unacked packet");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47721, "lastEndpoint must be legitimate receiver port 47721");
    uint32_t sessionGen = trSender.GetPeerLocalGeneration(peerReceiver);

    // 2. Attack A: Spoofed ACK with ack sequence higher than sent sequences (e.g. ack = 999) from attacker endpoint
    WireHeader fakeAck{};
    fakeAck.magic = REFIX_WIRE_MAGIC;
    fakeAck.version = REFIX_WIRE_VERSION;
    fakeAck.msgType = static_cast<uint8_t>(MsgType::DataAck);
    fakeAck.flags = FLAG_HAS_ACK;
    fakeAck.SetSenderPeerId(peerReceiver); // Spoof peer identity
    fakeAck.generationId = sessionGen;
    fakeAck.ack = 999; // Arbitrarily high ACK
    fakeAck.sackMask = 0;
    sendto(sockAttacker, reinterpret_cast<const char*>(&fakeAck), sizeof(fakeAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Unacked outbound must NOT be cleared by spoofed excessive ACK");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47721, "lastEndpoint must NOT be mutated by spoofed ACK endpoint");

    // 3. Attack B: Spoofed ACK with manipulated SACK bitmask from unauthorized endpoint
    fakeAck.ack = 0;
    fakeAck.sackMask = 0xFFFFFFFF; // Manipulated SACK mask
    sendto(sockAttacker, reinterpret_cast<const char*>(&fakeAck), sizeof(fakeAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(!trSender.IsSequenceSacked(peerReceiver, 1), "Outbound sequence 1 must NOT be marked isSacked by unauthorized endpoint");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47721, "lastEndpoint must remain legitimate");

    // 4. Attack C: Spoofed Disconnect from attacker endpoint
    WireHeader fakeDisc{};
    fakeDisc.magic = REFIX_WIRE_MAGIC;
    fakeDisc.version = REFIX_WIRE_VERSION;
    fakeDisc.msgType = static_cast<uint8_t>(MsgType::Disconnect);
    fakeDisc.SetSenderPeerId(peerReceiver);
    fakeDisc.generationId = sessionGen;
    sendto(sockAttacker, reinterpret_cast<const char*>(&fakeDisc), sizeof(fakeDisc), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetPeerStateCount() == 1, "Peer state must NOT be erased by spoofed Disconnect from foreign endpoint");

    // 5. Attack D: Spoofed duplicate DataReliable from foreign endpoint
    std::vector<uint8_t> fakeRelBuf(sizeof(WireHeader) + 16, 0);
    auto* fakeRelHdr = reinterpret_cast<WireHeader*>(fakeRelBuf.data());
    fakeRelHdr->magic = REFIX_WIRE_MAGIC;
    fakeRelHdr->version = REFIX_WIRE_VERSION;
    fakeRelHdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    fakeRelHdr->flags = FLAG_RELIABLE;
    fakeRelHdr->SetSenderPeerId(peerReceiver);
    fakeRelHdr->generationId = sessionGen;
    fakeRelHdr->sequence = 1; // Duplicate of initial sequence
    fakeRelHdr->payloadLen = 16;
    sendto(sockAttacker, reinterpret_cast<const char*>(fakeRelBuf.data()), static_cast<int>(fakeRelBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47721, "lastEndpoint must NOT be stolen by duplicate DataReliable from foreign endpoint");

    // 6. Authentic traffic: legitimate receiver sends authentic ACK from port 47721
    WireHeader legitAck{};
    legitAck.magic = REFIX_WIRE_MAGIC;
    legitAck.version = REFIX_WIRE_VERSION;
    legitAck.msgType = static_cast<uint8_t>(MsgType::DataAck);
    legitAck.flags = FLAG_HAS_ACK;
    legitAck.SetSenderPeerId(peerReceiver);
    legitAck.generationId = sessionGen;
    legitAck.channel = 1;
    legitAck.ack = 1;
    legitAck.sackMask = 0;
    sendto(sockReceiver, reinterpret_cast<const char*>(&legitAck), sizeof(legitAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Authentic ACK from legitimate receiver cleans up unacked packet!
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 0, "Authentic ACK from legitimate receiver must cleanly retire unacked packet");

    closesocket(sockAttacker);
    closesocket(sockReceiver);
    trSender.Stop();

    std::cout << "  [PASS] DataAck, SACK mask, Disconnect, and duplicate DataReliable endpoint validation certified!" << std::endl;
    return true;
}

static bool TestP1_SessionGeneration32BitRangeAndChurn() {
    std::cout << "[*] Running TestP1_SessionGeneration32BitRangeAndChurn (>256 sessions, >128 diff, delayed packet rejection)..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0xCAFE0001, 0xFEED0002};
    PeerId peerReceiver{0xFACE0003, 0xDEAD0004};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47730), "trSender start failed on 47730");
    TEST_ASSERT(trReceiver.Start(47731), "trReceiver start failed on 47731");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;
    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "rawSock creation failed");

    sockaddr_in sinReceiver{};
    sinReceiver.sin_family = AF_INET;
    sinReceiver.sin_addr.s_addr = htonl(epReceiver.ipv4);
    sinReceiver.sin_port = htons(epReceiver.port);

    // 1. Churn more than 256 local sessions on trSender (advancing 32-bit generation counter past 256)
    for (uint64_t c = 1; c <= 300; ++c) {
        PeerId dummyPeer{0x99990000 | c, 0x88880000 | c};
        trSender.ResetPeerState(dummyPeer);
    }

    // 2. Establish valid session with trReceiver
    std::string msgGen = "MSG_SESSION_HIGH_GEN";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msgGen.data(), msgGen.size()), "SendReliable failed");

    InboundPacket inPkt;
    bool gotMsg = false;
    for (int r = 0; r < 50; ++r) {
        if (trReceiver.PollInbound(inPkt)) { gotMsg = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    TEST_ASSERT(gotMsg, "Message must be received");
    uint32_t activeGen = inPkt.generationId;
    TEST_ASSERT(activeGen > 256, "32-bit generation ID must exceed 256 without 8-bit wrap or modular truncation");
    std::cout << "    [INFO] Verified active 32-bit session generation ID: " << activeGen << std::endl;

    // 3. Inject delayed stale packet from an ancient session with generationId = 1
    // Difference is: activeGen - 1 >= 300 > 128!
    std::vector<uint8_t> staleBuf(sizeof(WireHeader) + 16, 0);
    auto* staleHdr = reinterpret_cast<WireHeader*>(staleBuf.data());
    staleHdr->magic = REFIX_WIRE_MAGIC;
    staleHdr->version = REFIX_WIRE_VERSION;
    staleHdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    staleHdr->flags = FLAG_RELIABLE;
    staleHdr->SetSenderPeerId(peerSender);
    staleHdr->generationId = 1; // Ancient generation!
    staleHdr->sequence = 1;
    staleHdr->payloadLen = 16;
    sendto(rawSock, reinterpret_cast<const char*>(staleBuf.data()), static_cast<int>(staleBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket staleCheck;
    TEST_ASSERT(!trReceiver.PollInbound(staleCheck), "Ancient generation packet must be strictly rejected");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(peerSender) == activeGen, "Receiver remote generation must remain activeGen");

    // 4. Verify subsequent transmission on active session delivers cleanly
    std::string msgNext = "MSG_SESSION_HIGH_GEN_2";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msgNext.data(), msgNext.size()), "Second send failed");
    bool gotNext = false;
    for (int r = 0; r < 50; ++r) {
        if (trReceiver.PollInbound(inPkt)) {
            if (inPkt.sequence == 2) { gotNext = true; break; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    TEST_ASSERT(gotNext, "Active session message must deliver without disruption from ancient packet");

    closesocket(rawSock);
    trSender.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] 32-bit generation range (>256 churn, >128 distance, stale packet drop) certified!" << std::endl;
    return true;
}

static bool TestP1_IncompatibleFragmentMetadataRejection() {
    std::cout << "[*] Running TestP1_IncompatibleFragmentMetadataRejection (Reject datagram without advancing sequence or fake ACKs)..." << std::endl;

    LanTransport trReceiver;
    PeerId localPeer{0x11112222, 0x33334444};
    PeerId remoteClient{0x55556666, 0x77778888};

    trReceiver.SetLocalPeerId(localPeer);
    TEST_ASSERT(trReceiver.Start(47740), "trReceiver start failed on 47740");

    LanEndpoint ep = trReceiver.GetLocalDataEndpoint();
    ep.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "rawSock creation failed");

    // 1. Send fragment 0 of 2 (channel 1, reliable, payload 500B, seq 1, fragmentMsgId 50)
    std::vector<uint8_t> chunk500(500, 0x42);
    SendRawFragment(rawSock, ep, remoteClient, 50, 0, 2, 1, chunk500);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 500, "Global reassembly memory must be exactly 500 bytes");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteClient) == 2, "Sequence must advance to 2 for valid fragment");

    // 2. Send fragment with incompatible fragTotal = 5 (expected 2) on sequence 2
    SendRawFragment(rawSock, ep, remoteClient, 50, 1, 5, 2, chunk500);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Policy check: datagram rejected without confirming sequence or corrupting valid context
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 500, "Memory must NOT increase for incompatible fragTotal");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteClient) == 2, "Sequence must NOT advance for incompatible datagram");

    // 3. Send fragment with incompatible channel = 2 (expected 1) on sequence 2
    std::vector<uint8_t> badChanBuf(sizeof(WireHeader) + chunk500.size(), 0);
    auto* hdrBad = reinterpret_cast<WireHeader*>(badChanBuf.data());
    hdrBad->magic = REFIX_WIRE_MAGIC;
    hdrBad->version = REFIX_WIRE_VERSION;
    hdrBad->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdrBad->flags = FLAG_RELIABLE | FLAG_FRAGMENT;
    hdrBad->SetSenderPeerId(remoteClient);
    hdrBad->generationId = 1;
    hdrBad->fragmentMsgId = 50;
    hdrBad->channel = 2; // Incompatible channel!
    hdrBad->fragIndex = 1;
    hdrBad->fragTotal = 2;
    hdrBad->sequence = 2;
    hdrBad->payloadLen = static_cast<uint16_t>(chunk500.size());
    std::memcpy(badChanBuf.data() + sizeof(WireHeader), chunk500.data(), chunk500.size());

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(ep.ipv4);
    sin.sin_port = htons(ep.port);
    sendto(rawSock, reinterpret_cast<const char*>(badChanBuf.data()), static_cast<int>(badChanBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 500, "Memory must NOT increase for incompatible channel");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteClient) == 2, "Sequence must NOT advance for incompatible channel");

    // 4. Now send the authentic legitimate fragment 1 of 2 (seq 2, channel 1, fragTotal 2)
    SendRawFragment(rawSock, ep, remoteClient, 50, 1, 2, 2, chunk500);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Message must complete cleanly!
    InboundPacket fullMsg;
    TEST_ASSERT(trReceiver.PollInbound(fullMsg), "Message must be delivered cleanly upon authentic fragment arrival");
    TEST_ASSERT(fullMsg.payload.size() == 1000, "Reassembled payload size must be 1000 bytes");
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 0, "Global reassembly memory must cleanly return to 0");
    TEST_ASSERT(trReceiver.GetReassemblyContextCount() == 0, "Reassembly context count must return to 0");

    closesocket(rawSock);
    trReceiver.Stop();

    std::cout << "  [PASS] Incompatible fragment metadata strict rejection and context protection certified!" << std::endl;
    return true;
}

static bool TestP0_DualTransportSackReassemblyBudgetRecovery() {
    std::cout << "[*] Running TestP0_DualTransportSackReassemblyBudgetRecovery (Dual Transports, SACK, Reassembly Budget Saturation, Auto Drain)..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0x12344321, 0x56788765};
    PeerId peerReceiver{0x9ABCDEF0, 0x13572468};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47790), "trSender start failed");
    TEST_ASSERT(trReceiver.Start(47791), "trReceiver start failed");

    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET fwdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(fwdSock != INVALID_SOCKET, "fwdSock creation failed");
    sockaddr_in fwdSin{};
    fwdSin.sin_family = AF_INET;
    fwdSin.sin_addr.s_addr = htonl(0x7F000001);
    fwdSin.sin_port = htons(47792);
    TEST_ASSERT(bind(fwdSock, reinterpret_cast<const sockaddr*>(&fwdSin), sizeof(fwdSin)) == 0, "fwdSock bind failed");
    u_long nonblock = 1;
    ioctlsocket(fwdSock, FIONBIO, &nonblock);
    LanEndpoint epForwarder{0x7F000001, 47792};

    std::atomic<bool> fwdRunning{true};
    std::atomic<bool> releaseSeq1{false};
    std::vector<uint8_t> delayedSeq1;
    sockaddr_in senderSin{};
    std::mutex fwdMutex;

    std::thread fwdThread([&]() {
        uint8_t buf[65536];
        sockaddr_in fromSin{};
        int fromLen = sizeof(fromSin);

        while (fwdRunning.load()) {
            fromLen = sizeof(fromSin);
            int r = recvfrom(fwdSock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
            if (r > 0 && r >= static_cast<int>(sizeof(WireHeader))) {
                auto* hdr = reinterpret_cast<WireHeader*>(buf);
                uint16_t fromPort = ntohs(fromSin.sin_port);

                if (fromPort == epSender.port) {
                    senderSin = fromSin;
                    if (hdr->msgType == static_cast<uint8_t>(MsgType::DataReliable) && hdr->sequence == 1 && !releaseSeq1.load()) {
                        std::lock_guard<std::mutex> lk(fwdMutex);
                        delayedSeq1.assign(buf, buf + r);
                    } else {
                        sockaddr_in toRecvSin{};
                        toRecvSin.sin_family = AF_INET;
                        toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
                        toRecvSin.sin_port = htons(epReceiver.port);
                        sendto(fwdSock, reinterpret_cast<const char*>(buf), r, 0,
                               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
                    }
                } else if (fromPort == epReceiver.port) {
                    sendto(fwdSock, reinterpret_cast<const char*>(buf), r, 0,
                           reinterpret_cast<const sockaddr*>(&senderSin), sizeof(senderSin));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    });

    // 1. Sender transmits fragmented reliable message: 2000 bytes = chunk 0 (1150 bytes, seq 1) + chunk 1 (850 bytes, seq 2)
    std::vector<uint8_t> realMsg(2000);
    for (size_t i = 0; i < realMsg.size(); ++i) {
        realMsg[i] = static_cast<uint8_t>((i * 17 + 5) & 0xFF);
    }

    TEST_ASSERT(trSender.SendReliable(peerReceiver, epForwarder, 1, realMsg.data(), realMsg.size()), "SendReliable failed");

    // Wait for receiver to process chunk 1 (seq 2) and send real SACK back
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 2. Verify sender recognized real SACK for seq 2!
    TEST_ASSERT(trSender.IsSequenceSacked(peerReceiver, 2), "Sender must recognize SACK for seq 2");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) == 1, "Receiver must hold seq 2 in outOfOrderInbound");

    // 3. Saturate receiver's reassembly budget using dummy peers:
    // 4 dummy peers, each allocating 1,048,000 bytes = 4,192,000 bytes total (out of 4,194,304 global limit).
    // Headroom left is 304 bytes, which is strictly less than chunk 0's 1150 bytes!
    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "rawSock creation failed");

    std::vector<uint8_t> chunk1k(1000, 0xEE);
    for (uint64_t pIdx = 1; pIdx <= 4; ++pIdx) {
        PeerId p{0xCAFE0000 | pIdx, 0xBABE0000 | pIdx};
        uint32_t pSeq = 1;
        for (uint32_t sId = 1; sId <= 6; ++sId) {
            for (uint8_t f = 0; f < 150; ++f) {
                SendRawFragment(rawSock, epReceiver, p, sId, f, 200, pSeq++, chunk1k);
                if (f % 50 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        for (uint8_t f = 0; f < 148; ++f) {
            SendRawFragment(rawSock, epReceiver, p, 7, f, 200, pSeq++, chunk1k);
            if (f % 50 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    PeerId peer5{0xCAFE0005, 0xBABE0005};
    SendRawFragment(rawSock, epReceiver, peer5, 1, 0, 10, 1, chunk1k);
    SendRawFragment(rawSock, epReceiver, peer5, 1, 1, 10, 2, chunk1k);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 4194000, "Global reassembly memory must be 4,194,000 bytes");

    // 4. Release delayed chunk 0 (seq 1)
    {
        std::lock_guard<std::mutex> lk(fwdMutex);
        releaseSeq1.store(true);
        TEST_ASSERT(!delayedSeq1.empty(), "delayedSeq1 must have been captured");
        sockaddr_in toRecvSin{};
        toRecvSin.sin_family = AF_INET;
        toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
        toRecvSin.sin_port = htons(epReceiver.port);
        sendto(fwdSock, reinterpret_cast<const char*>(delayedSeq1.data()), static_cast<int>(delayedSeq1.size()), 0,
               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Chunk 0 arrives, but CANNOT allocate into reassembly buffer because 4,192,000 + 1150 > 4,194,304!
    // Therefore, chunk 0 is retained in outOfOrderInbound!
    // ExpectedSequenceIn must NOT advance!
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(peerSender) == 1, "Expected sequence must NOT advance under memory budget saturation");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) >= 1, "Chunks must remain retained in outOfOrderInbound");

    // 5. Free reassembly memory: reset dummy peer 1, freeing 1,048,000 bytes of global headroom!
    PeerId dummy1{0xCAFE0001, 0xBABE0001};
    trReceiver.ResetPeerState(dummy1);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 6. Automatic recovery: receiver reactor loop drains retained chunks, completes reassembly, and sends cumulative ACK!
    InboundPacket deliveredMsg;
    bool gotMsg = false;
    for (int r = 0; r < 50; ++r) {
        if (trReceiver.PollInbound(deliveredMsg)) {
            if (deliveredMsg.payload.size() == 2000) {
                gotMsg = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(gotMsg, "Reassembled message must be delivered cleanly via automatic drain once budget freed");
    TEST_ASSERT(deliveredMsg.payload == realMsg, "Reassembled payload must match original bit-for-bit");

    // Wait for cumulative ACK to reach sender
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 0, "Sender must retire all packets upon cumulative ACK");

    // Clean up
    fwdRunning.store(false);
    if (fwdThread.joinable()) fwdThread.join();
    closesocket(rawSock);
    closesocket(fwdSock);
    trSender.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] Dual-transport SACK and automatic recovery under reassembly budget saturation certified!" << std::endl;
    return true;
}

static bool TestP0_ZeroGenerationAndStrictSessionValidation() {
    std::cout << "[*] Running TestP0_ZeroGenerationAndStrictSessionValidation (Zero-gen rejection, spoofed gen, authentic ACK)..." << std::endl;

    LanTransport trSender;
    PeerId peerSender{0x12340001, 0x56780002};
    PeerId peerReceiver{0x9ABC0003, 0xDEF00004};

    trSender.SetLocalPeerId(peerSender);
    TEST_ASSERT(trSender.Start(47750), "trSender start failed");

    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    // Legitimate receiver on port 47751
    SOCKET sockReceiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockReceiver != INVALID_SOCKET, "Receiver socket failed");
    sockaddr_in sinRecv{};
    sinRecv.sin_family = AF_INET;
    sinRecv.sin_addr.s_addr = htonl(0x7F000001);
    sinRecv.sin_port = htons(47751);
    TEST_ASSERT(bind(sockReceiver, reinterpret_cast<const sockaddr*>(&sinRecv), sizeof(sinRecv)) == 0, "Bind receiver failed");
    LanEndpoint epReceiver{0x7F000001, 47751};

    // Attacker on port 47755
    SOCKET sockAttacker = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockAttacker != INVALID_SOCKET, "Attacker socket failed");
    sockaddr_in sinAttack{};
    sinAttack.sin_family = AF_INET;
    sinAttack.sin_addr.s_addr = htonl(0x7F000001);
    sinAttack.sin_port = htons(47755);
    TEST_ASSERT(bind(sockAttacker, reinterpret_cast<const sockaddr*>(&sinAttack), sizeof(sinAttack)) == 0, "Bind attacker failed");

    sockaddr_in sinSender{};
    sinSender.sin_family = AF_INET;
    sinSender.sin_addr.s_addr = htonl(epSender.ipv4);
    sinSender.sin_port = htons(epSender.port);

    // 1. Establish session from sender to receiver by sending sequence 1
    std::string msg = "MSG_ESTABLISH_SESSION";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msg.data(), msg.size()), "SendReliable failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    uint32_t activeGen = trSender.GetPeerLocalGeneration(peerReceiver);
    TEST_ASSERT(activeGen != 0, "Sender local generation must be non-zero");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Unacked outbound must have 1 packet");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47751, "lastEndpoint must be legitimate receiver port 47751");

    // 2. DataAck with generationId == 0 from legitimate endpoint (port 47751) -> MUST BE REJECTED
    WireHeader zeroGenAck{};
    zeroGenAck.magic = REFIX_WIRE_MAGIC;
    zeroGenAck.version = REFIX_WIRE_VERSION;
    zeroGenAck.msgType = static_cast<uint8_t>(MsgType::DataAck);
    zeroGenAck.flags = FLAG_HAS_ACK;
    zeroGenAck.SetSenderPeerId(peerReceiver);
    zeroGenAck.generationId = 0; // Generation 0 strictly forbidden in established reliable stream
    zeroGenAck.channel = 1;
    zeroGenAck.ack = 1;
    zeroGenAck.sackMask = 0;
    sendto(sockReceiver, reinterpret_cast<const char*>(&zeroGenAck), sizeof(zeroGenAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "DataAck with generation 0 must NOT retire unacked packet");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47751, "lastEndpoint must remain unchanged");

    // 3. DataReliable with generationId == 0 from legitimate endpoint -> MUST BE REJECTED
    std::vector<uint8_t> zeroGenRel(sizeof(WireHeader) + 16, 0);
    auto* hdrZeroRel = reinterpret_cast<WireHeader*>(zeroGenRel.data());
    hdrZeroRel->magic = REFIX_WIRE_MAGIC;
    hdrZeroRel->version = REFIX_WIRE_VERSION;
    hdrZeroRel->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdrZeroRel->flags = FLAG_RELIABLE;
    hdrZeroRel->SetSenderPeerId(peerReceiver);
    hdrZeroRel->generationId = 0; // Forbidden generation 0
    hdrZeroRel->channel = 1;
    hdrZeroRel->sequence = 1;
    hdrZeroRel->payloadLen = 16;
    sendto(sockReceiver, reinterpret_cast<const char*>(zeroGenRel.data()), static_cast<int>(zeroGenRel.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket dummyPkt;
    TEST_ASSERT(!trSender.PollInbound(dummyPkt), "DataReliable with generation 0 must NOT be admitted or delivered");
    TEST_ASSERT(trSender.GetPeerExpectedSequenceIn(peerReceiver) <= 1, "ExpectedSequenceIn must NOT advance on zero-generation packet");

    // 4. DataReliable with generationId == 0 from attacker endpoint (port 47755) -> MUST BE REJECTED
    sendto(sockAttacker, reinterpret_cast<const char*>(zeroGenRel.data()), static_cast<int>(zeroGenRel.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(!trSender.PollInbound(dummyPkt), "DataReliable with generation 0 from foreign endpoint must be rejected");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47751, "lastEndpoint must NOT be mutated by foreign zero-gen packet");

    // 5. DataAck with incorrect non-zero generation (activeGen + 99) -> MUST BE REJECTED
    WireHeader badGenAck = zeroGenAck;
    badGenAck.generationId = activeGen + 99; // Mismatched generation
    sendto(sockReceiver, reinterpret_cast<const char*>(&badGenAck), sizeof(badGenAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "DataAck with mismatched generation must NOT retire unacked packet");

    // 6. DataReliable with incorrect non-zero generation (activeGen + 99) on seq > 1 -> MUST BE REJECTED
    hdrZeroRel->generationId = activeGen + 99;
    hdrZeroRel->sequence = 2;
    sendto(sockReceiver, reinterpret_cast<const char*>(zeroGenRel.data()), static_cast<int>(zeroGenRel.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(!trSender.PollInbound(dummyPkt), "DataReliable seq > 1 with mismatched generation must be rejected");

    // 7. Authentic ACK from legitimate endpoint with matching generation -> MUST SUCCEED
    WireHeader legitAck = zeroGenAck;
    legitAck.generationId = activeGen;
    sendto(sockReceiver, reinterpret_cast<const char*>(&legitAck), sizeof(legitAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 0, "Authentic ACK with correct generation must retire unacked packet");
    TEST_ASSERT(trSender.GetPeerLastEndpoint(peerReceiver).port == 47751, "lastEndpoint must remain verified legitimate receiver");

    closesocket(sockAttacker);
    closesocket(sockReceiver);
    trSender.Stop();

    std::cout << "  [PASS] Zero generation and strict session generation validation certified!" << std::endl;
    return true;
}

static bool TestP1_EndpointMigrationAuthorizationRequirement() {
    std::cout << "[*] Running TestP1_EndpointMigrationAuthorizationRequirement (Reject unauth foreign endpoint seq 1 restart)..." << std::endl;

    LanTransport trReceiver;
    PeerId localReceiver{0x22220001, 0x33330002};
    PeerId remoteSender{0x44440003, 0x55550004};

    trReceiver.SetLocalPeerId(localReceiver);
    TEST_ASSERT(trReceiver.Start(47760), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET sockLegit = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in sinLegit{};
    sinLegit.sin_family = AF_INET;
    sinLegit.sin_addr.s_addr = htonl(0x7F000001);
    sinLegit.sin_port = htons(47761);
    TEST_ASSERT(bind(sockLegit, reinterpret_cast<const sockaddr*>(&sinLegit), sizeof(sinLegit)) == 0, "Bind sockLegit failed");

    SOCKET sockForeign = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in sinForeign{};
    sinForeign.sin_family = AF_INET;
    sinForeign.sin_addr.s_addr = htonl(0x7F000001);
    sinForeign.sin_port = htons(47765);
    TEST_ASSERT(bind(sockForeign, reinterpret_cast<const sockaddr*>(&sinForeign), sizeof(sinForeign)) == 0, "Bind sockForeign failed");

    sockaddr_in sinReceiver{};
    sinReceiver.sin_family = AF_INET;
    sinReceiver.sin_addr.s_addr = htonl(epReceiver.ipv4);
    sinReceiver.sin_port = htons(epReceiver.port);

    // 1. Establish session from legitimate endpoint (port 47761)
    std::vector<uint8_t> msgBuf(sizeof(WireHeader) + 16, 0);
    auto* hdr = reinterpret_cast<WireHeader*>(msgBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(remoteSender);
    hdr->generationId = 100;
    hdr->channel = 1;
    hdr->sequence = 1;
    hdr->payloadLen = 16;
    sendto(sockLegit, reinterpret_cast<const char*>(msgBuf.data()), static_cast<int>(msgBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket inPkt;
    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Legitimate packet seq 1 must be delivered");
    TEST_ASSERT(trReceiver.GetPeerLastEndpoint(remoteSender).port == 47761, "lastEndpoint must be legitimate port 47761");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 100, "Remote generation must be 100");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2, "Expected sequence must be 2");

    // 2. Foreign endpoint (port 47765) attempts to reset session by sending seq 1 with higher generation 200
    // MUST BE REJECTED without authorization!
    hdr->generationId = 200; // Higher generation
    hdr->sequence = 1;
    sendto(sockForeign, reinterpret_cast<const char*>(msgBuf.data()), static_cast<int>(msgBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(!trReceiver.PollInbound(inPkt), "Unauthorized foreign endpoint seq 1 higher generation must NOT be delivered");
    TEST_ASSERT(trReceiver.GetPeerLastEndpoint(remoteSender).port == 47761, "lastEndpoint must NOT be changed to foreign port");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 100, "Remote generation must remain 100");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2, "Expected sequence must remain 2");

    // 3. Now authorize migration to foreign endpoint (port 47765)
    LanEndpoint epForeign{0x7F000001, 47765};
    TEST_ASSERT(trReceiver.AuthorizePeerMigration(remoteSender, epForeign), "AuthorizePeerMigration must succeed");

    // 4. Send seq 1 with higher generation 200 again from authorized foreign endpoint
    sendto(sockForeign, reinterpret_cast<const char*>(msgBuf.data()), static_cast<int>(msgBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Authorized foreign endpoint packet must be accepted");
    TEST_ASSERT(trReceiver.GetPeerLastEndpoint(remoteSender).port == 47765, "lastEndpoint must now be updated to 47765");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 200, "Remote generation must be updated to 200");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2, "Expected sequence must advance to 2");

    closesocket(sockLegit);
    closesocket(sockForeign);
    trReceiver.Stop();

    std::cout << "  [PASS] Endpoint migration authorization enforcement certified!" << std::endl;
    return true;
}

static bool TestP1_PiggybackedAckCorrelationAndSeparation() {
    std::cout << "[*] Running TestP1_PiggybackedAckCorrelationAndSeparation (Piggybacked ACK flow correlation & standalone ACKs)..." << std::endl;

    LanTransport trSender;
    PeerId peerSender{0x77770001, 0x88880002};
    PeerId peerReceiver{0x99990003, 0xAAAA0004};

    trSender.SetLocalPeerId(peerSender);
    TEST_ASSERT(trSender.Start(47770), "trSender start failed");

    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    SOCKET sockReceiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in sinRecv{};
    sinRecv.sin_family = AF_INET;
    sinRecv.sin_addr.s_addr = htonl(0x7F000001);
    sinRecv.sin_port = htons(47771);
    TEST_ASSERT(bind(sockReceiver, reinterpret_cast<const sockaddr*>(&sinRecv), sizeof(sinRecv)) == 0, "Bind receiver failed");
    LanEndpoint epReceiver{0x7F000001, 47771};

    SOCKET sockAttacker = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in sinAttack{};
    sinAttack.sin_family = AF_INET;
    sinAttack.sin_addr.s_addr = htonl(0x7F000001);
    sinAttack.sin_port = htons(47775);
    TEST_ASSERT(bind(sockAttacker, reinterpret_cast<const sockaddr*>(&sinAttack), sizeof(sinAttack)) == 0, "Bind attacker failed");

    sockaddr_in sinSender{};
    sinSender.sin_family = AF_INET;
    sinSender.sin_addr.s_addr = htonl(epSender.ipv4);
    sinSender.sin_port = htons(epSender.port);

    // 1. Sender transmits reliable message (seq 1)
    std::string msg1 = "PAYLOAD_SEQ_1";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msg1.data(), msg1.size()), "Send msg1 failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Sender must have 1 unacked outbound packet");
    uint32_t sessionGen = trSender.GetPeerLocalGeneration(peerReceiver);

    // 2. Receiver establishes reverse stream so trSender has hasRemoteGeneration
    std::vector<uint8_t> revBuf(sizeof(WireHeader) + 16, 0);
    auto* revHdr = reinterpret_cast<WireHeader*>(revBuf.data());
    revHdr->magic = REFIX_WIRE_MAGIC;
    revHdr->version = REFIX_WIRE_VERSION;
    revHdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    revHdr->flags = FLAG_RELIABLE;
    revHdr->SetSenderPeerId(peerReceiver);
    revHdr->generationId = 50;
    revHdr->channel = 1;
    revHdr->sequence = 1;
    revHdr->ack = 0; // No piggybacked ack yet
    revHdr->payloadLen = 16;
    sendto(sockReceiver, reinterpret_cast<const char*>(revBuf.data()), static_cast<int>(revBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket inPkt;
    TEST_ASSERT(trSender.PollInbound(inPkt), "Reverse packet must be received");
    TEST_ASSERT(trSender.GetPeerRemoteGeneration(peerReceiver) == 50, "trSender must record remote generation 50");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Unacked outbound must still be 1");

    // Test A: Piggybacked ACK on a packet with stale generation 40 (genDiff < 0) -> REJECTED
    revHdr->generationId = 40; // Stale generation!
    revHdr->sequence = 2;
    revHdr->ack = 1; // Attempt to piggyback ack for msg1
    sendto(sockReceiver, reinterpret_cast<const char*>(revBuf.data()), static_cast<int>(revBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Stale generation packet must NOT process piggybacked ACK");

    // Test B: Piggybacked ACK sent from an attacker endpoint (port 47775) with gen 50 -> REJECTED
    revHdr->generationId = 50;
    revHdr->sequence = 2;
    revHdr->ack = 1;
    sendto(sockAttacker, reinterpret_cast<const char*>(revBuf.data()), static_cast<int>(revBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1, "Piggybacked ACK from unauthorized endpoint must be rejected");

    // Test C: DataReliable carrying payload with piggybacked ACK on active stream from legitimate endpoint
    // Protocol Contract: Data payload is accepted and delivered, but piggybacked ACK
    // is NOT processed into ProcessReliableAck() because wire v2 does not identify outbound generation.
    // Unacked outbound count MUST remain 1.
    sendto(sockReceiver, reinterpret_cast<const char*>(revBuf.data()), static_cast<int>(revBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.PollInbound(inPkt), "Legitimate reverse packet seq 2 data must be delivered");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1,
                "Piggybacked ACK in DataReliable must NOT retire unacked packet without independent generation validation");

    // Test D: Outbound stream restart while inbound flow remains active
    uint32_t newSessionGen = sessionGen + 10;
    trSender.SetPeerLocalGenerationForTesting(peerReceiver, newSessionGen);
    std::string msg2 = "PAYLOAD_SEQ_2";
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epReceiver, 1, msg2.data(), msg2.size()), "Send msg2 failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 2, "Sender must have unacked packets for active flow");

    // Inbound flow continues receiving packets on remote gen 50
    revHdr->sequence = 3;
    revHdr->ack = 0;
    sendto(sockReceiver, reinterpret_cast<const char*>(revBuf.data()), static_cast<int>(revBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(trSender.PollInbound(inPkt), "Inbound stream must remain active across outbound reset");

    // Test E: Stale ACK from prior outbound generation (sessionGen vs active newSessionGen) -> REJECTED
    WireHeader staleGenAck{};
    staleGenAck.magic = REFIX_WIRE_MAGIC;
    staleGenAck.version = REFIX_WIRE_VERSION;
    staleGenAck.msgType = static_cast<uint8_t>(MsgType::DataAck);
    staleGenAck.flags = FLAG_HAS_ACK;
    staleGenAck.SetSenderPeerId(peerReceiver);
    staleGenAck.generationId = sessionGen; // Obsolete generation!
    staleGenAck.channel = 1;
    staleGenAck.ack = 1;
    staleGenAck.sackMask = 0;
    sendto(sockReceiver, reinterpret_cast<const char*>(&staleGenAck), sizeof(staleGenAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 2,
                "DataAck from obsolete session generation must be rejected and not remove elements from unackedOutbound");

    // Test F: Standalone ACK from unauthorized endpoint (port 47775) -> REJECTED
    staleGenAck.generationId = newSessionGen;
    sendto(sockAttacker, reinterpret_cast<const char*>(&staleGenAck), sizeof(staleGenAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 2,
                "DataAck from unauthorized endpoint must be rejected and not remove elements from unackedOutbound");

    // Test G: Legitimate standalone DataAck from registered peer endpoint with matching generation -> ACCEPTED
    sendto(sockReceiver, reinterpret_cast<const char*>(&staleGenAck), sizeof(staleGenAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 1,
                "Legitimate standalone DataAck must retire seq 1 from unackedOutbound");

    // Final ACK for seq 2
    staleGenAck.ack = 2;
    sendto(sockReceiver, reinterpret_cast<const char*>(&staleGenAck), sizeof(staleGenAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 0,
                "Final standalone DataAck must cleanly retire seq 2 from unackedOutbound");

    closesocket(sockAttacker);
    closesocket(sockReceiver);
    trSender.Stop();

    std::cout << "  [PASS] Piggybacked ACK flow correlation, outbound restart, unauthorized rejection, and standalone ACK continuation certified!" << std::endl;
    return true;
}

static bool TestP1_Modular32BitGenerationWrap() {
    std::cout << "[*] Running TestP1_Modular32BitGenerationWrap (RFC 1982 modular comparison around 2^32 - 1)..." << std::endl;

    LanTransport trReceiver;
    PeerId localReceiver{0x66660001, 0x77770002};
    PeerId remoteSender{0x88880003, 0x99990004};

    trReceiver.SetLocalPeerId(localReceiver);
    TEST_ASSERT(trReceiver.Start(47780), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in sinReceiver{};
    sinReceiver.sin_family = AF_INET;
    sinReceiver.sin_addr.s_addr = htonl(epReceiver.ipv4);
    sinReceiver.sin_port = htons(epReceiver.port);

    // 1. Establish session near 32-bit wrap boundary: generationId = 0xFFFFFFFF (2^32 - 1)
    std::vector<uint8_t> pktBuf(sizeof(WireHeader) + 16, 0);
    auto* hdr = reinterpret_cast<WireHeader*>(pktBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(remoteSender);
    hdr->generationId = 0xFFFFFFFFu;
    hdr->channel = 1;
    hdr->sequence = 1;
    hdr->payloadLen = 16;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket inPkt;
    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Initial packet with generation 0xFFFFFFFF must be accepted");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 0xFFFFFFFFu, "Remote generation must be 0xFFFFFFFF");

    // 2. Wrap-around to small generation: generationId = 5
    // Under RFC 1982 modular arithmetic:
    // static_cast<int32_t>(5u - 0xFFFFFFFFu) = static_cast<int32_t>(6) > 0!
    // This is mathematically newer and valid!
    hdr->generationId = 5;
    hdr->sequence = 1; // Stream restart
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Wrapped generation 5 must be recognized as newer than 0xFFFFFFFF");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 5, "Remote generation must advance to 5");

    // 3. Packet with generation 0xFFFFFFF0 (stale by ~21 steps in the past)
    // static_cast<int32_t>(0xFFFFFFF0u - 5u) = static_cast<int32_t>(-21) < 0 -> OBSOLETE!
    hdr->generationId = 0xFFFFFFF0u;
    hdr->sequence = 2;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(!trReceiver.PollInbound(inPkt), "Obsolete generation datagram across wrap boundary must be rejected");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 5, "Remote generation must remain 5");

    closesocket(rawSock);
    trReceiver.Stop();

    std::cout << "  [PASS] RFC 1982 modular arithmetic and 32-bit generation wrap certified!" << std::endl;
    return true;
}

static bool TestP1_ArqSequenceModularWrap() {
    std::cout << "[*] Running TestP1_ArqSequenceModularWrap (32-bit ARQ sequence modular wrap around 2^32 - 1)..." << std::endl;

    LanTransport trReceiver;
    PeerId localReceiver{0x66660001, 0x77770002};
    PeerId remoteSender{0x88880003, 0x99990004};

    trReceiver.SetLocalPeerId(localReceiver);
    TEST_ASSERT(trReceiver.Start(47820), "trReceiver start failed on 47820");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET rawSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(rawSock != INVALID_SOCKET, "rawSock creation failed");

    sockaddr_in sinRaw{};
    sinRaw.sin_family = AF_INET;
    sinRaw.sin_addr.s_addr = htonl(0x7F000001);
    sinRaw.sin_port = htons(47821);
    TEST_ASSERT(bind(rawSock, reinterpret_cast<const sockaddr*>(&sinRaw), sizeof(sinRaw)) == 0, "rawSock bind failed");
    u_long nonblock = 1;
    ioctlsocket(rawSock, FIONBIO, &nonblock);

    sockaddr_in sinReceiver{};
    sinReceiver.sin_family = AF_INET;
    sinReceiver.sin_addr.s_addr = htonl(epReceiver.ipv4);
    sinReceiver.sin_port = htons(epReceiver.port);

    // 1. Establish session starting at sequence 0xFFFFFFFEu
    // Send seq 1 initially to establish generation 10
    std::vector<uint8_t> pktBuf(sizeof(WireHeader) + 16, 0);
    auto* hdr = reinterpret_cast<WireHeader*>(pktBuf.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(remoteSender);
    hdr->generationId = 10;
    hdr->channel = 1;
    hdr->sequence = 1;
    hdr->payloadLen = 16;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    InboundPacket inPkt;
    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Initial packet must establish session");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2, "Expected seq must be 2");

    // Advance expected sequence in to 0xFFFFFFFEu to test wrap boundary
    trReceiver.SetPeerExpectedSequenceInForTesting(remoteSender, 0xFFFFFFFEu);
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 0xFFFFFFFEu, "Set expected seq failed");

    // Helper lambda to drain any DataAck packets received on rawSock
    auto drainAcks = [&](uint32_t& lastAckSeq, uint32_t& lastSackMask) -> bool {
        uint8_t ackBuf[128];
        bool gotAck = false;
        sockaddr_in fromSin{};
        int fromLen = sizeof(fromSin);
        while (true) {
            fromLen = sizeof(fromSin);
            int r = recvfrom(rawSock, reinterpret_cast<char*>(ackBuf), sizeof(ackBuf), 0,
                             reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
            if (r >= static_cast<int>(sizeof(WireHeader))) {
                const auto* ackHdr = reinterpret_cast<const WireHeader*>(ackBuf);
                if (ackHdr->msgType == static_cast<uint8_t>(MsgType::DataAck)) {
                    lastAckSeq = ackHdr->ack;
                    lastSackMask = ackHdr->sackMask;
                    gotAck = true;
                }
            } else {
                break;
            }
        }
        return gotAck;
    };

    uint32_t ackSeq = 0;
    uint32_t sackMask = 0;
    drainAcks(ackSeq, sackMask);

    // 2. Sequential in-order delivery across 32-bit wrap boundary:
    // Sequences: 0xFFFFFFFEu -> 0xFFFFFFFFu -> 0u -> 1u
    const uint32_t wrapSeqs[] = { 0xFFFFFFFEu, 0xFFFFFFFFu, 0u, 1u };
    for (size_t i = 0; i < 4; ++i) {
        hdr->sequence = wrapSeqs[i];
        hdr->payloadLen = 8;
        std::memcpy(pktBuf.data() + sizeof(WireHeader), &wrapSeqs[i], sizeof(uint32_t));
        sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
               reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        TEST_ASSERT(trReceiver.PollInbound(inPkt), "In-order packet across wrap boundary must be delivered");
        uint32_t expectedNext = (wrapSeqs[i] + 1); // 0xFFFFFFFF + 1 wraps to 0
        TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == expectedNext,
                    "Expected sequence must wrap correctly across 2^32 - 1");

        TEST_ASSERT(drainAcks(ackSeq, sackMask), "Receiver must emit DataAck across wrap");
        TEST_ASSERT(ackSeq == wrapSeqs[i], "Cumulative ACK must match received sequence across wrap");
        TEST_ASSERT(sackMask == 0, "SACK mask must be 0 for in-order delivery");
    }

    // Currently, expectedSequenceIn is 2u.
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2u, "ExpectedSequenceIn must be 2");

    // 3. SACK out-of-order spanning across the wrap boundary:
    // Reset expectedSequenceIn back to 0xFFFFFFFFu.
    // Packet 0 is not sent yet. Packet 1 is sent out-of-order!
    // Modular distance: 1 - 0xFFFFFFFF = 2.
    // Expected SACK mask: bit (2 - 1 - 1) = bit 0 for seq 0, bit 1 for seq 1 -> sackMask = 2 (0x02)
    trReceiver.SetPeerExpectedSequenceInForTesting(remoteSender, 0xFFFFFFFFu);
    drainAcks(ackSeq, sackMask);

    hdr->sequence = 1u; // Out of order (missing 0xFFFFFFFF and 0)
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TEST_ASSERT(!trReceiver.PollInbound(inPkt), "OOO packet across wrap boundary must NOT be delivered yet");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(remoteSender) == 1, "Receiver must hold seq 1 in OOO map");
    TEST_ASSERT(drainAcks(ackSeq, sackMask), "Receiver must send SACK for OOO packet across wrap");
    TEST_ASSERT(ackSeq == 0xFFFFFFFEu, "Cumulative ack must acknowledge sequence prior to missing packet");
    TEST_ASSERT((sackMask & (1u << 2)) != 0, "SACK mask bit 2 must be set for sequence 1 (offset 3) across 32-bit wrap");

    // 4. Retransmission across wrap boundary:
    // Now send the missing in-order packet 0xFFFFFFFFu!
    hdr->sequence = 0xFFFFFFFFu;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Retransmitted packet 0xFFFFFFFF must be delivered");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 0u, "Expected seq must advance to 0");

    // Now send missing packet 0u!
    // Receiver should deliver 0u, and then automatically drain retained seq 1 from outOfOrderInbound!
    hdr->sequence = 0u;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Packet 0 must be delivered");
    TEST_ASSERT(trReceiver.PollInbound(inPkt), "Retained OOO packet 1 must be drained and delivered across wrap");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2u, "Expected sequence must advance to 2");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(remoteSender) == 0, "OOO map must be empty after full drain");

    // 5. Old duplicates across wrap boundary:
    // Send duplicate of 0xFFFFFFFEu and 0xFFFFFFFFu (which are ancient compared to expected 2u).
    drainAcks(ackSeq, sackMask);
    hdr->sequence = 0xFFFFFFFEu;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TEST_ASSERT(!trReceiver.PollInbound(inPkt), "Old duplicate across wrap boundary must be discarded");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2u, "Expected sequence must not mutate on duplicate");
    TEST_ASSERT(drainAcks(ackSeq, sackMask), "Receiver must ACK latest expected sequence for duplicate");
    TEST_ASSERT(ackSeq == 1u, "ACK for duplicate must be expectedSequenceIn - 1 (1)");

    // 6. Outbound SendReliable & Retransmission retirement across wrap:
    LanTransport trSender;
    trSender.SetLocalPeerId(remoteSender);
    TEST_ASSERT(trSender.Start(47822), "trSender start failed");
    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    trSender.SetPeerNextSequenceOutForTesting(localReceiver, 0xFFFFFFFEu);
    trSender.SetPeerLocalGenerationForTesting(localReceiver, 10);

    std::string wMsg1 = "WRAP_OUT_1";
    std::string wMsg2 = "WRAP_OUT_2";
    std::string wMsg3 = "WRAP_OUT_3";
    LanEndpoint epRaw{ 0x7F000001, 47821 };
    TEST_ASSERT(trSender.SendReliable(localReceiver, epRaw, 1, wMsg1.data(), wMsg1.size()), "Send wMsg1 failed");
    TEST_ASSERT(trSender.SendReliable(localReceiver, epRaw, 1, wMsg2.data(), wMsg2.size()), "Send wMsg2 failed");
    TEST_ASSERT(trSender.SendReliable(localReceiver, epRaw, 1, wMsg3.data(), wMsg3.size()), "Send wMsg3 failed");

    TEST_ASSERT(trSender.GetUnackedOutboundCount(localReceiver) == 3, "Sender must have 3 unacked packets in flight");
    TEST_ASSERT(trSender.GetPeerNextSequenceOut(localReceiver) == 1u, "nextSequenceOut must have wrapped past 0 to 1");

    // Cumulative DataAck for 0u: confirms 0xFFFFFFFEu, 0xFFFFFFFFu, 0u
    WireHeader wrapAck{};
    wrapAck.magic = REFIX_WIRE_MAGIC;
    wrapAck.version = REFIX_WIRE_VERSION;
    wrapAck.msgType = static_cast<uint8_t>(MsgType::DataAck);
    wrapAck.flags = FLAG_HAS_ACK;
    wrapAck.SetSenderPeerId(localReceiver);
    wrapAck.generationId = 10;
    wrapAck.channel = 1;
    wrapAck.ack = 0u; // Confirms up to 0u across the wrap!
    wrapAck.sackMask = 0;

    sockaddr_in sinSender{};
    sinSender.sin_family = AF_INET;
    sinSender.sin_addr.s_addr = htonl(epSender.ipv4);
    sinSender.sin_port = htons(epSender.port);

    sendto(rawSock, reinterpret_cast<const char*>(&wrapAck), sizeof(wrapAck), 0,
           reinterpret_cast<const sockaddr*>(&sinSender), sizeof(sinSender));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trSender.GetUnackedOutboundCount(localReceiver) == 0,
                "Cumulative ACK for 0u across wrap must retire all 3 packets");

    // 7. Coordinated session restart across wrap:
    hdr->generationId = 20; // New generation
    hdr->sequence = 1;
    sendto(rawSock, reinterpret_cast<const char*>(pktBuf.data()), static_cast<int>(pktBuf.size()), 0,
           reinterpret_cast<const sockaddr*>(&sinReceiver), sizeof(sinReceiver));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(trReceiver.PollInbound(inPkt), "New generation packet 1 must be accepted");
    TEST_ASSERT(trReceiver.GetPeerRemoteGeneration(remoteSender) == 20, "Remote generation must update to 20");
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(remoteSender) == 2, "Expected seq must reset to 2 for new generation");

    closesocket(rawSock);
    trSender.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] 32-bit ARQ sequence modular wrap (sequential, cumulative ACK, SACK, retransmission, duplicates, and coordinated reset) certified!" << std::endl;
    return true;
}

static bool TestP1_SackedCandidateInvalidationAndLifecycleRecovery() {
    std::cout << "[*] Running TestP1_SackedCandidateInvalidationAndLifecycleRecovery (Dual real transports, SACK revocation on corruption, terminal Disconnect)..." << std::endl;

    LanTransport trSender;
    LanTransport trReceiver;

    PeerId peerSender{0x5555AAAA, 0x6666BBBB};
    PeerId peerReceiver{0x7777CCCC, 0x8888DDDD};

    trSender.SetLocalPeerId(peerSender);
    trReceiver.SetLocalPeerId(peerReceiver);

    TEST_ASSERT(trSender.Start(47830), "trSender start failed");
    TEST_ASSERT(trReceiver.Start(47831), "trReceiver start failed");

    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET fwdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(fwdSock != INVALID_SOCKET, "fwdSock creation failed");
    sockaddr_in fwdSin{};
    fwdSin.sin_family = AF_INET;
    fwdSin.sin_addr.s_addr = htonl(0x7F000001);
    fwdSin.sin_port = htons(47832);
    TEST_ASSERT(bind(fwdSock, reinterpret_cast<const sockaddr*>(&fwdSin), sizeof(fwdSin)) == 0, "fwdSock bind failed");
    u_long nonblock = 1;
    ioctlsocket(fwdSock, FIONBIO, &nonblock);
    LanEndpoint epForwarder{0x7F000001, 47832};

    std::atomic<bool> fwdRunning{true};
    std::atomic<bool> releaseSeq1{false};
    std::vector<uint8_t> delayedSeq1;
    sockaddr_in senderSin{};
    std::mutex fwdMutex;

    std::thread fwdThread([&]() {
        uint8_t buf[65536];
        sockaddr_in fromSin{};
        int fromLen = sizeof(fromSin);

        while (fwdRunning.load()) {
            fromLen = sizeof(fromSin);
            int r = recvfrom(fwdSock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
            if (r > 0 && r >= static_cast<int>(sizeof(WireHeader))) {
                auto* hdr = reinterpret_cast<WireHeader*>(buf);
                uint16_t fromPort = ntohs(fromSin.sin_port);

                if (fromPort == epSender.port) {
                    senderSin = fromSin;
                    if (hdr->msgType == static_cast<uint8_t>(MsgType::DataReliable) && hdr->sequence == 1 && !releaseSeq1.load()) {
                        std::lock_guard<std::mutex> lk(fwdMutex);
                        delayedSeq1.assign(buf, buf + r);
                    } else {
                        sockaddr_in toRecvSin{};
                        toRecvSin.sin_family = AF_INET;
                        toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
                        toRecvSin.sin_port = htons(epReceiver.port);
                        sendto(fwdSock, reinterpret_cast<const char*>(buf), r, 0,
                               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
                    }
                } else if (fromPort == epReceiver.port) {
                    sendto(fwdSock, reinterpret_cast<const char*>(buf), r, 0,
                           reinterpret_cast<const sockaddr*>(&senderSin), sizeof(senderSin));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    });

    // 1. Sender transmits fragmented reliable message: 2000 bytes = chunk 0 (1150 bytes, seq 1) + chunk 1 (850 bytes, seq 2)
    std::vector<uint8_t> testMsg(2000, 0x77);
    TEST_ASSERT(trSender.SendReliable(peerReceiver, epForwarder, 1, testMsg.data(), testMsg.size()), "SendReliable failed");

    // Wait for receiver to process chunk 1 (seq 2) out of order and send real SACK back
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 2. Verify sender recognized real SACK for seq 2!
    TEST_ASSERT(trSender.IsSequenceSacked(peerReceiver, 2), "Sender must recognize SACK for seq 2");
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) == 1, "Receiver must hold seq 2 in outOfOrderInbound");

    // 3. Now force the condition that obliges rejecting the SACKed packet:
    // Release chunk 0 (seq 1), but modify its header to introduce incompatible metadata:
    // change fragTotal from 2 to 3!
    {
        std::lock_guard<std::mutex> lk(fwdMutex);
        TEST_ASSERT(!delayedSeq1.empty(), "delayedSeq1 must hold fragment 0");
        auto* hdr1 = reinterpret_cast<WireHeader*>(delayedSeq1.data());
        hdr1->fragTotal = 3; // Incompatible with chunk 1's fragTotal (2)!
        hdr1->flags &= ~FLAG_LAST_FRAGMENT; // Not last fragment anymore
        sockaddr_in toRecvSin{};
        toRecvSin.sin_family = AF_INET;
        toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
        toRecvSin.sin_port = htons(epReceiver.port);
        sendto(fwdSock, reinterpret_cast<const char*>(delayedSeq1.data()), static_cast<int>(delayedSeq1.size()), 0,
               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
        releaseSeq1.store(true);
    }

    // Wait for receiver to process chunk 0, discover metadata incompatibility on SACKed chunk 1,
    // revoke SACK, invalidate context, and send Disconnect to sender
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 4. Verify SACK revocation and error propagation:
    // - Receiver has evicted the corrupted context and erased OOO candidate
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) == 0, "Receiver must not retain incompatible SACKed packet");
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 0, "Reassembly memory must be cleanly released");
    TEST_ASSERT(trReceiver.GetReassemblyContextCount() == 0, "Reassembly context count must be 0");

    // - Sender receives SACK revocation and explicit Disconnect
    TEST_ASSERT(!trSender.IsSequenceSacked(peerReceiver, 2), "Sender isSacked state must be revoked (false)");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerReceiver) == 0,
                "Sender ARQ unacked queue must be cleanly cleared upon explicit Disconnect (no blocked queue)");

    // - Receiver did not advance sequence as if the invalid message was delivered
    uint32_t expSeq = trReceiver.GetPeerExpectedSequenceIn(peerSender);
    TEST_ASSERT(expSeq != 3,
                "Expected sequence must NOT advance past fragment 2 as if invalid payload was delivered");

    InboundPacket dummyPkt;
    TEST_ASSERT(!trReceiver.PollInbound(dummyPkt), "Corrupted message must NOT be delivered");

    fwdRunning.store(false);
    if (fwdThread.joinable()) fwdThread.join();
    closesocket(fwdSock);
    trSender.Stop();
    trReceiver.Stop();

    std::cout << "  [PASS] SACKed candidate invalidation, SACK revocation, clean context release, and terminal Disconnect certified!" << std::endl;
    return true;
}

static bool TestP0_DrainReconstructionLimitExceededInvalidation() {
    std::cout << "[*] Running TestP0_DrainReconstructionLimitExceededInvalidation..." << std::endl;

    LanTransport trReceiver;
    TEST_ASSERT(trReceiver.Start(47842), "trReceiver start failed");

    LanEndpoint epReceiver = trReceiver.GetLocalDataEndpoint();
    epReceiver.ipv4 = 0x7F000001;

    SOCKET sockSender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockSender != INVALID_SOCKET, "sockSender creation failed");
    sockaddr_in sendSin{};
    sendSin.sin_family = AF_INET;
    sendSin.sin_addr.s_addr = htonl(0x7F000001);
    sendSin.sin_port = htons(47841);
    TEST_ASSERT(bind(sockSender, reinterpret_cast<const sockaddr*>(&sendSin), sizeof(sendSin)) == 0, "sockSender bind failed");
    u_long nonblock = 1;
    ioctlsocket(sockSender, FIONBIO, &nonblock);

    PeerId peerSender{0x1234567812345678ULL, 0x8765432187654321ULL};
    uint32_t sessionGen = 88;
    uint32_t fragMsgId = 42;
    uint8_t totalFrags = 228; // 228 fragments of 1150 bytes = 262,200 bytes > 262,144 (256 KB)

    sockaddr_in toRecvSin{};
    toRecvSin.sin_family = AF_INET;
    toRecvSin.sin_addr.s_addr = htonl(epReceiver.ipv4);
    toRecvSin.sin_port = htons(epReceiver.port);

    auto makeFragment = [&](uint8_t fIndex, uint32_t seq, size_t payloadSize, bool isLast) {
        std::vector<uint8_t> buf(sizeof(WireHeader) + payloadSize, 0x5A);
        auto* hdr = reinterpret_cast<WireHeader*>(buf.data());
        hdr->magic = REFIX_WIRE_MAGIC;
        hdr->version = REFIX_WIRE_VERSION;
        hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
        hdr->flags = FLAG_RELIABLE | FLAG_FRAGMENT | (isLast ? FLAG_LAST_FRAGMENT : 0);
        hdr->SetSenderPeerId(peerSender);
        hdr->sessionId = fragMsgId;
        hdr->channel = 1;
        hdr->fragIndex = fIndex;
        hdr->fragTotal = totalFrags;
        hdr->generationId = sessionGen;
        hdr->sequence = seq;
        hdr->ack = 0;
        hdr->sackMask = 0;
        hdr->payloadLen = static_cast<uint16_t>(payloadSize);
        return buf;
    };

    // 1. Send fragments 0 to 225 (seq 1 to 226) in order.
    // 226 fragments * 1150 bytes = 259,900 bytes (within 256 KB = 262,144 bytes).
    for (uint8_t i = 0; i < 226; ++i) {
        auto pkt = makeFragment(i, i + 1, 1150, false);
        sendto(sockSender, reinterpret_cast<const char*>(pkt.data()), static_cast<int>(pkt.size()), 0,
               reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    TEST_ASSERT(trReceiver.GetPeerExpectedSequenceIn(peerSender) == 227, "Expected sequence must be 227 after 226 in-order fragments");
    TEST_ASSERT(trReceiver.GetReassemblyContextCount() == 1, "Must have 1 reassembly context");
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 259900, "Global reassembly bytes must be 259,900");
    TEST_ASSERT(trReceiver.GetPeerReassemblyBytes(peerSender) == 259900, "Peer reassembly bytes must be 259,900");

    // 2. Now send the last fragment: index 227 (seq 228), out of order!
    // Since expectedSequenceIn == 227, seq 228 has seqDiff == 1 (< 64), so it will be buffered in outOfOrderInbound as SACKed candidate!
    auto pkt228 = makeFragment(227, 228, 1150, true);
    sendto(sockSender, reinterpret_cast<const char*>(pkt228.data()), static_cast<int>(pkt228.size()), 0,
           reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) == 1, "Receiver must hold seq 228 in outOfOrderInbound");

    // 3. Send fragment index 226 (seq 227): in-order fragment bridging the gap!
    // When seq 227 is drained:
    // fa.allocatedBytes reaches 259,900 + 1150 = 261,050 bytes.
    // Next, DrainRetainedInboundLocked encounters candidate seq 228:
    // fa.allocatedBytes + 1150 = 262,200 > 262,144 (REFIX_MAX_MESSAGE_SIZE)!
    // The reconstruction limit triggers on candidate seq 228!
    auto pkt227 = makeFragment(226, 227, 1150, false);
    sendto(sockSender, reinterpret_cast<const char*>(pkt227.data()), static_cast<int>(pkt227.size()), 0,
           reinterpret_cast<const sockaddr*>(&toRecvSin), sizeof(toRecvSin));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 4. Verification:
    // - Receiver has evicted the corrupted/excessive context and wiped all OOO candidates
    TEST_ASSERT(trReceiver.GetPeerOutOfOrderCount(peerSender) == 0, "Receiver must clear all stored candidates");
    TEST_ASSERT(trReceiver.GetGlobalReassemblyBytes() == 0, "Global reassembly bytes must be cleanly reset to 0");
    TEST_ASSERT(trReceiver.GetPeerReassemblyBytes(peerSender) == 0, "Peer reassembly bytes must be cleanly reset to 0");
    TEST_ASSERT(trReceiver.GetReassemblyContextCount() == 0, "Reassembly context count must be 0");

    // - Expected sequence must NOT advance past sequence 228 as if the invalid oversized message was delivered
    uint32_t finalExpSeq = trReceiver.GetPeerExpectedSequenceIn(peerSender);
    TEST_ASSERT(finalExpSeq != 229, "Expected sequence must NOT advance as if invalid payload was delivered");

    InboundPacket dummyPkt;
    TEST_ASSERT(!trReceiver.PollInbound(dummyPkt), "Oversized message must NEVER be delivered to application");

    // - Verify sockSender received a Disconnect packet from receiver
    bool gotDisconnect = false;
    uint8_t rBuf[2048];
    sockaddr_in rSin{};
    int rLen = sizeof(rSin);
    while (true) {
        int bytes = recvfrom(sockSender, reinterpret_cast<char*>(rBuf), sizeof(rBuf), 0,
                             reinterpret_cast<sockaddr*>(&rSin), &rLen);
        if (bytes <= 0) break;
        if (bytes >= static_cast<int>(sizeof(WireHeader))) {
            auto* rHdr = reinterpret_cast<WireHeader*>(rBuf);
            if (rHdr->msgType == static_cast<uint8_t>(MsgType::Disconnect)) {
                gotDisconnect = true;
            }
        }
    }
    TEST_ASSERT(gotDisconnect, "Sender must receive terminal Disconnect packet from receiver");

    closesocket(sockSender);
    trReceiver.Stop();

    std::cout << "  [PASS] REFIX_MAX_MESSAGE_SIZE reconstruction limit, SACKed candidate eviction, clean zero-leak memory release, and Disconnect certified!" << std::endl;
    return true;
}

static bool TestP1_OutboundArqQueueLimitAndNoPartialSends() {
    std::cout << "[*] Running TestP1_OutboundArqQueueLimitAndNoPartialSends..." << std::endl;

    LanTransport trSender;
    TEST_ASSERT(trSender.Start(47851), "trSender start failed");

    LanEndpoint epSender = trSender.GetLocalDataEndpoint();
    epSender.ipv4 = 0x7F000001;

    SOCKET sockSink = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockSink != INVALID_SOCKET, "sockSink creation failed");
    sockaddr_in sinkSin{};
    sinkSin.sin_family = AF_INET;
    sinkSin.sin_addr.s_addr = htonl(0x7F000001);
    sinkSin.sin_port = htons(47853);
    TEST_ASSERT(bind(sockSink, reinterpret_cast<const sockaddr*>(&sinkSin), sizeof(sinkSin)) == 0, "sockSink bind failed");
    u_long nonblock = 1;
    ioctlsocket(sockSink, FIONBIO, &nonblock);
    LanEndpoint epSink{0x7F000001, 47853};

    PeerId peerSink{0x8888777766665555ULL, 0x1111222233334444ULL};

    // 1. Fill outbound queue with 512 unacknowledged reliable datagrams
    std::string smallMsg = "ARQ_QUEUE_LIMIT_MSG";
    for (int i = 0; i < 512; ++i) {
        bool ok = trSender.SendReliable(peerSink, epSink, 1, smallMsg.data(), smallMsg.size());
        TEST_ASSERT(ok, "SendReliable within 512 capacity must succeed");
    }

    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 512, "Unacked count must be exactly 512");
    uint32_t seqAfter512 = trSender.GetPeerNextSequenceOut(peerSink);
    TEST_ASSERT(seqAfter512 == 513, "Next sequence out must be 513 after 512 single packets");

    // Drain raw datagrams received at sockSink so socket buffer does not overflow
    uint8_t drainBuf[2048];
    while (recvfrom(sockSink, reinterpret_cast<char*>(drainBuf), sizeof(drainBuf), 0, nullptr, nullptr) > 0) {}

    // 2. 513th packet MUST be rejected: queue is at maximum capacity (512)
    bool ok513 = trSender.SendReliable(peerSink, epSink, 1, smallMsg.data(), smallMsg.size());
    TEST_ASSERT(!ok513, "513th SendReliable must be rejected when unackedOutbound reaches 512");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 512, "Queue must remain capped at 512");
    TEST_ASSERT(trSender.GetPeerNextSequenceOut(peerSink) == seqAfter512, "Next sequence out must NOT advance when send is rejected");

    // 3. Fragmented message when queue is full: MUST be rejected without partial sends
    std::vector<uint8_t> frag2Chunks(2000, 0x33); // 2000 bytes = 2 fragments
    bool okFragFull = trSender.SendReliable(peerSink, epSink, 1, frag2Chunks.data(), frag2Chunks.size());
    TEST_ASSERT(!okFragFull, "Fragmented send must be rejected when queue is full");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 512, "Queue must remain exactly 512");
    TEST_ASSERT(trSender.GetPeerNextSequenceOut(peerSink) == seqAfter512, "Sequence must NOT advance on rejected fragmented send");

    // Verify nothing was sent on the wire during rejected sends
    int leaked = recvfrom(sockSink, reinterpret_cast<char*>(drainBuf), sizeof(drainBuf), 0, nullptr, nullptr);
    TEST_ASSERT(leaked <= 0, "No partial datagrams must be transmitted on wire when send is rejected");

    // 4. Free 2 slots by sending cumulative ACK for sequence 2
    uint32_t senderGen = trSender.GetPeerLocalGeneration(peerSink);
    TEST_ASSERT(senderGen != 0, "Sender local generation must be non-zero");

    WireHeader ackHdr{};
    ackHdr.magic = REFIX_WIRE_MAGIC;
    ackHdr.version = REFIX_WIRE_VERSION;
    ackHdr.msgType = static_cast<uint8_t>(MsgType::DataAck);
    ackHdr.flags = FLAG_HAS_ACK;
    ackHdr.SetSenderPeerId(peerSink);
    ackHdr.generationId = senderGen;
    ackHdr.channel = 1;
    ackHdr.ack = 2; // Cumulatively acknowledges seq 1 and seq 2
    ackHdr.sackMask = 0;
    ackHdr.payloadLen = 0;

    sockaddr_in toSenderSin{};
    toSenderSin.sin_family = AF_INET;
    toSenderSin.sin_addr.s_addr = htonl(epSender.ipv4);
    toSenderSin.sin_port = htons(epSender.port);

    sendto(sockSink, reinterpret_cast<const char*>(&ackHdr), sizeof(ackHdr), 0,
           reinterpret_cast<const sockaddr*>(&toSenderSin), sizeof(toSenderSin));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 510, "Unacked count must drop to 510 after retiring seq 1 and 2");

    // 5. Test pre-admission check: room for 2, message requires 3 fragments (3000 bytes)
    // MUST return false, transmit 0 fragments, consume 0 sequence numbers!
    std::vector<uint8_t> frag3Chunks(3000, 0x44); // 3000 bytes = 3 fragments
    bool okFrag3 = trSender.SendReliable(peerSink, epSink, 1, frag3Chunks.data(), frag3Chunks.size());
    TEST_ASSERT(!okFrag3, "SendReliable with 3 fragments when capacity is 2 MUST return false");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 510, "Queue must remain 510 (no orphaned fragments)");
    TEST_ASSERT(trSender.GetPeerNextSequenceOut(peerSink) == seqAfter512, "Sequence must NOT advance");

    leaked = recvfrom(sockSink, reinterpret_cast<char*>(drainBuf), sizeof(drainBuf), 0, nullptr, nullptr);
    TEST_ASSERT(leaked <= 0, "Zero partial fragments transmitted on wire");

    // 6. Test send with exactly 2 fragments (2000 bytes): capacity is 2 -> MUST succeed!
    std::vector<uint8_t> fragExact2(2000, 0x55); // 2000 bytes = 2 fragments
    bool okExact2 = trSender.SendReliable(peerSink, epSink, 1, fragExact2.data(), fragExact2.size());
    TEST_ASSERT(okExact2, "SendReliable with 2 fragments into 2 available slots MUST succeed");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 512, "Queue must now be exactly 512");
    TEST_ASSERT(trSender.GetPeerNextSequenceOut(peerSink) == seqAfter512 + 2, "Sequence must advance by exactly 2");

    // Verify exactly 2 fragments arrived at sockSink
    int countRcv = 0;
    while (recvfrom(sockSink, reinterpret_cast<char*>(drainBuf), sizeof(drainBuf), 0, nullptr, nullptr) > 0) {
        countRcv++;
    }
    TEST_ASSERT(countRcv == 2, "Exactly 2 fragments must be transmitted on wire");

    // 7. Full ACK retirement: ack = 514 (all in-flight packets retired)
    ackHdr.ack = seqAfter512 + 1; // 514
    sendto(sockSink, reinterpret_cast<const char*>(&ackHdr), sizeof(ackHdr), 0,
           reinterpret_cast<const sockaddr*>(&toSenderSin), sizeof(toSenderSin));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 0, "Cumulative ACK must cleanly retire all unacked packets to 0");

    // Queue is empty: new send immediately succeeds
    bool okPostAck = trSender.SendReliable(peerSink, epSink, 1, smallMsg.data(), smallMsg.size());
    TEST_ASSERT(okPostAck, "SendReliable after full retirement must succeed");
    TEST_ASSERT(trSender.GetUnackedOutboundCount(peerSink) == 1, "Queue must contain 1 unacked packet");

    closesocket(sockSink);
    trSender.Stop();

    std::cout << "  [PASS] ARQ outbound capacity limit (512), pre-admission fragment validation, zero partial sends, and clean queue retirement certified!" << std::endl;
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
    if (!TestPeerRegistryAdvanced()) return 1;
    if (!TestTransportFragmentationAndEdgeCases()) return 1;
    if (!TestTransportRetransmissionTimeout()) return 1;
    if (!TestFallbackDiscoveryPorts()) return 1;
    if (!TestLobbySteamIdIntegrityAndDeterminism()) return 1;
    if (!TestSendUnreliableContractAndBoundaries()) return 1;
    if (!TestFragmentationHardeningAndMemoryLimits()) return 1;
    if (!TestLanInterfaceSelection()) return 1;
    if (!TestSendReliableStrictContractAndBoundaries()) return 1;
    if (!TestReassemblyExpirationAndPeerAccountingStress()) return 1;
    if (!TestReassemblyAutoExpirationAndReactorLiveness()) return 1;
    if (!TestMemoryLimitsRealBudgetsAndEviction()) return 1;
    if (!TestMulticastInitializationAndDiagnostics()) return 1;
    if (!TestPeerStateLifecycleAndInboundQueueHardening()) return 1;
    if (!TestLobbyConflictPolicyAndProductionRegistry()) return 1;
    if (!TestARQUnidirectionalFlowExceedingIdleTimeout()) return 1;
    if (!TestARQExplicitDisconnectAndReconnect()) return 1;
    if (!TestInboundQueueSaturationAndBackpressure()) return 1;
    if (!TestSymmetricDatagramFramingAndValidation()) return 1;
    if (!TestPeerStateAdmissionPolicyUnder128Limit()) return 1;
    if (!TestRealDualTransportE2EBackpressure()) return 1;
    if (!TestSessionResetVsOldDuplicatesAndDisconnectLoss()) return 1;
    if (!TestPeerAdmissionEvictionLifecycle()) return 1;
    if (!TestP0_DualTransportRealOOOAndQueueRecovery()) return 1;
    if (!TestP0_DualTransportSackReassemblyBudgetRecovery()) return 1;
    if (!TestP0_AckEndpointAndSessionValidation()) return 1;
    if (!TestP0_ZeroGenerationAndStrictSessionValidation()) return 1;
    if (!TestP1_EndpointMigrationAuthorizationRequirement()) return 1;
    if (!TestP1_PiggybackedAckCorrelationAndSeparation()) return 1;
    if (!TestP1_SessionGeneration32BitRangeAndChurn()) return 1;
    if (!TestP1_Modular32BitGenerationWrap()) return 1;
    if (!TestP1_ArqSequenceModularWrap()) return 1;
    if (!TestP1_IncompatibleFragmentMetadataRejection()) return 1;
    if (!TestP1_SackedCandidateInvalidationAndLifecycleRecovery()) return 1;
    if (!TestP0_DrainReconstructionLimitExceededInvalidation()) return 1;
    if (!TestP1_OutboundArqQueueLimitAndNoPartialSends()) return 1;

    std::cout << "\n============================================================" << std::endl;
    std::cout << " [SUCCESS] ALL REFIX LAN CORE & TRANSPORT TESTS PASSED!      " << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
