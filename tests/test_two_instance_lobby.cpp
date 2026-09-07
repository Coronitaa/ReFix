// ReFix EOS v3 - two-process interconnection test.
//
// This is not a mock: it loads the *shipped* EOSSDK-Win64-Shipping.dll by name,
// resolves the same exports a game resolves, and drives the real API over the
// real UDP transport. Run it twice - once as `host`, once as `client` - and the
// two processes must find each other, join, and exchange a P2P packet.
//
//   test_two_instance_lobby.exe host   <dll> [seconds]
//   test_two_instance_lobby.exe client <dll> [seconds]
//
// The two processes are given different REFIX_USER_INSTANCE values by the
// launcher, which is what lets one machine stand in for two players.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

#define EOS_MONOLITHIC 1
#include "../src/eossdk/sdk/eos_sdk.h"
#include "../src/eossdk/sdk/eos_lobby.h"
#include "../src/eossdk/sdk/eos_p2p.h"

// ---------------------------------------------------------------------------
static HMODULE g_dll = nullptr;
static int     g_failures = 0;

template <typename T>
static T Resolve(const char* name) {
    T fn = (T)GetProcAddress(g_dll, name);
    if (!fn) { printf("[FAIL] export not found: %s\n", name); g_failures++; }
    return fn;
}

#define LOADFN(name) auto name = Resolve<decltype(&::name)>(#name)

static void Check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "[ OK ]" : "[FAIL]", what);
    if (!ok) g_failures++;
}

// ---------------------------------------------------------------------------
struct Api {
    decltype(&EOS_Initialize)                       Initialize;
    decltype(&EOS_Platform_Create)                  PlatformCreate;
    decltype(&EOS_Platform_Tick)                    PlatformTick;
    decltype(&EOS_Platform_GetConnectInterface)     GetConnect;
    decltype(&EOS_Platform_GetLobbyInterface)       GetLobby;
    decltype(&EOS_Platform_GetP2PInterface)         GetP2P;
    decltype(&EOS_Connect_Login)                    ConnectLogin;
    decltype(&EOS_ProductUserId_ToString)           PuidToString;
    decltype(&EOS_Lobby_CreateLobby)                CreateLobby;
    decltype(&EOS_Lobby_CreateLobbySearch)          CreateLobbySearch;
    decltype(&EOS_Lobby_CopyLobbyDetailsHandle)     CopyDetailsHandle;
    decltype(&EOS_Lobby_JoinLobby)                  JoinLobby;
    decltype(&EOS_LobbySearch_SetParameter)         SearchSetParameter;
    decltype(&EOS_LobbySearch_Find)                 SearchFind;
    decltype(&EOS_LobbySearch_GetSearchResultCount) SearchCount;
    decltype(&EOS_LobbySearch_CopySearchResultByIndex) SearchCopy;
    decltype(&EOS_LobbyDetails_CopyInfo)            DetailsCopyInfo;
    decltype(&EOS_LobbyDetails_Info_Release)        DetailsInfoRelease;
    decltype(&EOS_LobbyDetails_GetMemberCount)      DetailsMemberCount;
    decltype(&EOS_LobbyDetails_GetMemberByIndex)    DetailsMemberByIndex;
    decltype(&EOS_LobbyDetails_GetLobbyOwner)       DetailsOwner;
    decltype(&EOS_LobbyDetails_CopyAttributeByKey)  DetailsAttrByKey;
    decltype(&EOS_Lobby_Attribute_Release)          AttrRelease;
    decltype(&EOS_P2P_SendPacket)                   P2PSend;
    decltype(&EOS_P2P_ReceivePacket)                P2PReceive;
    decltype(&EOS_P2P_GetNextReceivedPacketSize)    P2PNextSize;
    decltype(&EOS_P2P_AcceptConnection)             P2PAccept;
};

static Api LoadApi() {
    Api a{};
    a.Initialize          = Resolve<decltype(a.Initialize)>("EOS_Initialize");
    a.PlatformCreate      = Resolve<decltype(a.PlatformCreate)>("EOS_Platform_Create");
    a.PlatformTick        = Resolve<decltype(a.PlatformTick)>("EOS_Platform_Tick");
    a.GetConnect          = Resolve<decltype(a.GetConnect)>("EOS_Platform_GetConnectInterface");
    a.GetLobby            = Resolve<decltype(a.GetLobby)>("EOS_Platform_GetLobbyInterface");
    a.GetP2P              = Resolve<decltype(a.GetP2P)>("EOS_Platform_GetP2PInterface");
    a.ConnectLogin        = Resolve<decltype(a.ConnectLogin)>("EOS_Connect_Login");
    a.PuidToString        = Resolve<decltype(a.PuidToString)>("EOS_ProductUserId_ToString");
    a.CreateLobby         = Resolve<decltype(a.CreateLobby)>("EOS_Lobby_CreateLobby");
    a.CreateLobbySearch   = Resolve<decltype(a.CreateLobbySearch)>("EOS_Lobby_CreateLobbySearch");
    a.CopyDetailsHandle   = Resolve<decltype(a.CopyDetailsHandle)>("EOS_Lobby_CopyLobbyDetailsHandle");
    a.JoinLobby           = Resolve<decltype(a.JoinLobby)>("EOS_Lobby_JoinLobby");
    a.SearchSetParameter  = Resolve<decltype(a.SearchSetParameter)>("EOS_LobbySearch_SetParameter");
    a.SearchFind          = Resolve<decltype(a.SearchFind)>("EOS_LobbySearch_Find");
    a.SearchCount         = Resolve<decltype(a.SearchCount)>("EOS_LobbySearch_GetSearchResultCount");
    a.SearchCopy          = Resolve<decltype(a.SearchCopy)>("EOS_LobbySearch_CopySearchResultByIndex");
    a.DetailsCopyInfo     = Resolve<decltype(a.DetailsCopyInfo)>("EOS_LobbyDetails_CopyInfo");
    a.DetailsInfoRelease  = Resolve<decltype(a.DetailsInfoRelease)>("EOS_LobbyDetails_Info_Release");
    a.DetailsMemberCount  = Resolve<decltype(a.DetailsMemberCount)>("EOS_LobbyDetails_GetMemberCount");
    a.DetailsMemberByIndex= Resolve<decltype(a.DetailsMemberByIndex)>("EOS_LobbyDetails_GetMemberByIndex");
    a.DetailsOwner        = Resolve<decltype(a.DetailsOwner)>("EOS_LobbyDetails_GetLobbyOwner");
    a.DetailsAttrByKey    = Resolve<decltype(a.DetailsAttrByKey)>("EOS_LobbyDetails_CopyAttributeByKey");
    a.AttrRelease         = Resolve<decltype(a.AttrRelease)>("EOS_Lobby_Attribute_Release");
    a.P2PSend             = Resolve<decltype(a.P2PSend)>("EOS_P2P_SendPacket");
    a.P2PReceive          = Resolve<decltype(a.P2PReceive)>("EOS_P2P_ReceivePacket");
    a.P2PNextSize         = Resolve<decltype(a.P2PNextSize)>("EOS_P2P_GetNextReceivedPacketSize");
    a.P2PAccept           = Resolve<decltype(a.P2PAccept)>("EOS_P2P_AcceptConnection");
    return a;
}

// ---------------------------------------------------------------------------
static const char* kBucket = "refix_selftest";
static EOS_ProductUserId g_localUser = nullptr;
static char              g_lobbyId[256] = {0};
static bool              g_loginDone = false;
static bool              g_createDone = false;
static bool              g_searchDone = false;
static bool              g_joinDone = false;
static EOS_EResult       g_joinResult = EOS_EResult::EOS_UnexpectedError;

static void EOS_CALL OnLogin(const EOS_Connect_LoginCallbackInfo* info) {
    g_loginDone = true;
    g_localUser = info->LocalUserId;
    printf("       login result=%d\n", (int)info->ResultCode);
}
static void EOS_CALL OnCreate(const EOS_Lobby_CreateLobbyCallbackInfo* info) {
    g_createDone = true;
    if (info->LobbyId) strncpy(g_lobbyId, info->LobbyId, sizeof(g_lobbyId) - 1);
    printf("       create result=%d lobbyId=%s\n", (int)info->ResultCode, info->LobbyId ? info->LobbyId : "");
}
static void EOS_CALL OnFind(const EOS_LobbySearch_FindCallbackInfo* info) {
    g_searchDone = true;
    printf("       search result=%d\n", (int)info->ResultCode);
}
static void EOS_CALL OnJoin(const EOS_Lobby_JoinLobbyCallbackInfo* info) {
    g_joinDone = true;
    g_joinResult = info->ResultCode;
    if (info->LobbyId) strncpy(g_lobbyId, info->LobbyId, sizeof(g_lobbyId) - 1);
    printf("       join result=%d lobbyId=%s\n", (int)info->ResultCode, info->LobbyId ? info->LobbyId : "");
}

static void Pump(Api& api, EOS_HPlatform platform, int ms) {
    for (int i = 0; i < ms / 20; i++) { api.PlatformTick(platform); Sleep(20); }
}

static std::string PuidText(Api& api, EOS_ProductUserId id) {
    char buf[64] = {0};
    int32_t len = sizeof(buf);
    if (!id || api.PuidToString(id, buf, &len) != EOS_EResult::EOS_Success) return "<none>";
    return buf;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 3) { printf("usage: %s host|client <path-to-EOSSDK dll> [seconds]\n", argv[0]); return 2; }
    const std::string role = argv[1];
    const int seconds = (argc > 3) ? atoi(argv[3]) : 25;

    printf("=== ReFix EOS v3 interconnection test (%s) ===\n", role.c_str());
    g_dll = LoadLibraryA(argv[2]);
    if (!g_dll) { printf("[FAIL] LoadLibrary(%s) failed: %lu\n", argv[2], GetLastError()); return 1; }
    printf("[ OK ] loaded %s\n", argv[2]);

    Api api = LoadApi();
    if (g_failures) return 1;

    EOS_InitializeOptions initOpts{};
    initOpts.ApiVersion     = EOS_INITIALIZE_API_LATEST;
    initOpts.ProductName    = "ReFixSelfTest";
    initOpts.ProductVersion = "1.0";
    Check(api.Initialize(&initOpts) == EOS_EResult::EOS_Success, "EOS_Initialize");

    EOS_Platform_Options platOpts{};
    platOpts.ApiVersion   = EOS_PLATFORM_OPTIONS_API_LATEST;
    platOpts.ProductId    = "refix-selftest-product";
    platOpts.SandboxId    = "refix-selftest-sandbox";
    platOpts.DeploymentId = "refix-selftest-deployment";
    EOS_HPlatform platform = api.PlatformCreate(&platOpts);
    Check(platform != nullptr, "EOS_Platform_Create");
    if (!platform) return 1;

    EOS_HConnect connect = api.GetConnect(platform);
    EOS_HLobby   lobby   = api.GetLobby(platform);
    EOS_HP2P     p2p     = api.GetP2P(platform);
    Check(connect && lobby && p2p, "interface handles");

    EOS_Connect_Credentials creds{};
    creds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
    creds.Type       = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
    creds.Token      = nullptr;
    EOS_Connect_LoginOptions loginOpts{};
    loginOpts.ApiVersion  = EOS_CONNECT_LOGIN_API_LATEST;
    loginOpts.Credentials = &creds;
    api.ConnectLogin(connect, &loginOpts, nullptr, &OnLogin);
    Pump(api, platform, 500);
    Check(g_loginDone && g_localUser != nullptr, "EOS_Connect_Login produced a ProductUserId");
    printf("       local PUID = %s\n", PuidText(api, g_localUser).c_str());

    if (role == "host") {
        EOS_Lobby_CreateLobbyOptions createOpts{};
        createOpts.ApiVersion       = EOS_LOBBY_CREATELOBBY_API_LATEST;
        createOpts.LocalUserId      = g_localUser;
        createOpts.MaxLobbyMembers  = 4;
        createOpts.PermissionLevel  = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
        createOpts.bPresenceEnabled = EOS_FALSE;
        createOpts.bAllowInvites    = EOS_TRUE;
        createOpts.BucketId         = kBucket;
        createOpts.bEnableJoinById  = EOS_TRUE;
        api.CreateLobby(lobby, &createOpts, nullptr, &OnCreate);
        Pump(api, platform, 500);
        Check(g_createDone && g_lobbyId[0] != 0, "EOS_Lobby_CreateLobby");

        printf("       hosting; waiting %ds for a client...\n", seconds);
        uint32_t lastMembers = 0;
        bool sawJoin = false, sawPacket = false;
        for (int i = 0; i < seconds * 10; i++) {
            Pump(api, platform, 100);

            EOS_HLobbyDetails details = nullptr;
            EOS_Lobby_CopyLobbyDetailsHandleOptions copyOpts{};
            copyOpts.ApiVersion  = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
            copyOpts.LobbyId     = g_lobbyId;
            copyOpts.LocalUserId = g_localUser;
            if (api.CopyDetailsHandle(lobby, &copyOpts, &details) == EOS_EResult::EOS_Success && details) {
                EOS_LobbyDetails_GetMemberCountOptions mcOpts{};
                mcOpts.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
                uint32_t members = api.DetailsMemberCount(details, &mcOpts);
                if (members != lastMembers) {
                    lastMembers = members;
                    printf("       lobby now has %u member(s)\n", members);
                    for (uint32_t m = 0; m < members; m++) {
                        EOS_LobbyDetails_GetMemberByIndexOptions miOpts{};
                        miOpts.ApiVersion  = EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;
                        miOpts.MemberIndex = m;
                        printf("         member[%u] = %s\n", m,
                               PuidText(api, api.DetailsMemberByIndex(details, &miOpts)).c_str());
                    }
                    if (members >= 2) sawJoin = true;
                }
            }

            uint32_t size = 0;
            EOS_P2P_GetNextReceivedPacketSizeOptions szOpts{};
            szOpts.ApiVersion  = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
            szOpts.LocalUserId = g_localUser;
            if (api.P2PNextSize(p2p, &szOpts, &size) == EOS_EResult::EOS_Success && size) {
                char buf[1024] = {0};
                EOS_ProductUserId from = nullptr;
                EOS_P2P_SocketId sock{};
                uint8_t channel = 0;
                uint32_t written = 0;
                EOS_P2P_ReceivePacketOptions rxOpts{};
                rxOpts.ApiVersion       = EOS_P2P_RECEIVEPACKET_API_LATEST;
                rxOpts.LocalUserId      = g_localUser;
                rxOpts.MaxDataSizeBytes = sizeof(buf);
                if (api.P2PReceive(p2p, &rxOpts, &from, &sock, &channel, buf, &written) == EOS_EResult::EOS_Success) {
                    printf("       P2P packet from %s on '%s': %.*s\n",
                           PuidText(api, from).c_str(), sock.SocketName, (int)written, buf);
                    sawPacket = true;
                    EOS_P2P_AcceptConnectionOptions acc{};
                    acc.ApiVersion   = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
                    acc.LocalUserId  = g_localUser;
                    acc.RemoteUserId = from;
                    acc.SocketId     = &sock;
                    api.P2PAccept(p2p, &acc);

                    const char* reply = "pong-from-host";
                    EOS_P2P_SendPacketOptions tx{};
                    tx.ApiVersion      = EOS_P2P_SENDPACKET_API_LATEST;
                    tx.LocalUserId     = g_localUser;
                    tx.RemoteUserId    = from;
                    tx.SocketId        = &sock;
                    tx.Channel         = channel;
                    tx.DataLengthBytes = (uint32_t)strlen(reply);
                    tx.Data            = reply;
                    tx.bAllowDelayedDelivery = EOS_TRUE;
                    tx.Reliability     = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
                    api.P2PSend(p2p, &tx);
                }
            }
            if (sawJoin && sawPacket) break;
        }
        Check(sawJoin, "a remote player joined the hosted lobby");
        Check(sawPacket, "a P2P packet arrived from the remote player");
    } else {
        EOS_HLobbySearch search = nullptr;
        EOS_Lobby_CreateLobbySearchOptions searchOpts{};
        searchOpts.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
        searchOpts.MaxResults = 20;
        Check(api.CreateLobbySearch(lobby, &searchOpts, &search) == EOS_EResult::EOS_Success && search,
              "EOS_Lobby_CreateLobbySearch");

        EOS_Lobby_AttributeData bucketAttr{};
        bucketAttr.ApiVersion     = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        bucketAttr.Key            = EOS_LOBBY_SEARCH_BUCKET_ID;
        bucketAttr.ValueType      = EOS_ELobbyAttributeType::EOS_AT_STRING;
        bucketAttr.Value.AsUtf8   = kBucket;
        EOS_LobbySearch_SetParameterOptions paramOpts{};
        paramOpts.ApiVersion   = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
        paramOpts.Parameter    = &bucketAttr;
        paramOpts.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
        Check(api.SearchSetParameter(search, &paramOpts) == EOS_EResult::EOS_Success,
              "EOS_LobbySearch_SetParameter(bucket)");

        uint32_t found = 0;
        for (int attempt = 0; attempt < 8 && found == 0; attempt++) {
            g_searchDone = false;
            EOS_LobbySearch_FindOptions findOpts{};
            findOpts.ApiVersion  = EOS_LOBBYSEARCH_FIND_API_LATEST;
            findOpts.LocalUserId = g_localUser;
            api.SearchFind(search, &findOpts, nullptr, &OnFind);
            for (int i = 0; i < 40 && !g_searchDone; i++) Pump(api, platform, 50);
            EOS_LobbySearch_GetSearchResultCountOptions cntOpts{};
            cntOpts.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
            found = api.SearchCount(search, &cntOpts);
            printf("       attempt %d: %u lobby(ies) found\n", attempt + 1, found);
        }
        Check(found > 0, "the hosted lobby was discovered over the network");
        if (found == 0) { FreeLibrary(g_dll); return 1; }

        EOS_HLobbyDetails details = nullptr;
        EOS_LobbySearch_CopySearchResultByIndexOptions cpOpts{};
        cpOpts.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
        cpOpts.LobbyIndex = 0;
        Check(api.SearchCopy(search, &cpOpts, &details) == EOS_EResult::EOS_Success && details,
              "EOS_LobbySearch_CopySearchResultByIndex");

        EOS_LobbyDetails_Info* info = nullptr;
        EOS_LobbyDetails_CopyInfoOptions infoOpts{};
        infoOpts.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
        if (api.DetailsCopyInfo(details, &infoOpts, &info) == EOS_EResult::EOS_Success && info) {
            printf("       found lobby %s owner=%s bucket='%s' max=%u free=%u\n",
                   info->LobbyId, PuidText(api, info->LobbyOwnerUserId).c_str(),
                   info->BucketId ? info->BucketId : "", info->MaxMembers, info->AvailableSlots);
            Check(info->MaxMembers == 4, "MaxMembers round-tripped as configured (4, not a default)");
            Check(info->BucketId && strcmp(info->BucketId, kBucket) == 0,
                  "BucketId round-tripped as configured");
            api.DetailsInfoRelease(info);
        } else {
            Check(false, "EOS_LobbyDetails_CopyInfo");
        }

        EOS_ProductUserId owner = api.DetailsOwner(details, nullptr);
        Check(owner != nullptr && PuidText(api, owner) != PuidText(api, g_localUser),
              "the lobby owner is a *different* ProductUserId from the local player");

        EOS_Lobby_JoinLobbyOptions joinOpts{};
        joinOpts.ApiVersion         = EOS_LOBBY_JOINLOBBY_API_LATEST;
        joinOpts.LobbyDetailsHandle = details;
        joinOpts.LocalUserId        = g_localUser;
        joinOpts.bPresenceEnabled   = EOS_FALSE;
        api.JoinLobby(lobby, &joinOpts, nullptr, &OnJoin);
        for (int i = 0; i < 200 && !g_joinDone; i++) Pump(api, platform, 50);
        Check(g_joinDone && g_joinResult == EOS_EResult::EOS_Success, "EOS_Lobby_JoinLobby succeeded");

        // The joined lobby must now list both players, from the host's record.
        EOS_HLobbyDetails joined = nullptr;
        EOS_Lobby_CopyLobbyDetailsHandleOptions copyOpts{};
        copyOpts.ApiVersion  = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
        copyOpts.LobbyId     = g_lobbyId[0] ? g_lobbyId : nullptr;
        copyOpts.LocalUserId = g_localUser;
        uint32_t members = 0;
        if (copyOpts.LobbyId &&
            api.CopyDetailsHandle(lobby, &copyOpts, &joined) == EOS_EResult::EOS_Success && joined) {
            EOS_LobbyDetails_GetMemberCountOptions mcOpts{};
            mcOpts.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
            members = api.DetailsMemberCount(joined, &mcOpts);
        }
        printf("       member count after join: %u\n", members);

        EOS_P2P_SocketId sock{};
        sock.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strncpy(sock.SocketName, "refixtest", sizeof(sock.SocketName) - 1);
        const char* msg = "ping-from-client";
        EOS_P2P_SendPacketOptions tx{};
        tx.ApiVersion      = EOS_P2P_SENDPACKET_API_LATEST;
        tx.LocalUserId     = g_localUser;
        tx.RemoteUserId    = owner;
        tx.SocketId        = &sock;
        tx.Channel         = 0;
        tx.DataLengthBytes = (uint32_t)strlen(msg);
        tx.Data            = msg;
        tx.bAllowDelayedDelivery = EOS_TRUE;
        tx.Reliability     = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
        Check(api.P2PSend(p2p, &tx) == EOS_EResult::EOS_Success, "EOS_P2P_SendPacket to the host");

        bool gotReply = false;
        for (int i = 0; i < 100 && !gotReply; i++) {
            Pump(api, platform, 100);
            uint32_t size = 0;
            EOS_P2P_GetNextReceivedPacketSizeOptions szOpts{};
            szOpts.ApiVersion  = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
            szOpts.LocalUserId = g_localUser;
            if (api.P2PNextSize(p2p, &szOpts, &size) != EOS_EResult::EOS_Success || !size) continue;
            char buf[1024] = {0};
            EOS_ProductUserId from = nullptr;
            EOS_P2P_SocketId rxSock{};
            uint8_t channel = 0;
            uint32_t written = 0;
            EOS_P2P_ReceivePacketOptions rxOpts{};
            rxOpts.ApiVersion       = EOS_P2P_RECEIVEPACKET_API_LATEST;
            rxOpts.LocalUserId      = g_localUser;
            rxOpts.MaxDataSizeBytes = sizeof(buf);
            if (api.P2PReceive(p2p, &rxOpts, &from, &rxSock, &channel, buf, &written) == EOS_EResult::EOS_Success) {
                printf("       reply from %s: %.*s\n", PuidText(api, from).c_str(), (int)written, buf);
                gotReply = true;
            }
        }
        Check(gotReply, "the host's P2P reply came back");
    }

    printf("=== %s finished with %d failure(s) ===\n", role.c_str(), g_failures);
    FreeLibrary(g_dll);
    return g_failures == 0 ? 0 : 1;
}
