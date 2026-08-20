# Re:Photon — Binary Protocol & Wire Specification

## 1. Overview

Photon Realtime communication is layered on top of reliable UDP (based on ENet command framings), TCP, or WebSockets. The application payload uses **Photon Protocol 1.6 / 1.8 (GpBinary)** serialization.

---

## 2. Photon Data Types (GpType)

| Type ID (Hex / Dec) | Type Name | Wire Encoding & Description |
| :--- | :--- | :--- |
| `0x00` (`0`) | `Unknown` / `Null` | Nil or unassigned value. |
| `0x61` (`97` / `'a'`) | `Array` (Typed) | `[1-byte typeCode][2-byte length][elements...]` |
| `0x62` (`98` / `'b'`) | `Byte` | Single unsigned 8-bit integer (`uint8_t`). |
| `0x63` (`99` / `'c'`) | `Custom` | `[1-byte customType][2-byte length][custom data bytes...]` |
| `0x64` (`100` / `'d'`) | `Dictionary` | `[1-byte keyType][1-byte valType][2-byte length][key-value pairs...]` |
| `0x65` (`101` / `'e'`) | `EventData` | Encapsulated Photon event object. |
| `0x66` (`102` / `'f'`) | `Float` | 32-bit IEEE-754 floating point number (Big-Endian). |
| `0x68` (`104` / `'h'`) | `Hashtable` | `[2-byte length][keyType][key][valType][val]...` |
| `0x69` (`105` / `'i'`) | `Integer` | 32-bit signed integer (Big-Endian). |
| `0x6B` (`107` / `'k'`) | `Short` | 16-bit signed integer (Big-Endian). |
| `0x6C` (`108` / `'l'`) | `Long` | 64-bit signed integer (Big-Endian). |
| `0x6F` (`111` / `'o'`) | `Boolean` | Single byte boolean (`0x00` = false, `0x01` = true). |
| `0x70` (`112` / `'p'`) | `OperationResponse` | Encapsulated operation response object. |
| `0x71` (`113` / `'q'`) | `OperationRequest` | Encapsulated operation request object. |
| `0x73` (`115` / `'s'`) | `String` | `[2-byte length][UTF-8 bytes]` |
| `0x78` (`120` / `'x'`) | `ByteArray` | `[4-byte length][raw bytes]` |
| `0x79` (`121` / `'y'`) | `Array` (Object) | `[2-byte length][typeCode][element]...` (Generic array) |
| `0x7A` (`122` / `'z'`) | `ObjectArray` | Array of heterogeneous GpType objects. |

---

## 3. Core Operation Codes (OpCode)

| OpCode (Dec) | Operation Name | Description |
| :--- | :--- | :--- |
| `230` (`0xE6`) | `OpAuthenticate` | Client authentication with AppID, ClientVersion, Token / Region. |
| `231` (`0xE7`) | `OpAuthenticateOnce` | Single-step ticket-based authentication. |
| `229` (`0xE5`) | `OpJoinLobby` | Connects client to default or named matchmaking lobby. |
| `228` (`0xE4`) | `OpLeaveLobby` | Leaves the current matchmaking lobby. |
| `227` (`0xE3`) | `OpCreateGame` | Creates a new game room with custom options & properties. |
| `226` (`0xE2`) | `OpJoinGame` | Joins an existing game room by name. |
| `225` (`0xE1`) | `OpJoinRandomGame` | Matchmaking: Joins a random available room matching filters. |
| `254` (`0xFE`) | `OpLeave` | Leaves the active game room. |
| `253` (`0xFD`) | `OpRaiseEvent` | Relays an in-game event to all / specific actors in the room. |
| `252` (`0xFC`) | `OpSetProperties` | Sets room or player custom properties. |
| `251` (`0xFB`) | `OpGetProperties` | Queries room or player custom properties. |
| `248` (`0xF8`) | `OpChangeGroups` | Subscribes / unsubscribes to event interest groups. |

---

## 4. Parameter Codes (ParameterCode)

| Code (Dec) | Constant Name | Description |
| :--- | :--- | :--- |
| `255` | `GameId` | Unique room/session identifier string. |
| `254` | `ActorNr` | Assigned actor number for the player in the room. |
| `253` | `TargetActorNr` | Specific target actor for directed messages. |
| `252` | `ActorList` | Array of integer actor numbers present in room. |
| `251` | `Properties` | Hashtable of custom room or player properties. |
| `250` | `Broadcast` | Boolean flag indicating broadcast to all peers. |
| `249` | `ActorProperties` | Hashtable of properties keyed by actor number. |
| `248` | `GameProperties` | Hashtable of room-wide custom properties. |
| `247` | `Cache` | Event caching slice options (DoNotCache, SlicePurgeIndex, etc.). |
| `246` | `ReceiverGroup` | Target group (Others = 0, All = 1, MasterClient = 2). |
| `245` | `Data` | Main payload of custom event (`OpRaiseEvent`). |
| `244` | `Code` | Custom event code (`byte`). |
| `230` | `Address` | GameServer IP:Port endpoint returned on room creation/join. |
| `225` | `AppVersion` | Application version string. |
| `224` | `AppId` | Photon Application GUID. |
| `220` | `ApplicationId` | Application identifier for NameServer dispatch. |

---

## 5. Event Codes (EventCode)

| EventCode (Dec) | Constant Name | Description |
| :--- | :--- | :--- |
| `255` | `EventJoin` | Dispatched when a new actor enters the room. |
| `254` | `EventLeave` | Dispatched when an actor disconnects or leaves. |
| `253` | `EventPropertiesChanged` | Dispatched when custom room or player properties are updated. |
| `252` | `EventSetProperties` | Internal property update notification. |
| `230` | `EventCacheSliceChanged` | Notification that cache slice has changed. |
| `1..199` | `UserCustomEvents` | Application-specific game events, RPCs, and object sync. |
