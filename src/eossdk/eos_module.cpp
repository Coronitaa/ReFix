// ReFix EOS v3 - module entry point and export dispatch.
//
// The DLL exports all 679 EOS symbols through hand-written trampolines in
// src/eos_fwd.asm, each of which jumps through slot `i` of g_eosProcs. This file
// owns that table:
//
//   * every slot starts pointing at a per-export stub that logs the call and
//     returns a value that is *safe for the return type* (from the generated
//     ABI table) instead of a blanket zero. Returning EOS_Success from an
//     unimplemented Copy* while leaving its out-parameter untouched is the
//     single most common way an EOS emulator crashes its host, so unimplemented
//     entry points report EOS_NotFound and unimplemented async entry points
//     still answer their completion delegate.
//   * subsystems then overwrite the slots they actually implement.
#include "core/refix_common.h"
#include "core/refix_config.h"
#include "core/refix_log.h"
#include "core/eos_dispatch.h"
#include "eos_module.h"

namespace refix {

// ---------------------------------------------------------------------------
// Generated ABI description
// ---------------------------------------------------------------------------
enum ReturnKind { RK_VOID, RK_RESULT, RK_PTR, RK_NOTIFY, RK_BOOL, RK_INT, RK_ENUM };

struct ExportInfo {
    const char* Name;
    ReturnKind  Return;
    int         AsyncDelegateArg;   // index of the completion delegate, or -1
    int         ArgCount;
};

#define REFIX_EOS_EXPORT_TABLE_BEGIN static const ExportInfo kExports[] = {
#define REFIX_EOS_EXPORT(idx, name, ret, slot, argc) { name, ret, slot, argc },
#define REFIX_EOS_EXPORT_TABLE_END };
#include "gen/eos_export_table.inc"
#undef REFIX_EOS_EXPORT_TABLE_BEGIN
#undef REFIX_EOS_EXPORT
#undef REFIX_EOS_EXPORT_TABLE_END

static const int kExportCount = (int)(sizeof(kExports) / sizeof(kExports[0]));

// The table must stay in lock-step with src/eos_fwd.asm and src/eos_proxy.def:
// slot i of g_eosProcs is what stub i jumps through.
static_assert(sizeof(kExports) / sizeof(kExports[0]) == 679,
              "export table and eos_fwd.asm disagree on the export count");

const char* ExportName(int index) {
    return (index >= 0 && index < kExportCount) ? kExports[index].Name : "<out-of-range>";
}

int ExportCount() { return kExportCount; }

// True when the generated table really is strcmp-ordered, which is what makes
// the binary search below valid. Checked once so a regenerated name list that
// changed ordering degrades to a linear scan instead of silently mis-binding.
static bool TableIsSorted() {
    static const bool sorted = [] {
        for (int i = 1; i < kExportCount; i++)
            if (std::strcmp(kExports[i - 1].Name, kExports[i].Name) >= 0) return false;
        return true;
    }();
    return sorted;
}

int FindExport(const char* name) {
    if (!TableIsSorted()) {
        for (int i = 0; i < kExportCount; i++)
            if (std::strcmp(kExports[i].Name, name) == 0) return i;
        return -1;
    }
    int lo = 0, hi = kExportCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = std::strcmp(kExports[mid].Name, name);
        if (cmp == 0) return mid;
        if (cmp < 0) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

} // namespace refix

// ---------------------------------------------------------------------------
// Proc table consumed by src/eos_fwd.asm
// ---------------------------------------------------------------------------
#if defined(_MSC_VER)
#define REFIX_EXPORTED_DATA __declspec(dllexport)
#else
#define REFIX_EXPORTED_DATA
#endif

extern "C" {
    REFIX_EXPORTED_DATA void* g_eosProcs[679] = { nullptr };
}

namespace refix {
namespace {

// Each export gets its own 48-byte stub so the handler knows which function was
// called. The stub shifts the original four register arguments down one slot and
// passes the export index as the first argument.
#pragma pack(push, 1)
struct Stub {
    uint8_t  sub_rsp[4];        // sub rsp, 0x38
    uint8_t  mov_stack_r9[5];   // mov [rsp+0x20], r9   (original arg4)
    uint8_t  mov_r9_r8[3];      // mov r9, r8           (original arg3)
    uint8_t  mov_r8_rdx[3];     // mov r8, rdx          (original arg2)
    uint8_t  mov_rdx_rcx[3];    // mov rdx, rcx         (original arg1)
    uint8_t  mov_rcx_imm[2];    // mov rcx, <index>
    uint64_t index;
    uint8_t  mov_rax_imm[2];    // mov rax, <handler>
    uint64_t handler;
    uint8_t  call_rax[2];       // call rax
    uint8_t  add_rsp[4];        // add rsp, 0x38
    uint8_t  ret_[1];           // ret
    uint8_t  pad[3];
};
#pragma pack(pop)

Stub* g_stubs = nullptr;

int64_t EOS_CALL UnimplementedHandler(int64_t index, void* a1, void* a2, void* a3, void* a4) {
    const ExportInfo& e = kExports[index];
    RFLOG(Abi, "unimplemented %s(%p, %p, %p, %p)", e.Name, a1, a2, a3, a4);

    if (e.AsyncDelegateArg >= 0) {
        // (Handle, Options, ClientData, CompletionDelegate) - answer it so the
        // title's online subsystem does not wait on a request that never lands.
        Dispatcher::Get().PostGenericCompletion(a4, a3, ER::EOS_NotFound);
        return 0;
    }

    switch (e.Return) {
        case RK_RESULT: return (int64_t)ER::EOS_NotFound;
        case RK_NOTIFY: return (int64_t)Dispatcher::Get().Register(NotifyKind::Count, nullptr, a3);
        case RK_BOOL:   return EOS_FALSE;
        case RK_PTR:
        case RK_INT:
        case RK_ENUM:
        case RK_VOID:
        default:        return 0;
    }
}

void BuildStubs() {
    if (g_stubs) return;
    g_stubs = (Stub*)VirtualAlloc(nullptr, sizeof(Stub) * kExportCount,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_stubs) return;

    for (int i = 0; i < kExportCount; i++) {
        Stub& s = g_stubs[i];
        const uint8_t sub_rsp[4]      = { 0x48, 0x83, 0xEC, 0x38 };
        const uint8_t mov_stack_r9[5] = { 0x4C, 0x89, 0x4C, 0x24, 0x20 };
        const uint8_t mov_r9_r8[3]    = { 0x4D, 0x89, 0xC1 };
        const uint8_t mov_r8_rdx[3]   = { 0x49, 0x89, 0xD0 };
        const uint8_t mov_rdx_rcx[3]  = { 0x48, 0x89, 0xCA };
        const uint8_t mov_rcx_imm[2]  = { 0x48, 0xB9 };
        const uint8_t mov_rax_imm[2]  = { 0x48, 0xB8 };
        const uint8_t call_rax[2]     = { 0xFF, 0xD0 };
        const uint8_t add_rsp[4]      = { 0x48, 0x83, 0xC4, 0x38 };

        std::memcpy(s.sub_rsp,      sub_rsp,      sizeof(sub_rsp));
        std::memcpy(s.mov_stack_r9, mov_stack_r9, sizeof(mov_stack_r9));
        std::memcpy(s.mov_r9_r8,    mov_r9_r8,    sizeof(mov_r9_r8));
        std::memcpy(s.mov_r8_rdx,   mov_r8_rdx,   sizeof(mov_r8_rdx));
        std::memcpy(s.mov_rdx_rcx,  mov_rdx_rcx,  sizeof(mov_rdx_rcx));
        std::memcpy(s.mov_rcx_imm,  mov_rcx_imm,  sizeof(mov_rcx_imm));
        s.index = (uint64_t)i;
        std::memcpy(s.mov_rax_imm,  mov_rax_imm,  sizeof(mov_rax_imm));
        s.handler = (uint64_t)&UnimplementedHandler;
        std::memcpy(s.call_rax,     call_rax,     sizeof(call_rax));
        std::memcpy(s.add_rsp,      add_rsp,      sizeof(add_rsp));
        s.ret_[0] = 0xC3;
        std::memset(s.pad, 0xCC, sizeof(s.pad));
    }
}

bool g_initialised = false;

} // namespace

void Registrar::Bind(const char* name, void* fn) {
    int idx = FindExport(name);
    if (idx < 0) {
        RFLOG(Abi, "Registrar: '%s' is not an export of this SDK build", name);
        m_missing++;
        return;
    }
    g_eosProcs[idx] = fn;
    m_bound++;
}

void InitialiseModule() {
    if (g_initialised) return;
    g_initialised = true;

    BuildStubs();
    for (int i = 0; i < kExportCount; i++)
        g_eosProcs[i] = g_stubs ? (void*)&g_stubs[i] : nullptr;

    Registrar reg;
    RegisterPlatformApi(reg);
    RegisterConnectApi(reg);
    RegisterAuthApi(reg);
    RegisterUserApi(reg);
    RegisterLobbyApi(reg);
    RegisterSessionsApi(reg);
    RegisterP2PApi(reg);
    RegisterStorageApi(reg);

    RFLOG(Core, "ReFix EOS v3 online: %d exports, %d implemented, %d unresolved names "
                "(SDK headers %d.%d.%d, ini=%s)",
          kExportCount, reg.Bound(), reg.Missing(),
          EOS_MAJOR_VERSION, EOS_MINOR_VERSION, EOS_PATCH_VERSION,
          Config::Get().IniFound() ? Config::Get().IniPath().c_str() : "<not found>");
}

} // namespace refix

// Marker export used by ReFix's own deployment tooling (AutoDeploy, BlueStar)
// to recognise a ReFix-provided EOS DLL. It is declared in src/eos_proxy.def, so
// every build of this module must provide it.
extern "C" REFIX_EXPORTED_DATA int ReFix() {
    return 3;   // ReFix EOS emulator generation
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        refix::InitialiseModule();
    }
    return TRUE;
}
