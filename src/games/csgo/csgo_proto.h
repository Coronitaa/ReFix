#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace refix::csgo {

constexpr uint32_t kProtoFlag = 0x80000000;
constexpr size_t kProtoHeaderSize = 8;
constexpr uint32_t kSOTypeSteamId = 1;

// SO Type IDs
constexpr uint32_t kSOTypeItem = 1;
constexpr uint32_t kSOTypePersonaDataPublic = 2;
constexpr uint32_t kSOTypeEquipSlot = 3;
constexpr uint32_t kSOTypeGameAccountClient = 7;
constexpr uint32_t kSOTypeSeasonalOperation = 40;
constexpr uint32_t kSOTypeDefaultEquippedDefinitionInstanceClient = 43;
constexpr uint32_t kSOTypeCoupon = 45;

// GC SDK base messages
constexpr uint32_t kMsgClientWelcome = 4004;
constexpr uint32_t kMsgServerWelcome = 4005;
constexpr uint32_t kMsgClientHello = 4006;
constexpr uint32_t kMsgServerHello = 4007;
constexpr uint32_t kMsgClientConnectionStatus = 4009;
constexpr uint32_t kMsgServerConnectionStatus = 4010;

// CS:GO specific GC messages
constexpr uint32_t kMsgGCUnlockCrate = 1007;
constexpr uint32_t kMsgGCUnlockCrateResponse = 1008;
constexpr uint32_t kMsgGCUseItemRequest = 1025;
constexpr uint32_t kMsgGCAdjustItemEquippedState = 1059;
constexpr uint32_t kMsgGCSetItemPositions = 1077;
constexpr uint32_t kMsgGCStorePurchaseInit = 2500;
constexpr uint32_t kMsgGCStorePurchaseInitResponse = 2501;
constexpr uint32_t kMsgGCStorePurchaseFinalize = 2504;
constexpr uint32_t kMsgGCStorePurchaseFinalizeResponse = 2505;
constexpr uint32_t kMsgGCStorePurchaseCancel = 2506;
constexpr uint32_t kMsgGCStorePurchaseCancelResponse = 2507;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingStart = 9101;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingGC2ClientReserve = 9107;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingClient2GCHello = 9109;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingGC2ClientHello = 9110;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingStop = 9103;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate = 9104;
constexpr uint32_t kMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockRequest = 9156;
constexpr uint32_t kMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse = 9157;
constexpr uint32_t kMsgGCCStrike15_v2_Client2GCPublicInfoRequest = 9158;
constexpr uint32_t kMsgGCCStrike15_v2_ClientGCRankUpdate = 9194;
constexpr uint32_t kMsgGCCStrike15_v2_Client2GCPlayersProfileRequest = 9128;
constexpr uint32_t kMsgGCCStrike15_v2_Client2GCAccountPrivacySettingsRequest = 9189;
constexpr uint32_t kMsgGCCStrike15_v2_Client2GCLobbyData_Request = 9163;
constexpr uint32_t kMsgGCCStrike15_v2_GetEventFavorites_Request = 9201;
constexpr uint32_t kMsgGCCStrike15_v2_ClientToGCPlayerCardRequest = 9204;

inline void PutVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 0x80) { out.push_back((uint8_t)(value | 0x80)); value >>= 7; }
    out.push_back((uint8_t)value);
}

inline void PutVarintField(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    PutVarint(out, (uint64_t)field << 3);
    PutVarint(out, value);
}

inline void PutFixed64Field(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    PutVarint(out, ((uint64_t)field << 3) | 1);
    for (int i = 0; i < 8; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

inline void PutFixed32Field(std::vector<uint8_t>& out, uint32_t field, uint32_t value) {
    PutVarint(out, ((uint64_t)field << 3) | 5);
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

inline void PutBytesField(std::vector<uint8_t>& out, uint32_t field, const std::vector<uint8_t>& bytes) {
    PutVarint(out, ((uint64_t)field << 3) | 2);
    PutVarint(out, bytes.size());
    out.insert(out.end(), bytes.begin(), bytes.end());
}

inline void PutStringField(std::vector<uint8_t>& out, uint32_t field, const std::string& str) {
    std::vector<uint8_t> bytes(str.begin(), str.end());
    PutBytesField(out, field, bytes);
}

inline bool GetVarint(const uint8_t*& p, const uint8_t* end, uint64_t& value) {
    value = 0;
    for (int shift = 0; p < end && shift < 64; shift += 7) {
        uint8_t b = *p++;
        value |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

} // namespace refix::csgo
