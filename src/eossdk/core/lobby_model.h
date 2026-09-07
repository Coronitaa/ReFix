// ReFix EOS v3 - the authoritative lobby model.
//
// A lobby is whatever its host says it is. The host owns the record; every
// other participant holds a replica that it only ever updates from what the
// host publishes. Nothing here is invented: max members, bucket, permission
// level and every attribute come from the EOS_LobbyModification_* calls the
// title actually made, so a two-player duel is a two-player duel and a 16-player
// raid is a 16-player raid.
#pragma once
#include "refix_common.h"
#include "../net/refix_wire.h"
#include "../net/refix_transport.h"

namespace refix {

// An attribute value, mirroring the EOS_Lobby_AttributeData union but owning
// its storage so it survives the call that produced it.
struct AttributeValue {
    EOS_ELobbyAttributeType Type = EOS_ELobbyAttributeType::EOS_AT_STRING;
    int64_t      AsInt64  = 0;
    double       AsDouble = 0.0;
    bool         AsBool   = false;
    std::string  AsUtf8;

    bool Equals(const AttributeValue& o) const;
    // Ordering used by the search comparisons EOS defines (<, <=, >, >=).
    int  Compare(const AttributeValue& o) const;
    std::string Describe() const;
};

struct Attribute {
    std::string                   Key;
    AttributeValue                Value;
    EOS_ELobbyAttributeVisibility Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
};

struct LobbyMember {
    std::string            Puid;
    std::string            DisplayName;
    Endpoint               Address;          // where to reach this member
    std::vector<Attribute> Attributes;       // member attributes, host-replicated
    bool                   IsOwner = false;
};

struct LobbyRecord {
    std::string  LobbyId;
    std::string  OwnerPuid;
    std::string  BucketId;
    uint32_t     MaxMembers = 0;
    EOS_ELobbyPermissionLevel Permission = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
    bool         AllowInvites = true;
    bool         AllowHostMigration = false;
    bool         RtcRoomEnabled = false;
    bool         AllowJoinById = true;
    bool         RejoinAfterKickRequiresInvite = false;
    bool         PresenceEnabled = false;

    std::vector<Attribute>   Attributes;
    std::vector<LobbyMember> Members;

    Endpoint     HostAddress;                // where join requests go
    uint64_t     LastSeenMs = 0;             // for expiring stale advertisements
    uint32_t     Revision = 0;               // increases on every host-side change

    uint32_t AvailableSlots() const {
        uint32_t used = (uint32_t)Members.size();
        return MaxMembers > used ? MaxMembers - used : 0u;
    }
    const LobbyMember* FindMember(const std::string& puid) const;
    LobbyMember*       FindMember(const std::string& puid);
    const Attribute*   FindAttribute(const std::string& key) const;

    void Write(Writer& w) const;
    static bool Read(Reader& r, LobbyRecord& out);
};

// A single search condition, as set by EOS_LobbySearch_SetParameter.
struct SearchParameter {
    std::string                Key;
    AttributeValue             Value;
    EOS_EComparisonOp          Op = EOS_EComparisonOp::EOS_CO_EQUAL;
    bool Matches(const LobbyRecord& lobby) const;
};

} // namespace refix
