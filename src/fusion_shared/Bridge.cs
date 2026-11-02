using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using Fusion.Sockets;

namespace ReFixFusionShared;

// Loopback datagram link to the ReFix Photon emulator (src/photon/pn_shared.cpp), one message per datagram,
// integers little-endian:
//   emulator -> server: 'C' i32 maxPlayers, utf8 NetworkProjectConfig JSON | 'J' i32 actor, 8-byte UniqueId |
//                       'L' i32 actor | 'M' i32 master actor | 'D' i32 sender actor, u8 reliable, packet |
//                       'K' keep-alive | 'Q' quit
//   server -> emulator: 'H' hello | 'R' simulation running | 'D' i32 target actor, u8 reliable, packet
sealed class Bridge : IDisposable {
    public const byte Config = (byte)'C', Join = (byte)'J', Leave = (byte)'L', Master = (byte)'M', Data = (byte)'D',
                      KeepAlive = (byte)'K', Quit = (byte)'Q', Hello = (byte)'H', Running = (byte)'R';

    readonly Socket _socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
    readonly EndPoint _emulator;
    readonly byte[] _recv = new byte[65536];
    readonly byte[] _send = new byte[65536];

    public Bridge(int emulatorPort) {
        _emulator = new IPEndPoint(IPAddress.Loopback, emulatorPort);
        _socket.Bind(new IPEndPoint(IPAddress.Loopback, 0));
        _socket.Blocking = false;
        _socket.ReceiveBufferSize = 4 << 20;
        _socket.SendBufferSize = 4 << 20;
    }

    // Next datagram from the emulator, or null when none is waiting.
    public ArraySegment<byte>? Receive() {
        while (_socket.Available > 0) {
            EndPoint from = new IPEndPoint(IPAddress.Any, 0);
            int n;
            try {
                n = _socket.ReceiveFrom(_recv, ref from);
            } catch (SocketException) {
                continue; // ICMP port unreachable from an earlier send; not fatal on loopback
            }
            if (n > 0 && from.Equals(_emulator)) return new ArraySegment<byte>(_recv, 0, n);
        }
        return null;
    }

    public void SendSignal(byte kind) => SendRaw(new[] { kind }, 1);

    public unsafe void SendData(int targetActor, byte* packet, int length, bool reliable) {
        if (length + 6 > _send.Length) return;
        _send[0] = Data;
        BitConverter.TryWriteBytes(new Span<byte>(_send, 1, 4), targetActor);
        _send[5] = reliable ? (byte)1 : (byte)0;
        new ReadOnlySpan<byte>(packet, length).CopyTo(new Span<byte>(_send, 6, length));
        SendRaw(_send, length + 6);
    }

    void SendRaw(byte[] buffer, int length) {
        try {
            _socket.SendTo(buffer, 0, length, SocketFlags.None, _emulator);
        } catch (SocketException e) {
            Console.WriteLine($"[bridge] send failed: {e.SocketErrorCode}");
        }
    }

    public void Dispose() => _socket.Dispose();

    public static string Utf8(ArraySegment<byte> msg, int offset) => Encoding.UTF8.GetString(msg.Array, msg.Offset + offset, msg.Count - offset);
    public static int Int(ArraySegment<byte> msg, int offset) => BitConverter.ToInt32(msg.Array, msg.Offset + offset);
}

// Fusion's relay socket on the plugin side: every peer is a Photon actor (NetAddress.FromActorId), packets come
// from and go to the emulator instead of Photon DATA events.
sealed class BridgeSocket : INetSocket {
    readonly Bridge _bridge;
    readonly Queue<(int actor, byte[] packet)> _incoming = new Queue<(int, byte[])>();
    SharedServer _server;

    public BridgeSocket(Bridge bridge) => _bridge = bridge;

    public void AttachServer(SharedServer server) => _server = server;

    public void Push(int actor, ArraySegment<byte> packet) => _incoming.Enqueue((actor, packet.ToArray()));

    public bool SupportsMultiThreading => false;
    public void Initialize(NetConfig config) { }
    public NetSocket Create(NetConfig config) => new NetSocket { Handle = 1 };
    public NetAddress Bind(NetSocket socket, NetConfig config) => NetAddress.FromActorId(0);
    public bool CanFragment(NetAddress address) => true;
    public bool Poll(NetSocket socket, long timeout) => _incoming.Count > 0;

    public unsafe int Receive(NetSocket socket, NetAddress* address, byte* buffer, int bufferLength) {
        if (!_incoming.TryDequeue(out var item)) return 0;
        if (item.packet.Length > bufferLength) return 0;
        *address = NetAddress.FromActorId(item.actor);
        item.packet.CopyTo(new Span<byte>(buffer, bufferLength));
        return item.packet.Length;
    }

    public unsafe int Send(NetSocket socket, NetAddress* address, byte* buffer, int bufferLength, bool reliable = false) {
        if (!address->IsRelayAddr) return -1;
        bool shouldBeReliable = reliable || (_server != null && _server.IsConnectionUnacknowledged(address->ActorId));
        _bridge.SendData(address->ActorId, buffer, bufferLength, shouldBeReliable);
        return bufferLength;
    }

    public void Destroy(NetSocket socket) => _incoming.Clear();
    public void DeleteEncryptionKey(NetAddress address) { }
    public void SetupEncryption(byte[] key, byte[] encryptedKey) { }
}
