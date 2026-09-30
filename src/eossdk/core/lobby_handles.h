// ReFix EOS v3 - lobby handle types.
//
// EOS hands the title three kinds of lobby handle. Each one is a real, typed
// object here rather than an integer the emulator later has to guess the
// meaning of, and each carries a magic word so a stale handle is refused.
#pragma once
#include "refix_common.h"
#include "eos_handles.h"
#include "lobby_model.h"

// A snapshot of a lobby as it was when the handle was produced, which is
// exactly what EOS_Lobby_CopyLobbyDetailsHandle promises.
struct EOS_LobbyDetailsHandle {
    uint32_t             Magic;
    refix::LobbyRecord   Record;
    std::string          LocalPuid;
};

// Accumulates the changes a title makes through EOS_LobbyModification_*.
// Nothing is applied until EOS_Lobby_UpdateLobby, matching the real SDK.
struct EOS_LobbyModificationHandle {
    uint32_t    Magic;
    std::string LobbyId;
    std::string LocalPuid;

    bool                       SetBucket = false;
    std::string                Bucket;
    bool                       SetPermission = false;
    EOS_ELobbyPermissionLevel  Permission = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
    bool                       SetMaxMembers = false;
    uint32_t                   MaxMembers = 0;
    bool                       SetInvitesAllowed = false;
    bool                       InvitesAllowed = true;

    std::vector<refix::Attribute> AddAttributes;
    std::vector<std::string>      RemoveAttributes;
    std::vector<refix::Attribute> AddMemberAttributes;
    std::vector<std::string>      RemoveMemberAttributes;
};

struct EOS_LobbySearchHandle {
    uint32_t                            Magic;
    std::string                         LocalPuid;
    std::vector<refix::SearchParameter> Parameters;
    std::string                         LobbyIdFilter;
    std::string                         TargetUserFilter;
    uint32_t                            MaxResults = 50;
    std::vector<refix::LobbyRecord>     Results;
};
