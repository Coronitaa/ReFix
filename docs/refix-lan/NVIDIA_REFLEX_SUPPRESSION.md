# NVIDIA Reflex Overlay Suppression Specification

**Document Version:** 1.0.0  
**Scope:** ReFix LAN (branch `refix_lan`)  
**Implementation:** [`src/overlay/nvidia_reflex_block.h`](file:///C:/Users/Valen/.gemini/antigravity/worktrees/REFIX_LAN/refix_mods_sdk_architecture/src/overlay/nvidia_reflex_block.h), [`src/overlay/nvidia_reflex_block.cpp`](file:///C:/Users/Valen/.gemini/antigravity/worktrees/REFIX_LAN/refix_mods_sdk_architecture/src/overlay/nvidia_reflex_block.cpp)  
**Test Suite:** [`tests/test_nvidia_reflex_block.cpp`](file:///C:/Users/Valen/.gemini/antigravity/worktrees/REFIX_LAN/refix_mods_sdk_architecture/tests/test_nvidia_reflex_block.cpp) $\rightarrow$ `bin/test_nvidia_reflex_block.exe`  

---

## 1. Problem Statement

When games launch with the ReFix in-game overlay (Dear ImGui running atop DirectX 11 / DirectX 12 / Vulkan swapchains), the proprietary **NVIDIA Reflex Analyzer / GeForce Experience In-Game Overlay** frequently attempts to inject its own DirectX presentation hooks (`IDXGISwapChain::Present`, `Present1`, `ResizeBuffers`).

### Failure Modes Caused by NVIDIA Reflex:
1. **Hook Chaining Collisions:** Both ReFix and NVIDIA Reflex hook `IDXGISwapChain::Present`. If NVIDIA inserts its hook before or after ReFix without proper trampoline restoration, it causes access violations (`0xC0000005`) during swapchain destruction or mode switches (e.g., Alt+Tab).
2. **Latency Metric Interference:** Reflex attempts to measure input-to-present latency by sampling Windows message pumps (`PeekMessage`, `GetMessage`). ReFix's internal input interception for the overlay conflicts with this metric tracker, resulting in severe micro-stuttering or dropped controller events.
3. **Double-Overlay Render Artifacts:** Reflex draws its latency statistics box directly onto the backbuffer after ReFix has completed ImGui rendering, clipping overlay modals, avatars, and profile management interfaces.

---

## 2. Technical Mechanism & Implementation

To prevent interference without modifying game executables or NVIDIA driver binaries on disk, ReFix suppresses the Reflex overlay entirely in-memory using a multi-phase defense:

### Phase 1: Environment Variable Suppression
NVIDIA drivers and overlay injectors (`nvspcap64.dll`, `NvCamera64.dll`, `nvldumdx.dll`) check specific Windows environment variables during initialization to determine if the overlay and latency analyzer are enabled.

ReFix sets the following variables immediately upon DLL loading (prior to DirectX initialization):

| Variable | Value | Purpose |
|---|---|---|
| `NVTX_DISABLE` | `"1"` | Disables NVIDIA Tools Extension markers |
| `GFEXPERIENCE_OVERLAY` | `"0"` | Forces GeForce Experience overlay off |
| `__NV_DISABLE_REFLEX_OVERLAY` | `"1"` | Suppresses Reflex UI overlay rendering |
| `NV_REFLEX_DISABLE_OVERLAY` | `"1"` | Additional driver-level suppression flag |
| `NVSPCAP_DISABLE` | `"1"` | Disables ShadowPlay / Share capture hooks |
| `DISABLE_NV_OVERLAY` | `"1"` | Generic driver flag for NVIDIA overlay |

### Phase 2: Interception of Variable Recreation
Certain games explicitly attempt to re-enable Reflex by invoking `SetEnvironmentVariableA/W` or calling `NvAPI_Initialize`. ReFix installs inline hooks on `SetEnvironmentVariableA` and `SetEnvironmentVariableW`:
- Any attempt by the game to overwrite or clear these suppression flags is neutralized.
- Calls targeting any other variable pass through to the real Win32 implementation unchanged.

### Phase 3: Early Arming (`ArmEarly`)
To guarantee that the suppression takes effect before NVIDIA driver hooks load:
1. **`winmm_proxy.cpp` (`DllMain`)**: Invokes `NvidiaReflexBlock::ArmEarly()` as soon as `DLL_PROCESS_ATTACH` occurs.
2. **`ReFixOverlay::ArmEarly()`**: Ensures variables are locked before DirectX runtime DLLs (`d3d11.dll`, `dxgi.dll`) are loaded.
3. **`ReFixOverlay::Start()`**: Re-verifies state idempotently when the overlay thread initializes.

---

## 3. Idempotence & Thread Safety

`NvidiaReflexBlock` is implemented using atomic state flags (`std::atomic<bool>`) and standard Windows critical sections. It can be invoked multiple times across different threads without performance cost or race conditions:

```cpp
void NvidiaReflexBlock::Apply() {
    bool expected = false;
    if (!s_applied.compare_exchange_strong(expected, true)) {
        return; // Already active; zero overhead
    }

    for (const auto& var : kSuppressionVars) {
        SetEnvironmentVariableA(var.name, var.value);
    }
}
```

---

## 4. Verification & Testing

The implementation is verified via `bin/test_nvidia_reflex_block.exe`:
- **Variable Verification:** Asserts all suppression environment variables are actively set in process memory.
- **Idempotence Check:** Asserts that calling `Apply()` across multiple worker threads completes in $< 1$ ms with zero errors.
- **Inert Baseline:** In environments without NVIDIA hardware, the variables are set harmlessly without side effects on Intel, AMD, or software rasterizers.
