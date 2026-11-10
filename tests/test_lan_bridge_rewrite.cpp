// Self-check of the Steam bridge's rewrite of the endpoint a LanCore discovery packet advertises: only beacons,
// queries and responses change, only the endpoint field changes, and truncated packets are left alone.
#include "../src/lan_core/refix_lan_transport.h"
#include "../src/lan_core/refix_lan_wire.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace refix::lan;

static std::vector<uint8_t> Packet(MsgType type, const LanEndpoint& ep) {
    ByteWriter w;
    w.WriteString("app:3241660");
    w.WritePeerId(PeerId{ 0x1111, 0x2222 });
    w.WriteString("Corøna");
    w.WriteEndpoint(ep);
    w.WriteU64(76561198000000000ULL);
    std::vector<uint8_t> wire(sizeof(WireHeader) + w.Size());
    WireHeader h{};
    h.magic = REFIX_WIRE_MAGIC;
    h.version = REFIX_WIRE_VERSION;
    h.msgType = static_cast<uint8_t>(type);
    h.payloadLen = static_cast<uint16_t>(w.Size());
    memcpy(wire.data(), &h, sizeof(h));
    memcpy(wire.data() + sizeof(h), w.Data(), w.Size());
    return wire;
}

static LanEndpoint AdvertisedOf(const std::vector<uint8_t>& wire) {
    ByteReader r(wire.data() + sizeof(WireHeader), wire.size() - sizeof(WireHeader));
    r.ReadString();
    r.ReadPeerId();
    r.ReadString();
    return r.ReadEndpoint();
}

int main() {
    const LanEndpoint priv{ 0xC0A80105, 50123 };   // 192.168.1.5: the friend's own LAN, unreachable here
    for (MsgType t : { MsgType::DiscoveryBeacon, MsgType::DiscoveryQuery, MsgType::DiscoveryResponse }) {
        std::vector<uint8_t> wire = Packet(t, priv);
        const std::vector<uint8_t> before = wire;
        assert(LanTransport::RewriteAdvertisedEndpoint(wire.data(), wire.size(), 0xC0A80003, 61000));
        const LanEndpoint ep = AdvertisedOf(wire);
        assert(ep.ipv4 == 0xC0A80003 && ep.port == 61000);
        size_t changed = 0;
        for (size_t i = 0; i < wire.size(); ++i) changed += wire[i] != before[i];
        assert(changed <= 6);                       // nothing but the endpoint
    }
    {   // lobby join request: scope, peer, lobby id, client peer, client name, client endpoint
        ByteWriter w;
        w.WriteString("app:3241660");
        w.WritePeerId(PeerId{ 1, 2 });
        w.WriteString("lobby-42");
        w.WritePeerId(PeerId{ 1, 2 });
        w.WriteString("BluFix");
        w.WriteEndpoint(priv);
        std::vector<uint8_t> wire(sizeof(WireHeader) + w.Size());
        WireHeader h{};
        h.magic = REFIX_WIRE_MAGIC;
        h.version = REFIX_WIRE_VERSION;
        h.msgType = static_cast<uint8_t>(MsgType::LobbyJoinReq);
        h.payloadLen = static_cast<uint16_t>(w.Size());
        memcpy(wire.data(), &h, sizeof(h));
        memcpy(wire.data() + sizeof(h), w.Data(), w.Size());
        assert(LanTransport::RewriteAdvertisedEndpoint(wire.data(), wire.size(), 0x0A000001, 1234));
        ByteReader r(wire.data() + sizeof(WireHeader), w.Size());
        r.ReadString(); r.ReadPeerId(); r.ReadString(); r.ReadPeerId(); r.ReadString();
        const LanEndpoint ep = r.ReadEndpoint();
        assert(ep.ipv4 == 0x0A000001 && ep.port == 1234);
    }
    std::vector<uint8_t> lobby = Packet(MsgType::LobbyAnnouncement, priv);
    assert(!LanTransport::RewriteAdvertisedEndpoint(lobby.data(), lobby.size(), 1, 1));
    std::vector<uint8_t> cut = Packet(MsgType::DiscoveryBeacon, priv);
    cut.resize(sizeof(WireHeader) + 10);
    reinterpret_cast<WireHeader*>(cut.data())->payloadLen = 10;
    assert(!LanTransport::RewriteAdvertisedEndpoint(cut.data(), cut.size(), 1, 1));
    std::vector<uint8_t> bad = Packet(MsgType::DiscoveryBeacon, priv);
    bad[0] ^= 0xFF;                                 // not a ReFix wire packet
    assert(!LanTransport::RewriteAdvertisedEndpoint(bad.data(), bad.size(), 1, 1));
    {   // issue #20: a ZeroTier peer advertises its home LAN IP; reply to the ZeroTier IP the packet came from
        const LanEndpoint zt{ 0x0A3A3741, 47584 };  // 10.58.55.65
        const LanEndpoint home{ 0xC0A80141, 62098 }; // 192.168.1.65
        assert((ReachableFrom(zt, home) == LanEndpoint{ zt.ipv4, home.port }));
        assert(ReachableFrom(LanEndpoint{ 0x7F000001, 1 }, home) == home);   // same-PC instance keeps its address
        assert(!ReachableFrom(zt, LanEndpoint{}).IsValid());                  // nothing advertised stays empty
    }
    std::puts("test_lan_bridge_rewrite: OK");
    return 0;
}
