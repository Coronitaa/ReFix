// ReFix EOS v3 - session handle types.
//
// A session is modelled on top of the same replicated record a lobby uses -
// same discovery, same host authority, same member list - with the fields EOS
// exposes only for sessions carried in reserved attributes. That keeps one
// authoritative implementation instead of two that can disagree, which is what
// produced the old "session already exists" and ghost-session behaviour.
#pragma once
#include "refix_common.h"
#include "eos_handles.h"
#include "lobby_model.h"

// Reserved attribute keys. They are stripped from anything the title reads back,
// so a game never sees ReFix bookkeeping among its own attributes.
#define REFIX_SESSION_KIND_KEY   "__refix_kind"
#define REFIX_SESSION_ID_KEY     "__refix_session_id"
#define REFIX_SESSION_HOST_KEY   "__refix_host_address"
#define REFIX_SESSION_STATE_KEY  "__refix_session_state"
#define REFIX_SESSION_JIP_KEY    "__refix_join_in_progress"
#define REFIX_SESSION_INVITES_KEY "__refix_invites_allowed"
#define REFIX_SESSION_KIND_VALUE "session"

struct EOS_SessionDetailsHandle {
    uint32_t           Magic;
    refix::LobbyRecord Record;
};

struct EOS_SessionModificationHandle {
    uint32_t    Magic;
    std::string SessionName;     // local name the title uses
    std::string LocalPuid;
    bool        Creating = false;

    bool                              SetBucket = false;
    std::string                       Bucket;
    bool                              SetMaxPlayers = false;
    uint32_t                          MaxPlayers = 0;
    bool                              SetPermission = false;
    EOS_EOnlineSessionPermissionLevel Permission = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised;
    bool                              SetJoinInProgress = false;
    bool                              JoinInProgress = true;
    bool                              SetInvitesAllowed = false;
    bool                              InvitesAllowed = true;
    bool                              SetHostAddress = false;
    std::string                       HostAddress;

    std::vector<refix::Attribute> AddAttributes;
    std::vector<std::string>      RemoveAttributes;
};

struct EOS_SessionSearchHandle {
    uint32_t                            Magic;
    std::string                         LocalPuid;
    std::vector<refix::SearchParameter> Parameters;
    std::string                         SessionIdFilter;
    std::string                         TargetUserFilter;
    uint32_t                            MaxResults = 50;
    std::vector<refix::LobbyRecord>     Results;
};

struct EOS_ActiveSessionHandle {
    uint32_t              Magic;
    std::string           SessionName;
    refix::LobbyRecord    Record;
    EOS_EOnlineSessionState State = EOS_EOnlineSessionState::EOS_OSS_Pending;
};
