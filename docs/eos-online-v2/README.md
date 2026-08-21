# ReFix EOS Online v2 — Project Overview & Documentation

## 1. Executive Summary

**ReFix EOS Online v2** is a ground-up redesign and replacement of the Epic Online Services (EOS) emulation layer in ReFix, specifically targeting Unreal Engine titles utilizing **RedpointEOS** (Redpoint Online Subsystem EOS) and standard Unreal OnlineSubsystemEOS.

The previous implementation relied on a fragile, synthetic translation layer between EOS API calls and local Steamworks lobby/P2P calls. This approach caused severe architectural impedance mismatches, resulting in phantom lobbies, hardcoded 4-player caps, broken invitations, duplicate session errors (AlreadyExists), desynchronized state machines, and inability for peers to discover or connect to each other across LAN and WAN.

EOS Online v2 establishes a true online-grade EOS architecture featuring:
- Deterministic, unique, and persistent user identity (ProductUserId / EpicAccountId).
- Authoritative session and lobby state machines matching official EOS specifications.
- Dynamic attributes and metadata management without synthetic values or query lying.
- Real event-driven asynchronous callbacks and notifications.
- Hybrid P2P networking with direct UDP hole punching (STUN/UPnP) and robust fallback relay.
- Clean separation between the EOS SDK proxy frontend and the ReFix Online networking backend.

---

## 2. Strict Isolation & Subsystem Safety Rules

To protect the integrity of the overall ReFix project, development must strictly follow these rules:

1. **Branch Isolation**: All work is strictly isolated in branch eature/eos-online-v2 based on main. No commits are pushed to main until full validation passes.
2. **Subsystem Protection**: The following subsystems are frozen and must not be altered, broken, or regressed:
   - **ReFix Online for Unreal without EOS** (native Steamworks hooks).
   - **Re:Goldberg** standalone LAN emulator.
   - **Re:Goldberg for Godot**.
   - **Re:Goldberg for Unreal**.
   - **Legacy RedboneEOS** proxy (retained intact for backward fallback during migration).
   - **Re:Photon** branch, flow, and architecture (strictly isolated, untouched).
   - **Game detection infrastructure** (unreal_detect.cpp, winmm_proxy.cpp).
   - **ReFix Configuration** (ReFix.ini) and GUI overlays (server_browser_gui.cpp).
3. **Evidence-Based Engineering**: Every architecture decision, API classification, and bug explanation is anchored with citations.
4. **Zero Hallucinations**: Unverified assumptions are explicitly marked **[HYPOTHESIS]** or **[UNKNOWN]** and scheduled for runtime tracing.

---

## 3. Documentation Index

The docs/eos-online-v2/ directory provides full technical coverage of the investigation, diagnosis, design, and implementation plan:

| Document | Purpose & Key Topics |
| :--- | :--- |
| [rchitecture.md](architecture.md) | Full modular architecture specification: Frontend EOS Proxy, Core State Engine, and Backend Wire Transport. |
| [current-implementation.md](current-implementation.md) | Exhaustive code audit of legacy src/eos_proxy.cpp, handles, fake PUIDs, Steam tunneling, and state tracking flaws. |
| [pi-coverage.md](api-coverage.md) | Complete inventory and status classification of all 347 imported EOS APIs and 679 exports. |
| [
edpoint-analysis.md](redpoint-analysis.md) | Deep breakdown of Unreal Engine + RedpointEOS integration pipeline, Sessions vs Lobbies, auth tickets, and NetDriver interaction. |
| [
eferences.md](references.md) | Formal reference index with official Epic documentation links, reference emulator analysis, and binary dumps. |
| [known-problems.md](known-problems.md) | Comprehensive Root Cause Analysis (RCA) explaining every reported issue. |
| [	est-plan.md](test-plan.md) | Formal test matrix detailing Tests A through I (LAN, WAN, Invites, State Transitions, Attribute Integrity, Relay Fallback). |
| [migration-plan.md](migration-plan.md) | 14-step incremental milestone execution roadmap, build verification gates, and coexistence strategy. |
