#include "../src/refix_online/refix_backend_protocol.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace ReFixOnline;

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix Online v2 - Backend Protocol Serialization Unit Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // TEST 1: Header Serialization & Deserialization
    std::cout << "\n[TEST 1] Packet Header Serialization..." << std::endl;
    RefixPacketHeader hdr = {};
    hdr.Magic = REFIX_PROTOCOL_MAGIC;
    hdr.Version = REFIX_PROTOCOL_VERSION;
    hdr.MessageType = MSG_AUTH;
    hdr.RequestId = 0x123456789ABCDEF0ULL;
    hdr.PayloadLength = 128;

    ByteWriter wHeader;
    SerializeHeader(hdr, wHeader);
    assert(wHeader.GetSize() == sizeof(RefixPacketHeader));

    ByteReader rHeader(wHeader.GetData(), wHeader.GetSize());
    RefixPacketHeader parsedHdr = {};
    assert(DeserializeHeader(rHeader, parsedHdr));
    assert(parsedHdr.Magic == REFIX_PROTOCOL_MAGIC);
    assert(parsedHdr.Version == REFIX_PROTOCOL_VERSION);
    assert(parsedHdr.MessageType == MSG_AUTH);
    assert(parsedHdr.RequestId == 0x123456789ABCDEF0ULL);
    assert(parsedHdr.PayloadLength == 128);
    std::cout << "  Header serialization roundtrip 100% verified!" << std::endl;

    // TEST 2: Malformed Header / Bad Magic / Oversized Payload Rejection
    std::cout << "\n[TEST 2] Malformed Header Rejection..." << std::endl;
    hdr.Magic = 0xDEADBEEF; // Bad magic
    ByteWriter wBadMagic;
    SerializeHeader(hdr, wBadMagic);
    ByteReader rBadMagic(wBadMagic.GetData(), wBadMagic.GetSize());
    RefixPacketHeader outBad = {};
    assert(!DeserializeHeader(rBadMagic, outBad));

    hdr.Magic = REFIX_PROTOCOL_MAGIC;
    hdr.PayloadLength = 1000000; // Oversized > 64KB
    ByteWriter wOver;
    SerializeHeader(hdr, wOver);
    ByteReader rOver(wOver.GetData(), wOver.GetSize());
    assert(!DeserializeHeader(rOver, outBad));
    std::cout << "  Bad magic and oversized payload safely rejected!" << std::endl;

    // TEST 3: Auth Packets
    std::cout << "\n[TEST 3] Auth Request & Result Serialization..." << std::endl;
    ByteWriter wAuth;
    WriteAuthRequest(wAuth, REFIX_PROTOCOL_VERSION, "2b8db5d0c5cbd56ff14dff84a61cd9a2", "TestPlayer");
    ByteReader rAuth(wAuth.GetData(), wAuth.GetSize());
    uint16_t ver = 0;
    std::string uid, name;
    assert(ReadAuthRequest(rAuth, ver, uid, name));
    assert(ver == REFIX_PROTOCOL_VERSION);
    assert(uid == "2b8db5d0c5cbd56ff14dff84a61cd9a2");
    assert(name == "TestPlayer");

    ByteWriter wAuthRes;
    WriteAuthResult(wAuthRes, SUCCESS, uid, 1700000000000ULL, "sess_tok_998877");
    ByteReader rAuthRes(wAuthRes.GetData(), wAuthRes.GetSize());
    EBackendResult bRes = SERVER_ERROR;
    uint64_t sTime = 0;
    std::string tok;
    assert(ReadAuthResult(rAuthRes, bRes, uid, sTime, tok));
    assert(bRes == SUCCESS);
    assert(sTime == 1700000000000ULL);
    assert(tok == "sess_tok_998877");
    std::cout << "  Auth packet serialization verified!" << std::endl;

    // TEST 4: Create Lobby Packets & Attributes
    std::cout << "\n[TEST 4] Create Lobby Request & Result Serialization..." << std::endl;
    std::unordered_map<std::string, std::string> inAttrs = {
        { "room_name", "Awesome Match" },
        { "map", "hotel_stage_01" },
        { "game_mode", "deathmatch" }
    };
    ByteWriter wCreate;
    WriteCreateLobbyRequest(wCreate, 8, inAttrs);
    ByteReader rCreate(wCreate.GetData(), wCreate.GetSize());
    uint32_t outMax = 0;
    std::unordered_map<std::string, std::string> outAttrs;
    assert(ReadCreateLobbyRequest(rCreate, outMax, outAttrs));
    assert(outMax == 8);
    assert(outAttrs.size() == 3);
    assert(outAttrs["room_name"] == "Awesome Match");
    assert(outAttrs["map"] == "hotel_stage_01");

    LobbyData inLob;
    inLob.lobbyId = "lob_778899aabbcc";
    inLob.ownerUserId = "2b8db5d0c5cbd56ff14dff84a61cd9a2";
    inLob.maxMembers = 8;
    inLob.currentMembers = 1;
    inLob.state = 2;
    inLob.createdAt = 50000;
    inLob.attributes = inAttrs;
    inLob.members.push_back({ "2b8db5d0c5cbd56ff14dff84a61cd9a2", "TestPlayer", 50000, true });

    ByteWriter wCreateRes;
    WriteCreateLobbyResult(wCreateRes, SUCCESS, inLob);
    ByteReader rCreateRes(wCreateRes.GetData(), wCreateRes.GetSize());
    LobbyData outLob;
    assert(ReadCreateLobbyResult(rCreateRes, bRes, outLob));
    assert(bRes == SUCCESS);
    assert(outLob.lobbyId == "lob_778899aabbcc");
    assert(outLob.maxMembers == 8);
    assert(outLob.members.size() == 1);
    assert(outLob.members[0].isOwner == true);
    std::cout << "  Create lobby packet serialization verified!" << std::endl;

    // TEST 5: Notifications Serialization
    std::cout << "\n[TEST 5] Member Joined & Left Notifications..." << std::endl;
    ByteWriter wNotif;
    LobbyMemberInfo newM = { "6e397b6ad903cce2a3bd43df18810fd9", "JoinedPlayer", 60000, false };
    WriteMemberJoinedNotification(wNotif, "lob_778899aabbcc", newM);
    ByteReader rNotif(wNotif.GetData(), wNotif.GetSize());
    std::string nLobId;
    LobbyMemberInfo outM;
    assert(ReadMemberJoinedNotification(rNotif, nLobId, outM));
    assert(nLobId == "lob_778899aabbcc");
    assert(outM.userId == "6e397b6ad903cce2a3bd43df18810fd9");
    assert(outM.displayName == "JoinedPlayer");
    assert(!outM.isOwner);
    std::cout << "  Notifications serialization verified!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Backend Protocol Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
