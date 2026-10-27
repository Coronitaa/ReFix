using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using Fusion;
using HarmonyLib;

namespace ReFixFusionShared;

// Simulation.OnMessageInternal is an empty stub in the Unity build of Fusion.Runtime: the internal messages clients
// send in Shared mode are answered by Photon's plugin build only. This plays that part.
static unsafe class PluginPatches {
    static readonly HashSet<SimulationMessageInternalTypes> s_unhandledLogged = new HashSet<SimulationMessageInternalTypes>();

    public static void Apply() {
        var harmony = new Harmony("refix.fusion.shared");
        harmony.Patch(AccessTools.Method(typeof(Simulation), nameof(Simulation.OnMessageInternal)),
                      prefix: new HarmonyMethod(typeof(PluginPatches), nameof(OnMessageInternalPrefix)));

        var readObjUpdates = AccessTools.Method(typeof(Simulation.StateReplicator), "ReadObjectUpdates");
        if (readObjUpdates != null) {
            harmony.Patch(readObjUpdates, transpiler: new HarmonyMethod(typeof(PluginPatches), nameof(ReadObjectUpdatesTranspiler)));
            Console.WriteLine("[plugin] Patched StateReplicator.ReadObjectUpdates for headless server");
        } else {
            Console.WriteLine("[plugin] WARNING: Could not find StateReplicator.ReadObjectUpdates to patch");
        }

        var createInternal = AccessTools.Method(typeof(Simulation.Server), "CreateInternalStateObjects");
        if (createInternal != null) {
            harmony.Patch(createInternal, prefix: new HarmonyMethod(typeof(PluginPatches), nameof(CreateInternalStateObjectsPrefix)));
            Console.WriteLine("[plugin] Patched Simulation.Server.CreateInternalStateObjects");
        } else {
            Console.WriteLine("[plugin] WARNING: Could not find Simulation.Server.CreateInternalStateObjects to patch");
        }
    }

    static void CreateInternalStateObjectsPrefix(ref PlayerRef sceneInfoStateAuth) {
        sceneInfoStateAuth = PlayerRef.MasterClient;
    }

    // In the Unity build of Fusion.Runtime, StateReplicator.ReadObjectUpdates assumes a Shared-topology simulation
    // is always a client with a valid LocalPlayer connection (calling _simulation.GetSimulationConnectionForPlayer(_simulation.LocalPlayer)).
    // On Simulation.Server, LocalPlayer is PlayerRef.None and this returns null, causing a NullReferenceException on GetObjectData.
    // This transpiler inserts a null-check: if the connection is null, jump straight to the merge point where headers
    // are copied to meta, skipping client-only AOI interest tracking.
    static IEnumerable<CodeInstruction> ReadObjectUpdatesTranspiler(IEnumerable<CodeInstruction> instructions) {
        var list = new List<CodeInstruction>(instructions);
        int getConnIdx = -1;
        object targetLabel = null;

        for (int i = 0; i < list.Count; i++) {
            var inst = list[i];
            if (inst.opcode == OpCodes.Callvirt && inst.operand is MethodInfo mi && mi.Name == "GetSimulationConnectionForPlayer") {
                getConnIdx = i;
            }
            if (getConnIdx != -1 && targetLabel == null && inst.opcode == OpCodes.Br) {
                targetLabel = inst.operand;
                break;
            }
        }

        if (getConnIdx != -1 && targetLabel != null) {
            var stlocInst = list[getConnIdx + 1];
            CodeInstruction ldlocInst;
            if (stlocInst.opcode == OpCodes.Stloc_0) ldlocInst = new CodeInstruction(OpCodes.Ldloc_0);
            else if (stlocInst.opcode == OpCodes.Stloc_1) ldlocInst = new CodeInstruction(OpCodes.Ldloc_1);
            else if (stlocInst.opcode == OpCodes.Stloc_2) ldlocInst = new CodeInstruction(OpCodes.Ldloc_2);
            else if (stlocInst.opcode == OpCodes.Stloc_3) ldlocInst = new CodeInstruction(OpCodes.Ldloc_3);
            else if (stlocInst.opcode == OpCodes.Stloc_S) ldlocInst = new CodeInstruction(OpCodes.Ldloc_S, stlocInst.operand);
            else if (stlocInst.opcode == OpCodes.Stloc) ldlocInst = new CodeInstruction(OpCodes.Ldloc, stlocInst.operand);
            else throw new InvalidOperationException($"Unexpected opcode after GetSimulationConnectionForPlayer: {stlocInst.opcode}");

            var brfalseInst = new CodeInstruction(OpCodes.Brfalse, targetLabel);
            list.Insert(getConnIdx + 2, ldlocInst);
            list.Insert(getConnIdx + 3, brfalseInst);
            Console.WriteLine("[plugin] StateReplicator.ReadObjectUpdates transpiler successfully injected null check");
        } else {
            Console.WriteLine("[plugin] WARNING: ReadObjectUpdates pattern not matched for transpiler!");
        }

        return list;
    }

    static bool OnMessageInternalPrefix(Simulation __instance, SimulationMessage* message) {
        try {
            HandleInternal(__instance, message);
        } catch (Exception e) {
            Console.WriteLine($"[plugin] internal message failed: {e}");
        }
        return false;
    }

    static void HandleInternal(Simulation sim, SimulationMessage* message) {
        if (!sim.IsServer) return;
        PlayerRef source = message->Source;
        SimulationMessageInternalTypes type = Simulation.GetMessageInternalType(message);
        switch (type) {
            case SimulationMessageInternalTypes.SharedModeRequestStateAuthority: {
                var request = Simulation.GetMessageInternalData<SimulationMessageInternal_SharedModeRequestStateAuthority>(message);
                RequestStateAuthority(sim, source, request.Object, request.Acquire != 0);
                break;
            }
            case SimulationMessageInternalTypes.SharedModeSetAlwaysInterested: {
                var request = Simulation.GetMessageInternalData<SimulationMessageInternal_SharedModeSetAlwaysInterested>(message);
                PlayerRef player = PlayerRef.FromIndex(request.Player);
                if (!sim.TryGetMeta(request.Object, out NetworkObjectMeta meta)) break;
                SimulationConnection connection = sim.GetSimulationConnectionForPlayer(player);
                if (request.Interested != 0) connection?.AddAlwaysInterested(meta);
                else connection?.RemoveAlwaysInterested(meta);
                break;
            }
            case SimulationMessageInternalTypes.SetPlayerObject: {
                var request = Simulation.GetMessageInternalData<SimulationMessageInternal_SetPlayerObject>(message);
                sim.SetPlayerObjectId(source, request.Object);
                break;
            }
            default:
                if (s_unhandledLogged.Add(type)) Console.WriteLine($"[plugin] internal message {type} from {source} not handled");
                break;
        }
    }

    // Shared mode authority rules: a player may take an object nobody owns, one whose owner left, or one created
    // with AllowStateAuthorityOverride; releasing is only for the current owner.
    static void RequestStateAuthority(Simulation sim, PlayerRef source, NetworkId id, bool acquire) {
        if (!sim.TryGetMeta(id, out NetworkObjectMeta meta)) return;
        PlayerRef current = meta.StateAuthority;
        if (acquire) {
            if (current == source) return;
            bool free = current.IsNone || !sim.PlayerValid(current);
            bool overridable = (meta.Header.Flags & NetworkObjectHeaderFlags.AllowStateAuthorityOverride) != 0;
            if (!free && !overridable) return;
            meta.StateAuthority = source;
        } else {
            if (current != source) return;
            meta.StateAuthority = PlayerRef.None;
        }
    }
}
