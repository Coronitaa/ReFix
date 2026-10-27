using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using Fusion;

namespace ReFixFusionShared;

static class Program {
    const double EmulatorSilenceTimeoutSeconds = 15;

    [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint period);

    static int Main(string[] args) {
        if (args.Length < 1 || !int.TryParse(args[0], out int port) || port <= 0 || port > 65535) {
            Console.Error.WriteLine("usage: ReFixFusionShared <emulator-udp-port> [log-file]");
            return 2;
        }
        if (args.Length > 1) {
            try {
                string fullPath = Path.GetFullPath(args[1]);
                string dir = Path.GetDirectoryName(fullPath);
                if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) Directory.CreateDirectory(dir);
                var log = new StreamWriter(fullPath, append: false) { AutoFlush = true };
                Console.SetOut(log);
                Console.SetError(log);
            } catch (Exception ex) {
                Console.Error.WriteLine($"[server] failed to open log file {args[1]}: {ex.Message}");
            }
        }
        UnityRuntime.Install();
        return Run(port);
    }

    // Kept apart from Main so no Fusion or Unity type is touched before UnityRuntime is installed.
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int Run(int port) {
        Log.InitializeForConsole(LogLevel.Info);
        PluginPatches.Apply();
        timeBeginPeriod(1);

        using var bridge = new Bridge(port);
        var socket = new BridgeSocket(bridge);
        var server = new SharedServer(socket);
        bridge.SendSignal(Bridge.Hello);
        Console.WriteLine($"[server] ReFix Fusion Shared server up, emulator port {port}");

        var silence = Stopwatch.StartNew();
        while (silence.Elapsed.TotalSeconds < EmulatorSilenceTimeoutSeconds) {
            while (bridge.Receive() is ArraySegment<byte> msg) {
                silence.Restart();
                if (!Dispatch(msg, server, socket, bridge)) return 0;
            }
            try {
                server.Update();
            } catch (Exception e) {
                Console.WriteLine($"[server] update failed: {e}");
            }
            Thread.Sleep(1);
        }
        Console.WriteLine("[server] emulator went silent, exiting");
        return 0;
    }

    // Returns false when the emulator asked the server to quit.
    static bool Dispatch(ArraySegment<byte> msg, SharedServer server, BridgeSocket socket, Bridge bridge) {
        byte kind = msg[0];
        switch (kind) {
            case Bridge.Config when msg.Count >= 5:
                bool wasRunning = server.IsRunning;
                server.Start(Bridge.Utf8(msg, 5), Bridge.Int(msg, 1));
                if (!wasRunning && server.IsRunning) bridge.SendSignal(Bridge.Running);
                break;
            case Bridge.Join when msg.Count >= 13:
                server.AddPlayer(Bridge.Int(msg, 1), msg.Slice(5, 8).ToArray());
                break;
            case Bridge.Leave when msg.Count >= 5:
                server.RemovePlayer(Bridge.Int(msg, 1));
                break;
            case Bridge.Master when msg.Count >= 5:
                server.SetMaster(Bridge.Int(msg, 1));
                break;
            case Bridge.Data when msg.Count > 6:
                socket.Push(Bridge.Int(msg, 1), msg.Slice(6));
                break;
            case Bridge.Quit:
                Console.WriteLine("[server] quit requested");
                return false;
        }
        return true;
    }
}
