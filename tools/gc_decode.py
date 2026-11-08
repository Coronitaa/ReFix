#!/usr/bin/env python3
"""
tools/gc_decode.py - ReFix CS:GO Game Coordinator Message Decoder
Decodes hex-encoded GC trace log lines using system protoc and official .proto schemas.
"""

import sys
import os
import subprocess
import shutil
import re
import argparse
import binascii

# Mapping of GC message ID -> (Protobuf Message Name, Proto File)
MSG_MAP = {
    # Base GC messages (gcsdk_gcmessages.proto, gcsystemmsgs.proto)
    4004: ("CMsgClientWelcome", "gcsdk_gcmessages.proto"),
    4005: ("CMsgClientWelcome", "gcsdk_gcmessages.proto"),  # k_EMsgGCServerWelcome uses CMsgClientWelcome
    4006: ("CMsgClientHello", "gcsdk_gcmessages.proto"),
    4007: ("CMsgServerHello", "gcsdk_gcmessages.proto"),
    4009: ("CMsgConnectionStatus", "gcsdk_gcmessages.proto"),
    4010: ("CMsgConnectionStatus", "gcsdk_gcmessages.proto"),

    # Shared Object Cache messages (gcsdk_gcmessages.proto)
    21: ("CMsgSOSingleObject", "gcsdk_gcmessages.proto"),
    22: ("CMsgSOSingleObject", "gcsdk_gcmessages.proto"),
    23: ("CMsgSOSingleObject", "gcsdk_gcmessages.proto"),
    24: ("CMsgSOCacheSubscribed", "gcsdk_gcmessages.proto"),
    25: ("CMsgSOCacheUnsubscribed", "gcsdk_gcmessages.proto"),
    26: ("CMsgSOMultipleObjects", "gcsdk_gcmessages.proto"),
    27: ("CMsgSOCacheSubscriptionCheck", "gcsdk_gcmessages.proto"),
    28: ("CMsgSOCacheSubscriptionRefresh", "gcsdk_gcmessages.proto"),

    # Econ & Store messages (base_gcmessages.proto, econ_gcmessages.proto)
    1008: ("CMsgRequestInventoryRefresh", "base_gcmessages.proto"),
    1025: ("CMsgUseItem", "base_gcmessages.proto"),
    1090: ("CMsgGCItemCustomizationNotification", "econ_gcmessages.proto"),
    2504: ("CMsgGCStorePurchaseFinalize", "base_gcmessages.proto"),
    2505: ("CMsgGCStorePurchaseFinalizeResponse", "base_gcmessages.proto"),
    2510: ("CMsgGCStorePurchaseInit", "base_gcmessages.proto"),
    2511: ("CMsgGCStorePurchaseInitResponse", "base_gcmessages.proto"),

    # CS:GO specific messages (cstrike15_gcmessages.proto)
    9101: ("CMsgGCCStrike15_v2_MatchmakingStart", "cstrike15_gcmessages.proto"),
    9102: ("CMsgGCCStrike15_v2_MatchmakingStop", "cstrike15_gcmessages.proto"),
    9103: ("CMsgGCCStrike15_v2_MatchmakingClient2ServerPing", "cstrike15_gcmessages.proto"),
    9104: ("CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate", "cstrike15_gcmessages.proto"),
    9106: ("CMsgGCCStrike15_v2_MatchmakingServerReservationResponse", "cstrike15_gcmessages.proto"),
    9107: ("CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve", "cstrike15_gcmessages.proto"),
    9109: ("CMsgGCCStrike15_v2_MatchmakingClient2GCHello", "cstrike15_gcmessages.proto"),
    9110: ("CMsgGCCStrike15_v2_MatchmakingGC2ClientHello", "cstrike15_gcmessages.proto"),
    9112: ("CMsgGCCStrike15_v2_MatchmakingGC2ClientAbandon", "cstrike15_gcmessages.proto"),
    9119: ("CMsgGCCStrike15_v2_ClientReportPlayer", "cstrike15_gcmessages.proto"),
    9120: ("CMsgGCCStrike15_v2_ClientReportServer", "cstrike15_gcmessages.proto"),
    9121: ("CMsgGCCStrike15_v2_ClientCommendPlayer", "cstrike15_gcmessages.proto"),
    9122: ("CMsgGCCStrike15_v2_ClientReportResponse", "cstrike15_gcmessages.proto"),
    9127: ("CMsgGCCStrike15_v2_ClientRequestPlayersProfile", "cstrike15_gcmessages.proto"),
    9128: ("CMsgGCCStrike15_v2_PlayersProfile", "cstrike15_gcmessages.proto"),
    9131: ("CMsgGCCStrike15_v2_PlayerOverwatchCaseUpdate", "cstrike15_gcmessages.proto"),
    9132: ("CMsgGCCStrike15_v2_PlayerOverwatchCaseAssignment", "cstrike15_gcmessages.proto"),
    9133: ("CMsgGCCStrike15_v2_PlayerOverwatchCaseStatus", "cstrike15_gcmessages.proto"),
    9134: ("CMsgGCCStrike15_v2_GC2ClientTextMsg", "cstrike15_gcmessages.proto"),
    9135: ("CMsgGCCStrike15_v2_Client2GCTextMsg", "cstrike15_gcmessages.proto"),
    9138: ("CMsgGCCStrike15_v2_ClientRequestWatchInfoFriends2", "cstrike15_gcmessages.proto"),
    9156: ("CMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockRequest", "cstrike15_gcmessages.proto"),
    9157: ("CMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse", "cstrike15_gcmessages.proto"),
    9158: ("CMsgGCCStrike15_v2_AccountPrivacySettingsResponse", "cstrike15_gcmessages.proto"),
    9163: ("CMsgGCCStrike15_v2_ClientRequestJoinFriendData", "cstrike15_gcmessages.proto"),
    9164: ("CMsgGCCStrike15_v2_ClientRequestJoinServerData", "cstrike15_gcmessages.proto"),
    9189: ("CMsgGCCStrike15_v2_Party_Register", "cstrike15_gcmessages.proto"),
    9190: ("CMsgGCCStrike15_v2_Party_Register_Response", "cstrike15_gcmessages.proto"),
    9194: ("CMsgGCCStrike15_v2_ClientGCRankUpdate", "cstrike15_gcmessages.proto"),
    9197: ("CMsgGCCStrike15_v2_ClientPartyJoinRelay", "cstrike15_gcmessages.proto"),
    9198: ("CMsgGCCStrike15_v2_ClientPartyWarning", "cstrike15_gcmessages.proto"),
    9201: ("CMsgGCCStrike15_v2_GetEventFavorites_Request", "cstrike15_gcmessages.proto"),
    9203: ("CMsgGCCStrike15_v2_GetEventFavorites_Response", "cstrike15_gcmessages.proto"),
    9204: ("CMsgGCCStrike15_v2_ClientVarValueNotificationInfo", "cstrike15_gcmessages.proto"),
}

# Non-protobuf / legacy messages with known structural meaning
LEGACY_MAP = {
    1007: "k_EMsgGCUnlockCrate (non-proto struct: 8B keyId + 8B crateId)",
    1059: "k_EMsgGCAdjustItemEquippedState (non-proto/proto struct: item_id=1, new_class=2, new_slot=3, swap=4)",
}

def find_protoc():
    p = shutil.which("protoc")
    if p:
        return p
    # Standard candidate paths on Windows
    candidates = [
        os.path.expandvars(r"%LOCALAPPDATA%\Microsoft\WinGet\Packages\Google.Protobuf_Microsoft.Winget.Source_8wekyb3d8bbwe\bin\protoc.exe"),
        r"C:\Program Files\protobuf\bin\protoc.exe",
        r"C:\Program Files (x86)\protobuf\bin\protoc.exe",
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None

def find_proto_dir():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, "..", "third_party", "protos", "csgo"),
        os.path.join(script_dir, "third_party", "protos", "csgo"),
        os.path.abspath("third_party/protos/csgo"),
    ]
    for c in candidates:
        if os.path.isdir(c):
            return os.path.abspath(c)
    return None

def decode_protobuf(protoc_bin, proto_dir, msg_type, proto_file, data_bytes):
    cmd = [
        protoc_bin,
        "-I", proto_dir,
        f"--decode={msg_type}",
        proto_file
    ]
    proc = subprocess.run(cmd, input=data_bytes, capture_output=True)
    stdout_txt = proc.stdout.decode("utf-8", errors="replace")
    stderr_txt = proc.stderr.decode("utf-8", errors="replace")
    
    # Check for wire type errors or unknown fields
    unknown_fields = []
    for line in stdout_txt.splitlines():
        # Match lines like "  1: 1014" or "99: 1" where field name is just digits
        m = re.match(r"^\s*([0-9]+)\s*:", line)
        if m:
            unknown_fields.append(int(m.group(1)))
            
    return proc.returncode, stdout_txt, stderr_txt, unknown_fields

def parse_trace_line(line):
    line = line.strip()
    if not line or line.startswith("#") or line.startswith(";"):
        return None
        
    # Format: [YYYY-MM-DD HH:MM:SS.mmm] <pid> <role> <dir> <msg> <proto> <hex>
    # or: YYYY-MM-DD HH:MM:SS.mmm <pid> <role> <dir> <msg> <proto> <hex>
    parts = line.split()
    if len(parts) < 6:
        return None
        
    idx = 0
    # Check if first part has brackets or is date
    if parts[0].startswith("["):
        # e.g. "[2026-11-07" "05:10:00.123]"
        if parts[0].endswith("]"):
            timestamp = parts[0].strip("[]")
            idx = 1
        elif len(parts) > 1 and parts[1].endswith("]"):
            timestamp = (parts[0] + " " + parts[1]).strip("[]")
            idx = 2
        else:
            timestamp = parts[0]
            idx = 1
    elif re.match(r"^\d{4}-\d{2}-\d{2}$", parts[0]):
        timestamp = parts[0] + " " + parts[1]
        idx = 2
    else:
        timestamp = "UNKNOWN"

    pid = parts[idx]
    idx += 1
    role = parts[idx]
    idx += 1
    direction = parts[idx]
    idx += 1
    msg_id = int(parts[idx])
    idx += 1
    proto = int(parts[idx])
    idx += 1
    hex_data = parts[idx] if idx < len(parts) else ""
    if hex_data == "-":
        hex_data = ""

    return {
        "timestamp": timestamp,
        "pid": pid,
        "role": role,
        "direction": direction,
        "msg_id": msg_id,
        "proto": proto,
        "hex": hex_data
    }

def main():
    parser = argparse.ArgumentParser(description="Decode CS:GO GC trace logs")
    parser.add_argument("logfile", help="Path to ReFix_gc_<pid>.log or trace file")
    parser.add_argument("--strict", action="store_true", help="Fail if any unknown fields or wire-type errors are detected")
    args = parser.parse_args()

    protoc_bin = find_protoc()
    if not protoc_bin:
        print("[!] Error: protoc executable not found in PATH or standard locations.", file=sys.stderr)
        sys.exit(1)

    proto_dir = find_proto_dir()
    if not proto_dir:
        print("[!] Error: third_party/protos/csgo directory not found.", file=sys.stderr)
        sys.exit(1)

    if not os.path.isfile(args.logfile):
        print(f"[!] Error: File not found: {args.logfile}", file=sys.stderr)
        sys.exit(1)

    print(f"[*] ReFix GC Decoder | protoc: {protoc_bin} | protos: {proto_dir}")
    print(f"[*] Decoding: {args.logfile}\n" + "="*80)

    total_msgs = 0
    decoded_msgs = 0
    error_count = 0

    with open(args.logfile, "r", encoding="utf-8", errors="replace") as f:
        for line_num, line in enumerate(f, start=1):
            entry = parse_trace_line(line)
            if not entry:
                continue

            total_msgs += 1
            msg_id = entry["msg_id"]
            direction_str = "->" if entry["direction"] == "out" else "<-"
            arrow = f"[{entry['role']}] {entry['direction'].upper():3s} {direction_str}"
            
            raw_bytes = b""
            if entry["hex"]:
                try:
                    raw_bytes = binascii.unhexlify(entry["hex"])
                except Exception as e:
                    print(f"[{entry['timestamp']}] [PID:{entry['pid']}] {arrow} Msg {msg_id} (INVALID HEX: {e})")
                    error_count += 1
                    continue

            if msg_id in MSG_MAP and entry["proto"]:
                msg_name, proto_file = MSG_MAP[msg_id]
                ret, decoded_txt, err_txt, unknown_fields = decode_protobuf(
                    protoc_bin, proto_dir, msg_name, proto_file, raw_bytes
                )
                
                status_tags = []
                if ret != 0:
                    status_tags.append("PARSE_ERROR")
                    error_count += 1
                if unknown_fields:
                    status_tags.append(f"UNKNOWN_FIELDS:{unknown_fields}")
                    if args.strict:
                        error_count += 1
                if not status_tags:
                    status_tags.append("OK")
                    decoded_msgs += 1

                print(f"[{entry['timestamp']}] [PID:{entry['pid']}] {arrow} {msg_name} (ID={msg_id}, {len(raw_bytes)}B) [{' '.join(status_tags)}]")
                if decoded_txt.strip():
                    for dline in decoded_txt.strip().splitlines():
                        print(f"    {dline}")
                if err_txt.strip():
                    print(f"    [!] protoc stderr: {err_txt.strip()}")
            elif msg_id in LEGACY_MAP:
                decoded_msgs += 1
                desc = LEGACY_MAP[msg_id]
                print(f"[{entry['timestamp']}] [PID:{entry['pid']}] {arrow} {desc} (ID={msg_id}, {len(raw_bytes)}B)")
            else:
                proto_str = "proto" if entry["proto"] else "raw"
                print(f"[{entry['timestamp']}] [PID:{entry['pid']}] {arrow} Msg {msg_id} ({proto_str}, {len(raw_bytes)}B) [UNMAPPED_MSG_ID]")

    print("="*80)
    print(f"[*] Summary: {total_msgs} messages processed, {decoded_msgs} successfully decoded, {error_count} errors.")
    
    if args.strict and error_count > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
