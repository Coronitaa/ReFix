# CS:GO Protocol Buffers

Source: https://github.com/SteamDatabase/Protobufs/tree/master/csgo
Commit hash: 5414eb56e4a033fefbe667f1988af5297864a784
Date: 2026

## Protocol Truth
Official protobuf definitions for Counter-Strike: Global Offensive game coordinator messages.
Key files:
- `cstrike15_gcmessages.proto`
- `gcsdk_gcmessages.proto`
- `base_gcmessages.proto`
- `econ_gcmessages.proto`
- `gcsystemmsgs.proto`

All message and field IDs used in ReFix GC emulation must match these definitions.
Legacy non-protobuf messages (e.g. k_EMsgGCAdjustItemEquippedState = 1059, k_EMsgGCUnlockCrate = 1007) are validated against reference implementation https://github.com/mikkokko/csgo_gc.
