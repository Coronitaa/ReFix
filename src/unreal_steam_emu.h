#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <string>

class CCallbackBase;

// Public API of the Re:Goldberg Unreal Steam Emulation Layer
namespace UnrealSteamEmu {

    // Lifecycle
    bool Initialize();
    inline bool Init() { return Initialize(); }
    bool InitFlat(char* pOutErrMsg);
    int InitInternal(const char* pszInternalCheckInterfaceVersions, char* pOutErrMsg);
    void Shutdown();
    bool IsInitialized();

    // GameServer Lifecycle
    bool GameServer_Init(uint32_t unIP, uint16_t usGamePort, uint16_t usQueryPort, int eServerMode, const char* pchVersionString);
    bool GameServer_InitSafe();

    // Callbacks & CallResults
    void RunCallbacks();
    void GameServer_RunCallbacks();
    void RegisterCallback(class CCallbackBase* pCallback, int iCallback);
    void UnregisterCallback(class CCallbackBase* pCallback);
    void RegisterCallResult(class CCallbackBase* pCallback, uint64_t hAPICall);
    void UnregisterCallResult(class CCallbackBase* pCallback, uint64_t hAPICall);

    uint64_t PostCallResult(int iCallback, const void* pData, size_t dataSize, double delaySeconds = 0.0);
    void PostCallback(int iCallback, const void* pData, size_t dataSize, double delaySeconds = 0.0);

    bool IsAPICallCompleted(uint64_t hAPICall, bool* pbFailed);
    bool GetAPICallResult(uint64_t hAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed);
    int GetAPICallFailureReason(uint64_t hAPICall);

    // Manual Dispatch
    void ManualDispatch_Init();
    void ManualDispatch_RunFrame(int32_t hSteamPipe);
    bool ManualDispatch_GetNextCallback(int32_t hSteamPipe, void* pCallbackMsg);
    void ManualDispatch_FreeLastCallback(int32_t hSteamPipe);
    bool ManualDispatch_GetAPICallResult(int32_t hSteamPipe, uint64_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed);

    // Interface Factory & Accessors
    void* GetGenericInterface(const char* pchVersion);
    void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion);
    void* FindOrCreateGameServerInterface(int32_t hUser, const char* pszVersion);
    void* CreateInterface(const char* ver);
    void* ContextInit(void* pContextInitData);

    int32_t GetHSteamPipe();
    int32_t GetHSteamUser();
    int32_t GameServer_GetHSteamPipe();
    int32_t GameServer_GetHSteamUser();

    uint64_t GetLocalSteamID();
    const char* GetPersonaName();
    const char* GetFriendPersonaName(uint64_t steamIDFriend);
    uint32_t GetAppID();

    // Direct interface pointers
    void* GetSteamClient();
    void* GetSteamUser();
    void* GetSteamFriends();
    void* GetSteamUtils();
    void* GetSteamMatchmaking();
    void* GetSteamMatchmakingServers();
    void* GetSteamUserStats();
    void* GetSteamApps();
    void* GetSteamNetworking();
    void* GetSteamNetworkingSockets();
    void* GetSteamNetworkingUtils();
    void* GetSteamRemoteStorage();
    void* GetSteamUGC();
    void* GetSteamGameServer();
    void* GetSteamGameServerStats();
    void* GetSteamGameServerNetworking();
    void* GetSteamHTTP();
    void* GetSteamInput();
    void* GetSteamInventory();
    void* GetSteamScreenshots();
    void* GetSteamTimeline();
    void* GetSteamNetworkingMessages();
    void* GetSteamGameSearch();
    void* GetSteamAppList();
    void* GetSteamMusic();
    void* GetSteamMusicRemote();
    void* GetSteamHTMLSurface();
    void* GetSteamVideo();
    void* GetSteamParentalSettings();
    void* GetSteamParties();
    void* GetSteamRemotePlay();

    // Aliases with ISteam prefix
    inline void* GetISteamClient() { return GetSteamClient(); }
    inline void* GetISteamUser() { return GetSteamUser(); }
    inline void* GetISteamFriends() { return GetSteamFriends(); }
    inline void* GetISteamUtils() { return GetSteamUtils(); }
    inline void* GetISteamMatchmaking() { return GetSteamMatchmaking(); }
    inline void* GetISteamMatchmakingServers() { return GetSteamMatchmakingServers(); }
    inline void* GetISteamUserStats() { return GetSteamUserStats(); }
    inline void* GetISteamApps() { return GetSteamApps(); }
    inline void* GetISteamNetworking() { return GetSteamNetworking(); }
    inline void* GetISteamNetworkingSockets() { return GetSteamNetworkingSockets(); }
    inline void* GetISteamNetworkingUtils() { return GetSteamNetworkingUtils(); }
    inline void* GetISteamNetworkingMessages() { return GetSteamNetworkingMessages(); }
    inline void* GetISteamRemoteStorage() { return GetSteamRemoteStorage(); }
    inline void* GetISteamUGC() { return GetSteamUGC(); }
    inline void* GetISteamGameServer() { return GetSteamGameServer(); }
    inline void* GetISteamGameServerStats() { return GetSteamGameServerStats(); }
    inline void* GetISteamGameServerNetworking() { return GetSteamGameServerNetworking(); }
    inline void* GetISteamGameServerNetworkingMessages() { return GetSteamNetworkingMessages(); }
    inline void* GetISteamGameServerUtils() { return GetSteamUtils(); }
    inline void* GetISteamHTTP() { return GetSteamHTTP(); }
    inline void* GetISteamInput() { return GetSteamInput(); }
    inline void* GetISteamInventory() { return GetSteamInventory(); }
    inline void* GetISteamScreenshots() { return GetSteamScreenshots(); }
    inline void* GetISteamTimeline() { return GetSteamTimeline(); }
    inline void* GetISteamGameSearch() { return GetSteamGameSearch(); }
    inline void* GetISteamAppList() { return GetSteamAppList(); }
    inline void* GetISteamMusic() { return GetSteamMusic(); }
    inline void* GetISteamMusicRemote() { return GetSteamMusicRemote(); }
    inline void* GetISteamHTMLSurface() { return GetSteamHTMLSurface(); }
    inline void* GetISteamVideo() { return GetSteamVideo(); }
    inline void* GetISteamParentalSettings() { return GetSteamParentalSettings(); }
    inline void* GetISteamParties() { return GetSteamParties(); }
    inline void* GetISteamRemotePlay() { return GetSteamRemotePlay(); }

    // Auth ticket helpers
    uint32_t GetAuthSessionTicket(void* pTicket, int cbMaxTicket, uint32_t* pcbTicket, const void* pSteamNetworkingIdentity = nullptr);
    uint32_t GetAuthTicketForWebApi(const char* pchIdentity);

    // RemoteStorage helpers
    bool RemoteStorage_FileWrite(const char* pchFile, const void* pvData, int32_t cubData);
    int32_t RemoteStorage_FileRead(const char* pchFile, void* pvData, int32_t cubDataToRead);
    bool RemoteStorage_FileExists(const char* pchFile);
    int32_t RemoteStorage_GetFileSize(const char* pchFile);
    bool RemoteStorage_FileDelete(const char* pchFile);

    // Matchmaking helpers
    uint64_t Matchmaking_CreateLobby(int eLobbyType, int cMaxMembers);
    uint64_t Matchmaking_JoinLobby(uint64_t steamIDLobby);
    void Matchmaking_LeaveLobby(uint64_t steamIDLobby);
    bool Matchmaking_SetLobbyData(uint64_t steamIDLobby, const char* pchKey, const char* pchValue);
    const char* Matchmaking_GetLobbyData(uint64_t steamIDLobby, const char* pchKey);
    int Matchmaking_GetLobbyDataCount(uint64_t steamIDLobby);
    bool Matchmaking_GetLobbyDataByIndex(uint64_t steamIDLobby, int iLobbyData, char* pchKey, int cchKeyBufferSize, char* pchValue, int cchValueBufferSize);
    uint64_t Matchmaking_RequestLobbyList();
    void Matchmaking_AddRequestLobbyListStringFilter(const char* pchKeyToMatch, const char* pchValueToMatch, int eComparisonType);
    void Matchmaking_AddRequestLobbyListNumericalFilter(const char* pchKeyToMatch, int nValueToMatch, int eComparisonType);
    void Matchmaking_AddRequestLobbyListNearValueFilter(const char* pchKeyToMatch, int nValueToBeCloseTo);
    void Matchmaking_AddRequestLobbyListFilterSlotsAvailable(int nSlotsAvailable);
    void Matchmaking_AddRequestLobbyListDistanceFilter(int eLobbyDistanceFilter);
    void Matchmaking_AddRequestLobbyListResultCountFilter(int cMaxResults);
    void Matchmaking_AddRequestLobbyListCompatibleMembersFilter(uint64_t steamIDLobby);
    uint64_t Matchmaking_GetLobbyByIndex(int iLobby);
    int Matchmaking_GetNumLobbyMembers(uint64_t steamIDLobby);
    uint64_t Matchmaking_GetLobbyMemberByIndex(uint64_t steamIDLobby, int iMember);
    uint64_t Matchmaking_GetLobbyOwner(uint64_t steamIDLobby);
    bool Matchmaking_SetLobbyOwner(uint64_t steamIDLobby, uint64_t steamIDNewOwner);
    bool Matchmaking_SetLobbyJoinable(uint64_t steamIDLobby, bool bLobbyJoinable);
    const char* Matchmaking_GetLobbyMemberData(uint64_t steamIDLobby, uint64_t steamIDUser, const char* pchKey);
    void Matchmaking_SetLobbyMemberData(uint64_t steamIDLobby, const char* pchKey, const char* pchValue);
    bool Matchmaking_InviteUserToLobby(uint64_t steamIDLobby, uint64_t steamIDInvitee);
    bool Matchmaking_SetLobbyMemberLimit(uint64_t steamIDLobby, int cMaxMembers);
    int Matchmaking_GetLobbyMemberLimit(uint64_t steamIDLobby);
    bool Matchmaking_SetLobbyType(uint64_t steamIDLobby, int eLobbyType);
    bool Matchmaking_SetLinkedLobby(uint64_t steamIDLobby, uint64_t steamIDLobbyDependent);
    bool Matchmaking_DeleteLobbyData(uint64_t steamIDLobby, const char* pchKey);
    bool Matchmaking_SendLobbyChatMsg(uint64_t steamIDLobby, const void* pvMsgBody, int cubMsgBody);
    int Matchmaking_GetLobbyChatEntry(uint64_t steamIDLobby, int iChatID, uint64_t* pSteamIDUser, void* pvData, int cubData, int* peChatEntryType);
    bool Matchmaking_RequestLobbyData(uint64_t steamIDLobby);
    void Matchmaking_SetLobbyGameServer(uint64_t steamIDLobby, uint32_t unGameServerIP, uint16_t unGameServerPort, uint64_t steamIDGameServer);
    bool Matchmaking_GetLobbyGameServer(uint64_t steamIDLobby, uint32_t* punGameServerIP, uint16_t* punGameServerPort, uint64_t* psteamIDGameServer);

    // Networking helpers
    bool Networking_SendP2PPacket(uint64_t steamIDRemote, const void* pubData, uint32_t cubData, int eP2PSendType, int nChannel);
    bool Networking_IsP2PPacketAvailable(uint32_t* pcubMsgSize, int nChannel);
    bool Networking_ReadP2PPacket(void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize, uint64_t* psteamIDRemote, int nChannel);
    bool Networking_AcceptP2PSessionWithUser(uint64_t steamIDRemote);
    bool Networking_CloseP2PSessionWithUser(uint64_t steamIDRemote);
    int Matchmaking_GetFavoriteGameCount();
    bool Matchmaking_GetFavoriteGame(int iGame, uint32_t* pnAppID, uint32_t* pnIP, uint16_t* pnConnPort, uint16_t* pnQueryPort, uint32_t* punFlags, uint32_t* pRTime32LastPlayedOnServer);
    int Matchmaking_AddFavoriteGame(uint32_t nAppID, uint32_t nIP, uint16_t nConnPort, uint16_t nQueryPort, uint32_t unFlags, uint32_t rTime32);
    bool Matchmaking_RemoveFavoriteGame(uint32_t nAppID, uint32_t nIP, uint16_t nConnPort, uint16_t nQueryPort, uint32_t unFlags);

    // Lobby sync notification with EOS
    void NotifyEOSLobby(uint64_t lobbyID);

    // Network isolation runtime verification
    uint64_t GetBlockedEgressCount();
    void ResetBlockedEgressCount();
    void SimulatePeerEndpoint(uint64_t steamID, const char* ipStr, uint16_t port);
    void SendTestLanPacket(uint64_t remoteID, uint8_t msgType, const void* payload, size_t payloadLen, int dir);

    // Call tracking to certify Online / LAN isolation
    uint64_t GetCallCount();
    void ResetCallCount();
}

