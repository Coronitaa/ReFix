# ReFix Online v2 — Binary Protocol Specification

## 1. Packet Framing & Header Format

All communication across the ReFix Online v2 wire uses a binary packet format. All multi-byte integers are encoded in Little-Endian format.

### 1.1. Packet Header (`RefixPacketHeader`)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Magic (0x52464958)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Version (0x0001)       |      MessageType (uint16)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                       RequestId (uint64)                      +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     PayloadLength (uint32)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Data...                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Type | Size | Description |
| :--- | :---: | :---: | :--- |
| `Magic` | `uint32_t` | 4 bytes | Protocol identifier: `0x52464958` ('RFIX'). |
| `Version` | `uint16_t` | 2 bytes | Protocol version (current: `1`). |
| `MessageType` | `uint16_t` | 2 bytes | Operation or notification identifier. |
| `RequestId` | `uint64_t` | 8 bytes | Monotonic client request identifier for async response correlation. |
| `PayloadLength` | `uint32_t` | 4 bytes | Length of following payload in bytes (Max: 65,536 bytes). |

---

## 2. Message Types & Error Codes

### 2.1. Message Types (`EMessageType`)
- `MSG_HELLO` (1), `MSG_HELLO_ACK` (2)
- `MSG_AUTH` (3), `MSG_AUTH_RESULT` (4)
- `MSG_CREATE_LOBBY` (5), `MSG_CREATE_LOBBY_RESULT` (6)
- `MSG_FIND_LOBBIES` (7), `MSG_FIND_LOBBIES_RESULT` (8)
- `MSG_JOIN_LOBBY` (9), `MSG_JOIN_LOBBY_RESULT` (10)
- `MSG_LEAVE_LOBBY` (11), `MSG_LEAVE_LOBBY_RESULT` (12)
- `MSG_LOBBY_UPDATE` (13)
- `MSG_MEMBER_JOINED` (14), `MSG_MEMBER_LEFT` (15)
- `MSG_HEARTBEAT` (16), `MSG_HEARTBEAT_ACK` (17)
- `MSG_RESYNC_LOBBY` (18), `MSG_RESYNC_LOBBY_RESULT` (19)
- `MSG_DESTROY_LOBBY` (20), `MSG_DESTROY_LOBBY_RESULT` (21)
- `MSG_ERROR` (99)

### 2.2. Backend Result Codes (`EBackendResult`)
- `SUCCESS = 0`
- `INVALID_PACKET = 1`
- `UNSUPPORTED_VERSION = 2`
- `NOT_AUTHENTICATED = 3`
- `INVALID_USER = 4`
- `LOBBY_NOT_FOUND = 5`
- `LOBBY_FULL = 6`
- `ALREADY_MEMBER = 7`
- `NOT_MEMBER = 8`
- `NOT_OWNER = 9`
- `INVALID_ATTRIBUTE = 10`
- `REQUEST_TIMEOUT = 11`
- `SERVER_ERROR = 12`

---

## 3. Security Limits & Input Validation

| Parameter | Maximum Limit | Enforcement Action |
| :--- | :--- | :--- |
| `MAX_PACKET_SIZE` | 65,536 bytes (64 KB) | Deserializer immediately drops packet. |
| `MAX_DISPLAY_NAME_LEN` | 64 characters | Truncated / rejected with `INVALID_PACKET`. |
| `MAX_LOBBY_ATTRIBUTES` | 64 key-value pairs | Rejected with `INVALID_ATTRIBUTE`. |
| `MAX_ATTRIBUTE_KEY_LEN`| 64 characters | Rejected with `INVALID_ATTRIBUTE`. |
| `MAX_ATTRIBUTE_VAL_LEN`| 1,024 characters | Rejected with `INVALID_ATTRIBUTE`. |
| `MAX_LOBBY_MEMBERS` | 64 players | Clamped / rejected with `LOBBY_FULL`. |
| `MAX_LOBBY_ID_LEN` | 64 characters | Rejected with `LOBBY_NOT_FOUND`. |
