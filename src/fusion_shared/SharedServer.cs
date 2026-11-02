using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using Fusion;
using Fusion.Sockets;

namespace ReFixFusionShared;

// Runs Fusion's server simulation in Shared topology, the role Photon Cloud's Fusion plugin plays in a Shared
// room: clients connect to actor 0 through the relay, send the state of the objects they have authority over,
// and receive everyone else's. Player identity comes from the emulator (actor, Join UniqueId, master client).
sealed unsafe class SharedServer : Simulation.ICallbacks {
    readonly BridgeSocket _socket;
    readonly Stopwatch _clock = Stopwatch.StartNew();
    readonly Dictionary<int, byte[]> _players = new Dictionary<int, byte[]>(); // actor -> UniqueId
    Simulation.Server _sim;
    double _lastUpdate;
    int _masterActor;

    public SharedServer(BridgeSocket socket) => _socket = socket;

    public bool IsRunning => _sim != null;
    internal Simulation Simulation => _sim;

    public void Start(string configJson, int maxPlayers) {
        if (_sim != null) return;
        NetworkProjectConfig config = UnityJson.FromJson<NetworkProjectConfig>(configJson);
        if (config.Simulation.PlayerCount <= 0) config.Simulation.PlayerCount = Math.Max(maxPlayers, 1);
        config.Simulation.Topology = Topologies.Shared;

        // Simulation only touches its runner for start-up bookkeeping; the plugin has no NetworkRunner either.
        var runner = (NetworkRunner)RuntimeHelpers.GetUninitializedObject(typeof(NetworkRunner));
        runner.OnGameStartedInvoked = true;
        var args = new SimulationArgs {
            Mode = SimulationModes.Server,
            Address = NetAddress.FromActorId(0),
            Socket = _socket,
            Config = config,
            Callbacks = this,
        };
        _sim = new Simulation.Server(args);
        _sim.Runner = runner;
        runner._simulation = _sim;
        foreach (var (actor, uniqueId) in _players) _sim.RegisterUniqueIdPlayerMapping(actor, uniqueId, PlayerRef.FromIndex(actor));
        _lastUpdate = _clock.Elapsed.TotalSeconds;
        Console.WriteLine($"[server] Shared simulation started (players {config.Simulation.PlayerCount}, " +
                          $"tick rate {config.Simulation.TickRateSelection.Client})");
    }

    public void AddPlayer(int actor, byte[] uniqueId) {
        _players[actor] = uniqueId;
        _sim?.RegisterUniqueIdPlayerMapping(actor, uniqueId, PlayerRef.FromIndex(actor));
        Console.WriteLine($"[server] actor {actor} registered");
    }

    public void RemovePlayer(int actor) {
        _players.Remove(actor);
        _sim?.Disconnect(NetAddress.FromActorId(actor));
        Console.WriteLine($"[server] actor {actor} left");
    }

    public void SetMaster(int actor) => _masterActor = actor;

    public bool IsConnectionUnacknowledged(int actorId) {
        if (_sim == null) return false;
        var conn = _sim.GetSimulationConnectionForPlayer(PlayerRef.FromIndex(actorId));
        if (conn == null) return true;
        return conn._latestTickAcknowledged == 0;
    }

    public void Update() {
        if (_sim == null) return;
        double now = _clock.Elapsed.TotalSeconds;
        double dt = now - _lastUpdate;
        if (dt <= 0) return;
        _lastUpdate = now;
        _sim.Update(dt);
        SyncMasterClient();
    }

    void SyncMasterClient() {
        if (_masterActor == 0 || !_sim.HasStructData(NetworkId.RuntimeConfig)) return;
        ref SimulationRuntimeConfig runtime = ref _sim.RuntimeConfigRef;
        PlayerRef master = PlayerRef.FromIndex(_masterActor);
        if (runtime.MasterClient != master) {
            runtime.MasterClient = master;
            Console.WriteLine($"[server] master client is now {master}");
        }
        if (_sim.TryGetMeta(NetworkId.SceneInfo, out var sceneInfoMeta) && sceneInfoMeta.StateAuthority.IsNone) {
            sceneInfoMeta.StateAuthority = PlayerRef.MasterClient;
        }
        if (_sim.TryGetMeta(NetworkId.PhysicsInfo, out var physicsInfoMeta) && physicsInfoMeta.StateAuthority.IsNone) {
            physicsInfoMeta.StateAuthority = PlayerRef.MasterClient;
        }
    }

    // ---- Simulation.ICallbacks ----
    public bool IsSharedModeMasterClient => false;
    public bool CanReceivePlayerJoinLeaveCallbacks => true;
    public PlayerRef LocalPlayerRef => PlayerRef.None;

    public OnConnectionRequestReply OnConnectionRequest(NetAddress remoteAddress, byte[] token) => OnConnectionRequestReply.Ok;

    // RPCs: the plugin cannot run game code, it only routes. Targeted messages go to their target, the others to
    // every player but the sender; each client then filters by its own authority.
    public SimulationMessageResult OnMessage(SimulationMessage* message) {
        PlayerRef source = message->Source;
        if (message->IsTargeted()) {
            PlayerRef target = message->Target;
            if (target != source && _sim.PlayerValid(target)) _sim.ForwardMessage(message, target, required: true);
            return SimulationMessageResult.Handled;
        }
        foreach (SimulationConnection connection in _sim.Connections) {
            PlayerRef player = _sim.Connection2Player(connection);
            if (player != source) _sim.ForwardMessage(message, player, required: false);
        }
        return SimulationMessageResult.Handled;
    }

    public void PlayerJoined(PlayerRef player) {
        Console.WriteLine($"[server] {player} joined");
        if (_sim == null) return;
        var newConn = _sim.GetSimulationConnectionForPlayer(player);
        if (newConn != null) {
            foreach (var meta in _sim._metaLookup.Values) {
                if (!meta.IsStruct) {
                    newConn.GetObjectData(meta.Id, create: true);
                }
            }
        }
    }

    public void PlayerLeft(PlayerRef player) => Console.WriteLine($"[server] {player} left");

    public void OnServerStart() {
        Console.WriteLine("[server] first tick");
        if (_sim.TryGetMeta(NetworkId.SceneInfo, out var sceneInfoMeta)) {
            sceneInfoMeta.StateAuthority = PlayerRef.MasterClient;
            Console.WriteLine("[server] SceneInfo StateAuthority set to MasterClient");
        }
        if (_sim.TryGetMeta(NetworkId.PhysicsInfo, out var physicsInfoMeta)) {
            physicsInfoMeta.StateAuthority = PlayerRef.MasterClient;
            Console.WriteLine("[server] PhysicsInfo StateAuthority set to MasterClient");
        }
    }
    public void OnInternalConnectionAttempt(int attempt, int total, out bool shouldChange, out NetAddress newAddress) {
        shouldChange = false;
        newAddress = default;
    }
    // false: no local instance handled it, so the simulation drops the object's state itself.
    public bool RemoteObjectDestroyed(NetworkId id) => false;
    public void OnConnectionFailed(NetAddress remoteAddress, NetConnectFailedReason reason) =>
        Console.WriteLine($"[server] connection from {remoteAddress} failed: {reason}");

    public void OnTick() { }
    public void OnClientStart() { }
    public void OnAfterClientSidePredictionReset() { }
    public void OnBeforeClientSidePredictionReset() { }
    public void OnAfterTick() { }
    public void OnBeforeTick() { }
    public void OnAfterAllTicks(bool resimulation, int tickCount) { }
    public void OnBeforeAllTicks(bool resimulation, int tickCount) { }
    public void OnAfterSimulation() { }
    public void OnBeforeSimulation(int forwardTickCount) { }
    public void OnBeforeCopyPreviousState() { }
    public void OnConnectedToServer() { }
    public void OnDisconnectedFromServer(NetDisconnectReason reason) { }
    public void OnReliableData(PlayerRef player, ReliableId id, bool local, byte[] dataArray) { }
    public void ObjectStateAuthorityChanged(NetworkId id, bool gained) { }
    public void ObjectInputAuthorityChanged(NetworkId id, bool gained) { }
    public void ObjectIsSimulatedChanged(NetworkId id, bool simulated) { }
    public void ObjectEnterAOI(PlayerRef player, NetworkId id) { }
    public void ObjectExitAOI(PlayerRef player, NetworkId id) { }
    public void ObjectChanged(PlayerRef player, NetworkObjectMeta obj, Simulation.ObjectChangeType changeType) {
        if (_sim == null || obj.IsStruct) return;
        if (changeType == Simulation.ObjectChangeType.Created) {
            foreach (SimulationConnection conn in _sim.Connections) {
                PlayerRef connPlayer = _sim.Connection2Player(conn);
                if (connPlayer != player) {
                    conn.GetObjectData(obj.Id, create: true);
                }
            }
        }
    }
    public void RemoteObjectCreated(NetworkObjectMeta obj) { }
    public void UpdateRemotePrefabs() { }
    public void OnInput(SimulationInput input) { }
    public void OnInputMissing(SimulationInput input) { }
}
