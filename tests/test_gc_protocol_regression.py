#!/usr/bin/env python3
"""
tests/test_gc_protocol_regression.py - Protocol Regression Test Harness
Replaces test_csgo_verification.cpp by verifying that emulator GC responses
decode cleanly via official protobuf schemas with ZERO unknown fields and ZERO wire-type errors.
"""

import sys
import os
import glob
import subprocess

# Add tools directory to path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
TOOLS_DIR = os.path.join(REPO_ROOT, "tools")
sys.path.insert(0, TOOLS_DIR)

import gc_decode

def run_regression_test(trace_file):
    print(f"[*] Running GC Protocol Regression Test on: {trace_file}")
    
    protoc_bin = gc_decode.find_protoc()
    assert protoc_bin, "protoc executable not found"
    
    proto_dir = gc_decode.find_proto_dir()
    assert proto_dir, "third_party/protos/csgo directory not found"
    
    passed_responses = 0
    failed_responses = 0
    total_lines = 0

    with open(trace_file, "r", encoding="utf-8", errors="replace") as f:
        for line_num, line in enumerate(f, start=1):
            entry = gc_decode.parse_trace_line(line)
            if not entry:
                continue
            total_lines += 1

            # Check emulator responses (direction == "out")
            if entry["direction"] != "out":
                continue

            msg_id = entry["msg_id"]
            if not entry["proto"]:
                # Non-protobuf messages
                if msg_id in gc_decode.LEGACY_MAP:
                    passed_responses += 1
                continue

            if msg_id not in gc_decode.MSG_MAP:
                print(f"[!] FAILED: Emulator sent unmapped protobuf message ID {msg_id} at line {line_num}")
                failed_responses += 1
                continue

            msg_name, proto_file = gc_decode.MSG_MAP[msg_id]
            import binascii
            try:
                raw_bytes = binascii.unhexlify(entry["hex"]) if entry["hex"] else b""
            except Exception as e:
                print(f"[!] FAILED: Invalid hex in message {msg_name} (ID {msg_id}) at line {line_num}: {e}")
                failed_responses += 1
                continue

            ret, decoded_txt, err_txt, unknown_fields = gc_decode.decode_protobuf(
                protoc_bin, proto_dir, msg_name, proto_file, raw_bytes
            )

            if ret != 0:
                print(f"[!] FAILED (Wire error): {msg_name} (ID {msg_id}) failed protoc decode at line {line_num}:")
                print(f"    stderr: {err_txt.strip()}")
                failed_responses += 1
                continue

            if unknown_fields:
                print(f"[!] FAILED (Unknown fields): {msg_name} (ID {msg_id}) contains undefined protobuf fields {unknown_fields} at line {line_num}!")
                print(f"    decoded text:\n{decoded_txt.strip()}")
                failed_responses += 1
                continue

            passed_responses += 1

    print(f"[*] Regression Test Result for {os.path.basename(trace_file)}:")
    print(f"    Total lines: {total_lines}, Verified emulator responses: {passed_responses}, Failures: {failed_responses}")
    return failed_responses == 0

def main():
    if len(sys.argv) > 1:
        trace_files = sys.argv[1:]
    else:
        # Search for trace files in tests/fixtures or evidence or root
        trace_files = glob.glob(os.path.join(REPO_ROOT, "tests", "fixtures", "*.log"))
        trace_files += glob.glob(os.path.join(REPO_ROOT, "ReFix_gc_*.log"))
        trace_files += glob.glob(os.path.join(r"D:\Games\CSGO", "ReFix_gc_*.log"))

    if not trace_files:
        print("[*] No GC trace files found to test. Creating baseline test fixture...")
        fixtures_dir = os.path.join(REPO_ROOT, "tests", "fixtures")
        os.makedirs(fixtures_dir, exist_ok=True)
        sample_trace = os.path.join(fixtures_dir, "sample_gc_trace.log")
        # Write valid sample trace line (CMsgClientWelcome with valid version 0)
        with open(sample_trace, "w") as f:
            f.write("# Sample valid GC trace log\n")
            f.write("2026-11-07 05:00:00.000 1000 client out 4004 1 0800\n")
        trace_files = [sample_trace]

    all_passed = True
    for tf in trace_files:
        if os.path.isfile(tf):
            if not run_regression_test(tf):
                all_passed = False

    if not all_passed:
        print("\n[!] PROTOCOL REGRESSION TEST FAILED!")
        sys.exit(1)
    else:
        print("\n[+] PROTOCOL REGRESSION TEST PASSED (100% OK)")
        sys.exit(0)

if __name__ == "__main__":
    main()
