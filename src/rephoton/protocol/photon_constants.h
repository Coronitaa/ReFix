#pragma once

#include <cstdint>

namespace ReFix::Photon::Protocol {

    // Photon GpType (Serialization Data Types)
    namespace GpType {
        constexpr uint8_t Null               = 0x00; // 0
        constexpr uint8_t StringArray        = 0x61; // 'a' (97) - String Array
        constexpr uint8_t Byte               = 0x62; // 'b' (98) - 8-bit unsigned
        constexpr uint8_t Custom             = 0x63; // 'c' (99) - Custom Type
        constexpr uint8_t Dictionary         = 0x64; // 'd' (100) - Typed Dictionary
        constexpr uint8_t EventData          = 0x65; // 'e' (101) - Event Data Object
        constexpr uint8_t Float              = 0x66; // 'f' (102) - 32-bit Float
        constexpr uint8_t Hashtable          = 0x68; // 'h' (104) - Heterogeneous Key-Value Table
        constexpr uint8_t Integer            = 0x69; // 'i' (105) - 32-bit Signed Integer
        constexpr uint8_t Short              = 0x6B; // 'k' (107) - 16-bit Signed Integer
        constexpr uint8_t Long               = 0x6C; // 'l' (108) - 64-bit Signed Integer
        constexpr uint8_t IntegerArray       = 0x6E; // 'n' (110) - 32-bit Integer Array
        constexpr uint8_t Boolean            = 0x6F; // 'o' (111) - 1-byte Boolean
        constexpr uint8_t OperationResponse  = 0x70; // 'p' (112) - Response Object
        constexpr uint8_t OperationRequest   = 0x71; // 'q' (113) - Request Object
        constexpr uint8_t String             = 0x73; // 's' (115) - UTF-8 String
        constexpr uint8_t ByteArray          = 0x78; // 'x' (120) - Raw Byte Array
        constexpr uint8_t Array              = 0x79; // 'y' (121) - Generic Typed Array
        constexpr uint8_t GenericArray       = 0x79; // 'y' (121) - Generic Object Array
        constexpr uint8_t ObjectArray        = 0x7A; // 'z' (122) - Object Array
    }

    // Photon Core Operation Codes (OpCode)
    namespace OpCode {
        constexpr uint8_t Authenticate       = 230; // 0xE6
        constexpr uint8_t AuthenticateOnce   = 231; // 0xE7
        constexpr uint8_t JoinLobby          = 229; // 0xE5
        constexpr uint8_t LeaveLobby         = 228; // 0xE4
        constexpr uint8_t CreateGame         = 227; // 0xE3
        constexpr uint8_t JoinGame           = 226; // 0xE2
        constexpr uint8_t JoinRandomGame     = 225; // 0xE1
        constexpr uint8_t Leave              = 254; // 0xFE
        constexpr uint8_t RaiseEvent         = 253; // 0xFD
        constexpr uint8_t SetProperties      = 252; // 0xFC
        constexpr uint8_t GetProperties      = 251; // 0xFB
        constexpr uint8_t ChangeGroups       = 248; // 0xF8
    }

    // Photon Parameter Codes (ParameterCode)
    namespace ParameterCode {
        constexpr uint8_t GameId             = 255;
        constexpr uint8_t ActorNr            = 254;
        constexpr uint8_t TargetActorNr      = 253;
        constexpr uint8_t ActorList          = 252;
        constexpr uint8_t Properties         = 251;
        constexpr uint8_t Broadcast          = 250;
        constexpr uint8_t ActorProperties    = 249;
        constexpr uint8_t PlayerProperties   = 249;
        constexpr uint8_t GameProperties     = 248;
        constexpr uint8_t Cache              = 247;
        constexpr uint8_t ReceiverGroup      = 246;
        constexpr uint8_t Data               = 245;
        constexpr uint8_t Code               = 244;
        constexpr uint8_t MasterClientId     = 203;
        constexpr uint8_t CustomInitData     = 229;
        constexpr uint8_t EncryptionMode     = 228;
        constexpr uint8_t EncryptionData     = 227;
        constexpr uint8_t AppVersion         = 225;
        constexpr uint8_t AppId              = 224;
        constexpr uint8_t UserId             = 225;
        constexpr uint8_t Address            = 230;
        constexpr uint8_t ClientAuthenticationType = 217;
        constexpr uint8_t ClientAuthenticationParams = 216;
        constexpr uint8_t ClientAuthenticationData = 214;
        constexpr uint8_t RoomName           = 255;
        constexpr uint8_t MaxPlayers         = 255;
        constexpr uint8_t IsOpen             = 253;
        constexpr uint8_t IsVisible          = 254;
        constexpr uint8_t EmptyRoomTtl       = 245;
        constexpr uint8_t PlayerTtl          = 246;
    }

    // Photon Event Codes (EventCode)
    namespace EventCode {
        constexpr uint8_t Join               = 255; // Player entered room
        constexpr uint8_t Leave              = 254; // Player left room
        constexpr uint8_t PropertiesChanged  = 253; // Custom properties updated
        constexpr uint8_t SetProperties      = 252;
        constexpr uint8_t AppInfo            = 251;
        constexpr uint8_t CacheSliceChanged  = 230;
        constexpr uint8_t ErrorInfo          = 224;
        constexpr uint8_t AuthEvent          = 223;
    }

    // Photon Error Return Codes
    namespace ErrorCode {
        constexpr short Ok                              = 0;
        constexpr short OperationNotAllowedInCurrentState = -3;
        constexpr short InvalidOperationCode            = -2;
        constexpr short InternalServerError             = -1;
        constexpr short InvalidAuthentication           = 32767;
        constexpr short GameIdAlreadyExists             = 32766;
        constexpr short GameFull                        = 32765;
        constexpr short GameClosed                      = 32764;
        constexpr short AlreadyMatched                  = 32763;
        constexpr short ServerFull                      = 32762;
        constexpr short UserBlocked                     = 32761;
        constexpr short NoRandomMatchFound              = 32760;
        constexpr short GameDoesNotExist                = 32758;
        constexpr short MaxCcuReached                   = 32757;
        constexpr short InvalidRegion                   = 32756;
        constexpr short CustomAuthenticationFailed      = 32755;
    }

    // Receiver Groups for OpRaiseEvent
    namespace ReceiverGroup {
        constexpr uint8_t Others        = 0;
        constexpr uint8_t All           = 1;
        constexpr uint8_t MasterClient  = 2;
    }

    // Event Caching Options
    namespace EventCaching {
        constexpr uint8_t DoNotCache                    = 0;
        constexpr uint8_t MergeCache                    = 1;
        constexpr uint8_t ReplaceCache                  = 2;
        constexpr uint8_t RemoveCache                   = 3;
        constexpr uint8_t AddToRoomCache                = 4;
        constexpr uint8_t AddToRoomCacheGlobal          = 5;
        constexpr uint8_t RemoveFromRoomCache           = 6;
        constexpr uint8_t RemoveFromRoomCacheForActorsLeft = 7;
        constexpr uint8_t SliceIncreaseIndex            = 10;
        constexpr uint8_t SliceSetIndex                 = 11;
        constexpr uint8_t SlicePurgeIndex               = 12;
        constexpr uint8_t SlicePurgeUpToIndex           = 13;
    }

} // namespace ReFix::Photon::Protocol
