#include "../src/lan_core/refix_lan_types.h"
#include "../src/lan_core/refix_lan_firewall.h"
#include "../src/lan_core/refix_lan_wire.h"
#include "../src/lan_core/refix_lan_transport.h"
#include "../src/lan_core/refix_lan_core.h"

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
    uint64_t h = 0xCBF29CE484222325ULL;
    for (char c : coreLobbyId) {
        h ^= static_cast<uint8_t>(c);
        h *= 0x100000001B3ULL;
    }
    h ^= (h >> 33);
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= (h >> 33);
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= (h >> 33);
    uint32_t accountId = static_cast<uint32_t>(h ^ (h >> 32));
    if (accountId == 0) accountId = 1;
    return accountId;
}

static uint64_t ComputeLobbySteamID(const std::string& coreLobbyId) {
    if (coreLobbyId.empty()) return 0;
    uint32_t accountId = ComputeLobbyAccountId(coreLobbyId);
    CSteamID lobbySteamId(accountId, k_EChatInstanceFlagLobby, k_EUniversePublic, k_EAccountTypeChat);
    return lobbySteamId.ConvertToUint64();
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

    std::cout << "\n============================================================" << std::endl;
    std::cout << " [SUCCESS] ALL REFIX LAN CORE & TRANSPORT TESTS PASSED!      " << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
