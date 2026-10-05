# Locking Order Audit - Phase 3.6.1

## Potential Inversion Identified

During Phase 3.6, a potential lock inversion was detected between `g_emuMutex` and `g_socketsMutex`. Using `std::recursive_mutex` solved self-deadlocks but does not prevent deadlocks across multiple threads.

**Thread A: Network Polling (`RunCallbacks`)**
1. Locks `g_emuMutex` directly.
2. Calls `PollNetwork()`.
3. `PollNetwork` processes incoming P2P packets.
4. If a handshake packet is received, it calls `std::lock_guard<std::recursive_mutex> lock(g_socketsMutex);`.
**Order:** `g_emuMutex -> g_socketsMutex`

**Thread B: SteamNetworkingSockets API (e.g., `AcceptConnection`, `SendMessageToConnection`)**
1. Locks `g_socketsMutex` directly.
2. Performs connection state modifications.
3. Calls `EnqueueSocketCallback` or `SendLanPacket`.
4. These functions then lock `g_emuMutex` (to queue the callback or read the peer IP).
**Order:** `g_socketsMutex -> g_emuMutex`

This represents a classic `A->B`, `B->A` lock inversion that can lead to deadlocks if both threads preempt each other at the precise middle step.

## Resolution

The canonical locking order must always be:
1. `g_socketsMutex`
2. `g_emuMutex`

To achieve this, `RunCallbacks` and `GameServer_RunCallbacks` must **NOT** hold `g_emuMutex` while calling `PollNetwork()`. `PollNetwork()` safely handles its own locking as necessary.

By calling `PollNetwork()` outside the `g_emuMutex` critical section in `RunCallbacks()`, we ensure that thread A only locks `g_socketsMutex` first (inside `PollNetwork`), and then `g_emuMutex` (inside `EnqueueSocketCallback` or `SendLanPacket`), adhering to the strict `g_socketsMutex -> g_emuMutex` order.
