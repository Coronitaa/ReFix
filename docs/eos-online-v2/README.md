# ReFix EOS Online v2 — Phase 0: Observation & Diagnosis

## 1. Executive Summary & Phase Status

**PHASE 0: OBSERVATION ONLY (ACTIVE)**
- **Status**: Implementation is strictly **BLOCKED**. No production source code replacement, no deletion of legacy proxy, no final backend or relay implementation until Phase 0 sign-off.
- **Objective**: Conduct comprehensive code audit, binary disassembly symbol extraction, call graph mapping, root cause analysis (RCA), and evidence-based architectural redesign.
- **Repository Branch**: `feature/eos-online-v2` (checked out directly from `main` at commit `0ee88cb`, fully synchronized with origin).

---

## 2. Strict Subsystem Isolation Rules

The following existing subsystems are frozen and must not be altered, broken, or regressed:
1. **ReFix Online for Unreal without EOS** (native Steamworks hooks).
2. **Re:Goldberg** standalone LAN emulator.
3. **Re:Goldberg for Godot**.
4. **Re:Goldberg for Unreal**.
5. **Legacy RedboneEOS proxy** (`src/eos_proxy.cpp`, preserved as selectable `Backend=legacy`).
6. **Re:Photon** branch and infrastructure (strictly untouched).
7. **Game Detection Infrastructure** (`unreal_detect.cpp`, `winmm_proxy.cpp`).
8. **ReFix Configuration** (`ReFix.ini`) and GUI overlays (`server_browser_gui.cpp`).

---

## 3. Phase 0 Documentation Index

| Document | Purpose & Key Topics |
| :--- | :--- |
| [`README.md`](README.md) | Phase 0 overview, constraints, isolation rules, and roadmap index. |
| [`architecture.md`](architecture.md) | Legacy proxy system architecture, component breakdown, and flow. |
| [`current-implementation.md`](current-implementation.md) | Line-by-line audit of legacy `src/eos_proxy.cpp`, pointer heuristics, and fake handles. |
| [`api-coverage.md`](api-coverage.md) | Complete inventory and status classification of all 347 imported EOS APIs and 679 DLL exports. |
| [`redpoint-call-map.md`](redpoint-call-map.md) | Evidence-backed RedpointEOS call graph (`Game -> Unreal -> Redpoint -> EOS SDK -> ReFix`). |
| [`runtime-traces.md`](runtime-traces.md) | Specification for non-intrusive runtime instrumentation and logging format. |
| [`root-cause.md`](root-cause.md) | Definite Root Cause Analysis for all known bugs with line-number citations. |
| [`reference-projects.md`](reference-projects.md) | Deep comparison with Nemirtingas `eos_sdk_emu`, EOSLANKit, and official EOS specs. |
| [`proposed-architecture.md`](proposed-architecture.md) | Ground-up modular architecture specification: Logical Identity, Authoritative Backend, P2P steps. |
| [`test-plan.md`](test-plan.md) | Test matrix (Tests A–I) and the end-to-end **Golden Test** specification. |
| [`open-questions.md`](open-questions.md) | Structured register of remaining unknowns (`UNKNOWN` / `HYPOTHESIS`) and verification criteria. |
