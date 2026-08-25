#include "../src/eos/eos_lobby.h"
#include <cstddef>
#include <iostream>

// Verify EOS ABI Layouts via static_assert
static_assert(sizeof(EOS_Bool) == 4, "EOS_Bool must be 4 bytes (int32_t)");
static_assert(sizeof(EOS_ELobbyPermissionLevel) == 4, "EOS_ELobbyPermissionLevel must be 4 bytes");

static_assert(sizeof(EOS_Lobby_CreateLobbyOptions) == 88, "EOS_Lobby_CreateLobbyOptions size mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, ApiVersion) == 0, "ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, LocalUserId) == 8, "LocalUserId offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, MaxLobbyMembers) == 16, "MaxLobbyMembers offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, PermissionLevel) == 20, "PermissionLevel offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, bPresenceEnabled) == 24, "bPresenceEnabled offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, Attributes) == 32, "Attributes offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, BucketId) == 40, "BucketId offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, bDisableHostMigration) == 48, "bDisableHostMigration offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, bEnableRTCOptions) == 52, "bEnableRTCOptions offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, Reserved) == 56, "Reserved offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, AllowedPlatformIdsCount) == 64, "AllowedPlatformIdsCount offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, AllowedPlatformIds) == 72, "AllowedPlatformIds offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyOptions, bCrossplayOptOut) == 80, "bCrossplayOptOut offset mismatch");

static_assert(sizeof(EOS_Lobby_CreateLobbyCallbackInfo) == 24, "EOS_Lobby_CreateLobbyCallbackInfo size mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyCallbackInfo, ResultCode) == 0, "ResultCode offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyCallbackInfo, ClientData) == 8, "ClientData offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbyCallbackInfo, LobbyId) == 16, "LobbyId offset mismatch");

static_assert(sizeof(EOS_Lobby_AttributeData) == 32, "EOS_Lobby_AttributeData size mismatch");
static_assert(offsetof(EOS_Lobby_AttributeData, ApiVersion) == 0, "AttributeData ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_AttributeData, Key) == 8, "AttributeData Key offset mismatch");
static_assert(offsetof(EOS_Lobby_AttributeData, Value) == 16, "AttributeData Value offset mismatch");
static_assert(offsetof(EOS_Lobby_AttributeData, ValueType) == 24, "AttributeData ValueType offset mismatch");

static_assert(sizeof(EOS_Lobby_Attribute) == 24, "EOS_Lobby_Attribute size mismatch");
static_assert(offsetof(EOS_Lobby_Attribute, ApiVersion) == 0, "Attribute ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_Attribute, Data) == 8, "Attribute Data offset mismatch");
static_assert(offsetof(EOS_Lobby_Attribute, Visibility) == 16, "Attribute Visibility offset mismatch");

static_assert(sizeof(EOS_Lobby_UpdateLobbyOptions) == 16, "EOS_Lobby_UpdateLobbyOptions size mismatch");
static_assert(offsetof(EOS_Lobby_UpdateLobbyOptions, ApiVersion) == 0, "UpdateLobbyOptions ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_UpdateLobbyOptions, LobbyModificationHandle) == 8, "UpdateLobbyOptions Handle offset mismatch");

static_assert(sizeof(EOS_Lobby_UpdateLobbyCallbackInfo) == 24, "EOS_Lobby_UpdateLobbyCallbackInfo size mismatch");
static_assert(offsetof(EOS_Lobby_UpdateLobbyCallbackInfo, ResultCode) == 0, "UpdateLobbyCallbackInfo ResultCode offset mismatch");
static_assert(offsetof(EOS_Lobby_UpdateLobbyCallbackInfo, ClientData) == 8, "UpdateLobbyCallbackInfo ClientData offset mismatch");
static_assert(offsetof(EOS_Lobby_UpdateLobbyCallbackInfo, LobbyId) == 16, "UpdateLobbyCallbackInfo LobbyId offset mismatch");

static_assert(sizeof(EOS_Lobby_DestroyLobbyOptions) == 24, "EOS_Lobby_DestroyLobbyOptions size mismatch");
static_assert(offsetof(EOS_Lobby_DestroyLobbyOptions, ApiVersion) == 0, "DestroyLobbyOptions ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_DestroyLobbyOptions, LocalUserId) == 8, "DestroyLobbyOptions LocalUserId offset mismatch");
static_assert(offsetof(EOS_Lobby_DestroyLobbyOptions, LobbyId) == 16, "DestroyLobbyOptions LobbyId offset mismatch");

static_assert(sizeof(EOS_Lobby_DestroyLobbyCallbackInfo) == 24, "EOS_Lobby_DestroyLobbyCallbackInfo size mismatch");
static_assert(offsetof(EOS_Lobby_DestroyLobbyCallbackInfo, ResultCode) == 0, "DestroyLobbyCallbackInfo ResultCode offset mismatch");
static_assert(offsetof(EOS_Lobby_DestroyLobbyCallbackInfo, ClientData) == 8, "DestroyLobbyCallbackInfo ClientData offset mismatch");
static_assert(offsetof(EOS_Lobby_DestroyLobbyCallbackInfo, LobbyId) == 16, "DestroyLobbyCallbackInfo LobbyId offset mismatch");

static_assert(sizeof(EOS_Lobby_CreateLobbySearchOptions) == 8, "EOS_Lobby_CreateLobbySearchOptions size mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbySearchOptions, ApiVersion) == 0, "CreateLobbySearchOptions ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_CreateLobbySearchOptions, MaxResults) == 4, "CreateLobbySearchOptions MaxResults offset mismatch");

static_assert(sizeof(EOS_LobbySearch_FindOptions) == 16, "EOS_LobbySearch_FindOptions size mismatch");
static_assert(offsetof(EOS_LobbySearch_FindOptions, ApiVersion) == 0, "FindOptions ApiVersion offset mismatch");
static_assert(offsetof(EOS_LobbySearch_FindOptions, LocalUserId) == 8, "FindOptions LocalUserId offset mismatch");

static_assert(sizeof(EOS_LobbySearch_FindCallbackInfo) == 16, "EOS_LobbySearch_FindCallbackInfo size mismatch");
static_assert(offsetof(EOS_LobbySearch_FindCallbackInfo, ResultCode) == 0, "FindCallbackInfo ResultCode offset mismatch");
static_assert(offsetof(EOS_LobbySearch_FindCallbackInfo, ClientData) == 8, "FindCallbackInfo ClientData offset mismatch");

static_assert(sizeof(EOS_Lobby_JoinLobbyOptions) == 32, "EOS_Lobby_JoinLobbyOptions size mismatch");
static_assert(offsetof(EOS_Lobby_JoinLobbyOptions, ApiVersion) == 0, "JoinLobbyOptions ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_JoinLobbyOptions, LobbyDetailsHandle) == 8, "JoinLobbyOptions Handle offset mismatch");
static_assert(offsetof(EOS_Lobby_JoinLobbyOptions, LocalUserId) == 16, "JoinLobbyOptions LocalUserId offset mismatch");

static_assert(sizeof(EOS_Lobby_JoinLobbyCallbackInfo) == 24, "EOS_Lobby_JoinLobbyCallbackInfo size mismatch");
static_assert(offsetof(EOS_Lobby_JoinLobbyCallbackInfo, ResultCode) == 0, "JoinLobbyCallbackInfo ResultCode offset mismatch");
static_assert(offsetof(EOS_Lobby_JoinLobbyCallbackInfo, ClientData) == 8, "JoinLobbyCallbackInfo ClientData offset mismatch");
static_assert(offsetof(EOS_Lobby_JoinLobbyCallbackInfo, LobbyId) == 16, "JoinLobbyCallbackInfo LobbyId offset mismatch");

static_assert(sizeof(EOS_Lobby_LeaveLobbyOptions) == 24, "EOS_Lobby_LeaveLobbyOptions size mismatch");
static_assert(offsetof(EOS_Lobby_LeaveLobbyOptions, ApiVersion) == 0, "LeaveLobbyOptions ApiVersion offset mismatch");
static_assert(offsetof(EOS_Lobby_LeaveLobbyOptions, LocalUserId) == 8, "LeaveLobbyOptions LocalUserId offset mismatch");
static_assert(offsetof(EOS_Lobby_LeaveLobbyOptions, LobbyId) == 16, "LeaveLobbyOptions LobbyId offset mismatch");

static_assert(sizeof(EOS_Lobby_LeaveLobbyCallbackInfo) == 24, "EOS_Lobby_LeaveLobbyCallbackInfo size mismatch");
static_assert(offsetof(EOS_Lobby_LeaveLobbyCallbackInfo, ResultCode) == 0, "LeaveLobbyCallbackInfo ResultCode offset mismatch");
static_assert(offsetof(EOS_Lobby_LeaveLobbyCallbackInfo, ClientData) == 8, "LeaveLobbyCallbackInfo ClientData offset mismatch");
static_assert(offsetof(EOS_Lobby_LeaveLobbyCallbackInfo, LobbyId) == 16, "LeaveLobbyCallbackInfo LobbyId offset mismatch");

#pragma pack(push, 8)
struct Steam_GetAuthSessionTicketResponse_t {
    enum { k_iCallback = 163 };
    uint32_t m_hAuthTicket;
    int32_t  m_eResult;
};
static_assert(sizeof(Steam_GetAuthSessionTicketResponse_t) == 8, "GetAuthSessionTicketResponse_t size mismatch");
static_assert(offsetof(Steam_GetAuthSessionTicketResponse_t, m_hAuthTicket) == 0, "m_hAuthTicket offset mismatch");
static_assert(offsetof(Steam_GetAuthSessionTicketResponse_t, m_eResult) == 4, "m_eResult offset mismatch");

struct Steam_GetTicketForWebApiResponse_t {
    enum { k_iCallback = 168 };
    uint32_t m_hAuthTicket;
    int32_t  m_eResult;
    int32_t  m_cubTicket;
    uint8_t  m_rgubTicket[1024];
};
static_assert(sizeof(Steam_GetTicketForWebApiResponse_t) == 1036, "GetTicketForWebApiResponse_t size mismatch");
static_assert(offsetof(Steam_GetTicketForWebApiResponse_t, m_hAuthTicket) == 0, "m_hAuthTicket offset mismatch");
static_assert(offsetof(Steam_GetTicketForWebApiResponse_t, m_eResult) == 4, "m_eResult offset mismatch");
static_assert(offsetof(Steam_GetTicketForWebApiResponse_t, m_cubTicket) == 8, "m_cubTicket offset mismatch");
static_assert(offsetof(Steam_GetTicketForWebApiResponse_t, m_rgubTicket) == 12, "m_rgubTicket offset mismatch");
#pragma pack(pop)

int main() {
    std::cout << "[PASS] All EOS and Steam ABI static_asserts verified successfully!" << std::endl;
    return 0;
}
