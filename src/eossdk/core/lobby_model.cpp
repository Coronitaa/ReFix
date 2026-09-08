#include "lobby_model.h"

namespace refix {

bool AttributeValue::Equals(const AttributeValue& o) const {
    if (Type != o.Type) return false;
    switch (Type) {
        case EOS_ELobbyAttributeType::EOS_AT_INT64:   return AsInt64 == o.AsInt64;
        case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:  return AsDouble == o.AsDouble;
        case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN: return AsBool == o.AsBool;
        case EOS_ELobbyAttributeType::EOS_AT_STRING:
        default:                                      return AsUtf8 == o.AsUtf8;
    }
}

int AttributeValue::Compare(const AttributeValue& o) const {
    switch (Type) {
        case EOS_ELobbyAttributeType::EOS_AT_INT64:
            return AsInt64 < o.AsInt64 ? -1 : (AsInt64 > o.AsInt64 ? 1 : 0);
        case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:
            return AsDouble < o.AsDouble ? -1 : (AsDouble > o.AsDouble ? 1 : 0);
        case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN:
            return (int)AsBool - (int)o.AsBool;
        case EOS_ELobbyAttributeType::EOS_AT_STRING:
        default:
            return AsUtf8.compare(o.AsUtf8);
    }
}

std::string AttributeValue::Describe() const {
    char buf[64];
    switch (Type) {
        case EOS_ELobbyAttributeType::EOS_AT_INT64:   snprintf(buf, sizeof(buf), "%lld", (long long)AsInt64); return buf;
        case EOS_ELobbyAttributeType::EOS_AT_DOUBLE:  snprintf(buf, sizeof(buf), "%f", AsDouble); return buf;
        case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN: return AsBool ? "true" : "false";
        case EOS_ELobbyAttributeType::EOS_AT_STRING:
        default:                                      return AsUtf8;
    }
}

const LobbyMember* LobbyRecord::FindMember(const std::string& puid) const {
    for (const auto& m : Members) if (m.Puid == puid) return &m;
    return nullptr;
}

LobbyMember* LobbyRecord::FindMember(const std::string& puid) {
    for (auto& m : Members) if (m.Puid == puid) return &m;
    return nullptr;
}

const Attribute* LobbyRecord::FindAttribute(const std::string& key) const {
    for (const auto& a : Attributes) if (a.Key == key) return &a;
    return nullptr;
}

namespace {

void WriteValue(Writer& w, const AttributeValue& v) {
    w.U8((uint8_t)v.Type);
    w.I64(v.AsInt64);
    w.U64(*(const uint64_t*)&v.AsDouble);
    w.Bool(v.AsBool);
    w.Str(v.AsUtf8);
}

bool ReadValue(Reader& r, AttributeValue& v) {
    v.Type    = (EOS_ELobbyAttributeType)r.U8();
    v.AsInt64 = r.I64();
    uint64_t bits = r.U64();
    std::memcpy(&v.AsDouble, &bits, sizeof(double));
    v.AsBool  = r.Bool();
    v.AsUtf8  = r.Str();
    return r.Ok();
}

void WriteAttributes(Writer& w, const std::vector<Attribute>& attrs) {
    w.U32((uint32_t)attrs.size());
    for (const auto& a : attrs) {
        w.Str(a.Key);
        WriteValue(w, a.Value);
        w.U8((uint8_t)a.Visibility);
    }
}

bool ReadAttributes(Reader& r, std::vector<Attribute>& attrs, uint32_t maxCount = 512) {
    uint32_t n = r.U32();
    if (!r.Ok() || n > maxCount) return false;
    attrs.clear();
    attrs.reserve(n);
    for (uint32_t i = 0; i < n && r.Ok(); i++) {
        Attribute a;
        a.Key = r.Str(1024);
        if (!ReadValue(r, a.Value)) return false;
        a.Visibility = (EOS_ELobbyAttributeVisibility)r.U8();
        attrs.push_back(std::move(a));
    }
    return r.Ok();
}

} // namespace

void LobbyRecord::Write(Writer& w) const {
    w.Str(LobbyId);
    w.Str(OwnerPuid);
    w.Str(BucketId);
    w.U32(MaxMembers);
    w.U8((uint8_t)Permission);
    w.Bool(AllowInvites);
    w.Bool(AllowHostMigration);
    w.Bool(RtcRoomEnabled);
    w.Bool(AllowJoinById);
    w.Bool(RejoinAfterKickRequiresInvite);
    w.Bool(PresenceEnabled);
    w.U32(Revision);
    w.U32(HostAddress.Ipv4);
    w.U16(HostAddress.Port);
    w.U64(OwnerSteamId);
    WriteAttributes(w, Attributes);
    w.U32((uint32_t)Members.size());
    for (const auto& m : Members) {
        w.Str(m.Puid);
        w.Str(m.DisplayName);
        w.U32(m.Address.Ipv4);
        w.U16(m.Address.Port);
        w.U64(m.SteamId);
        w.Bool(m.IsOwner);
        WriteAttributes(w, m.Attributes);
    }
}

bool LobbyRecord::Read(Reader& r, LobbyRecord& out) {
    out.LobbyId   = r.Str(256);
    out.OwnerPuid = r.Str(64);
    out.BucketId  = r.Str(1024);
    out.MaxMembers = r.U32();
    out.Permission = (EOS_ELobbyPermissionLevel)r.U8();
    out.AllowInvites                  = r.Bool();
    out.AllowHostMigration            = r.Bool();
    out.RtcRoomEnabled                = r.Bool();
    out.AllowJoinById                 = r.Bool();
    out.RejoinAfterKickRequiresInvite = r.Bool();
    out.PresenceEnabled               = r.Bool();
    out.Revision                      = r.U32();
    out.HostAddress.Ipv4              = r.U32();
    out.HostAddress.Port              = r.U16();
    out.OwnerSteamId                  = r.U64();
    if (!ReadAttributes(r, out.Attributes)) return false;

    uint32_t memberCount = r.U32();
    // A lobby cannot have more members than it has seats; refuse absurd counts
    // rather than allocating whatever a datagram asks for.
    if (!r.Ok() || memberCount > 256) return false;
    out.Members.clear();
    out.Members.reserve(memberCount);
    for (uint32_t i = 0; i < memberCount && r.Ok(); i++) {
        LobbyMember m;
        m.Puid         = r.Str(64);
        m.DisplayName  = r.Str(256);
        m.Address.Ipv4 = r.U32();
        m.Address.Port = r.U16();
        m.SteamId      = r.U64();
        m.IsOwner      = r.Bool();
        if (!ReadAttributes(r, m.Attributes, 128)) return false;
        out.Members.push_back(std::move(m));
    }
    return r.Ok() && !out.LobbyId.empty();
}

bool SearchParameter::Matches(const LobbyRecord& lobby) const {
    // EOS exposes a handful of lobby fields as searchable pseudo-attributes.
    AttributeValue actual;
    const Attribute* attr = lobby.FindAttribute(Key);
    if (attr) {
        actual = attr->Value;
    } else if (Key == "bucket" || Key == EOS_LOBBY_SEARCH_BUCKET_ID) {
        actual.Type = EOS_ELobbyAttributeType::EOS_AT_STRING;
        actual.AsUtf8 = lobby.BucketId;
    } else if (Key == EOS_LOBBY_SEARCH_MINCURRENTMEMBERS) {
        actual.Type = EOS_ELobbyAttributeType::EOS_AT_INT64;
        actual.AsInt64 = (int64_t)lobby.Members.size();
    } else if (Key == EOS_LOBBY_SEARCH_MINSLOTSAVAILABLE) {
        actual.Type = EOS_ELobbyAttributeType::EOS_AT_INT64;
        actual.AsInt64 = (int64_t)lobby.AvailableSlots();
    } else {
        // A lobby that does not carry the attribute cannot satisfy a positive
        // filter on it, but it does satisfy "not equal".
        return Op == EOS_EComparisonOp::EOS_CO_NOTEQUAL ||
               Op == EOS_EComparisonOp::EOS_CO_NOTANYOF;
    }

    // ANYOF / ONEOF carry a *set*, which EOS transports as one string with the
    // members separated by ';'. Comparing that string whole never matches, so a
    // title looking for "any of my friends' lobbies" would find none of them.
    auto inSet = [&]() {
        if (actual.Type != EOS_ELobbyAttributeType::EOS_AT_STRING ||
            Value.Type  != EOS_ELobbyAttributeType::EOS_AT_STRING)
            return actual.Equals(Value);
        const std::string& set = Value.AsUtf8;
        size_t start = 0;
        while (start <= set.size()) {
            size_t end = set.find(';', start);
            if (end == std::string::npos) end = set.size();
            if (end > start && set.compare(start, end - start, actual.AsUtf8) == 0) return true;
            start = end + 1;
        }
        return false;
    };

    const int cmp = actual.Compare(Value);
    switch (Op) {
        case EOS_EComparisonOp::EOS_CO_EQUAL:              return actual.Equals(Value);
        case EOS_EComparisonOp::EOS_CO_NOTEQUAL:           return !actual.Equals(Value);
        case EOS_EComparisonOp::EOS_CO_GREATERTHAN:        return cmp > 0;
        case EOS_EComparisonOp::EOS_CO_GREATERTHANOREQUAL: return cmp >= 0;
        case EOS_EComparisonOp::EOS_CO_LESSTHAN:           return cmp < 0;
        case EOS_EComparisonOp::EOS_CO_LESSTHANOREQUAL:    return cmp <= 0;
        case EOS_EComparisonOp::EOS_CO_DISTANCE:           return true;   // no geo data to filter on
        case EOS_EComparisonOp::EOS_CO_ANYOF:              return inSet();
        case EOS_EComparisonOp::EOS_CO_NOTANYOF:           return !inSet();
        case EOS_EComparisonOp::EOS_CO_ONEOF:              return inSet();
        case EOS_EComparisonOp::EOS_CO_NOTONEOF:           return !inSet();
        case EOS_EComparisonOp::EOS_CO_CONTAINS:
            return actual.AsUtf8.find(Value.AsUtf8) != std::string::npos;
        default:                                           return true;
    }
}

} // namespace refix
