# Re:Photon — Testing & Verification Strategy

## 1. Test Architecture

The Re:Photon test suite consists of three distinct testing levels:

```
+-------------------------------------------------------------------------+
| Level 1: Unit Tests (test_main.cpp)                                     |
| - GpType binary serialization / deserialization round-trips             |
| - Operation encoding (Authenticate, CreateGame, JoinGame, RaiseEvent)   |
| - Parameter mapping & Hashtable integrity                               |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Level 2: Multi-Client Integration Harness (test_harness.cpp)            |
| - Simulated Client A (Host) creates Room "Alpha"                        |
| - Simulated Client B (Joiner) joins Room "Alpha"                        |
| - Actor Number validation (Actor 1 = Host, Actor 2 = Joiner)            |
| - In-room Custom Property broadcast & verification                      |
| - Reliable Event Relay (OpRaiseEvent 100 -> EventCode 100 received)     |
| - Graceful Leave (OpLeave -> EventLeave broadcast -> Room closed)       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Level 3: Real Game Live Verification (R.E.P.O. / Phasmophobia)          |
| - 2 active game client instances executing on ReFix                     |
| - Lobby creation, discovery, in-game spawn, and gameplay sync           |
+-------------------------------------------------------------------------+
```

---

## 2. Running Automated Tests

To build and execute the Re:Photon verification suite:
```cmd
cd ReFix_src
build.bat
bin\rephoton_test.exe
```

---

## 3. Real Success Criterion

A test milestone is strictly marked successful only when:
1. Two independent client instances create and join the same game room.
2. Both actors synchronize player positions / network objects without desync.
3. Disconnections are handled cleanly without room corruption.
