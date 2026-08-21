# ReFix EOS Online v2 — Runtime Tracing & Instrumentation Specification

## 1. Tracing Objective

To eliminate guesswork regarding RedpointEOS execution, a non-intrusive, thread-safe tracing layer will be embedded within the EOS proxy.

The tracing layer captures:
1. Exact sequence of EOS SDK functions invoked.
2. Argument structures (unpacked struct versions and key parameters).
3. Thread ID and high-resolution timestamp.
4. Asynchronous callback registration, queuing, and completion execution.
5. Return values (`EOS_EResult` / Handles).

---

## 2. Redaction & Safety Rules

> [!CAUTION]
> Under no circumstances may the logger output:
> - Cleartext auth tokens (Steam Session Tickets, OAuth tokens, Epic ID tokens).
> - Passwords, private keys, or secret URLs.
> Sensitive strings must be hashed or replaced with `[REDACTED:len=N]`.

---

## 3. Configuration & Format

Tracing is activated via `ReFix.ini`:
```ini
[EOS]
DebugLogging=true
TraceCallbacks=true
TraceMemory=false
```

### Output Schema:
```text
[HH:MM:SS.mmm] [TID:0x1A4C] [EOS:CALL] EOS_Lobby_CreateLobby (Options={ApiVersion=1, LocalUserId=puid_8f1b..., MaxMembers=6, BucketId="Battle_FFA"}) -> Result=EOS_Success
[HH:MM:SS.mmm] [TID:0x1A4C] [EOS:CB_QUEUE] Queued CB_Lobby_CreateLobby (Delegate=0x7FF74A12B000, ClientData=0x000001B2)
[HH:MM:SS.mmm] [TID:0x0984] [EOS:TICK] Dispatching 1 queued callbacks...
[HH:MM:SS.mmm] [TID:0x0984] [EOS:CB_EXEC] Executing CB_Lobby_CreateLobby -> Result=EOS_Success, LobbyId="eos_lob_7a9c1e2f"
```
