#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

// Generic Unity/IL2CPP scene diagnostics and render-safety helpers.
// Self-contained: resolves the IL2CPP C API from GameAssembly.dll on first use,
// so it can be reused by any IL2CPP title. All calls must run on a thread that
// is allowed to touch Unity objects (the Unity main thread).
namespace ReFixIL2CPPScene {
    // Logs every Camera in the loaded scenes (including disabled/inactive ones)
    // with name, tag, enabled, activeInHierarchy, depth, cullingMask, clearFlags and targetTexture.
    void LogCameraInventory(const char* tag);

    // Ensures at least one world camera (culling more than the UI layer) renders to the backbuffer.
    // If none does, enables the best candidate (tag MainCamera first, then highest depth) that has
    // no targetTexture. Returns true when such a camera is (now) active.
    bool EnsureScreenCamera();

    // Logs the runtime class of every node reachable from rootNode through m_Root / m_Child / m_Children.
    void LogFlowNodeTree(const char* label, void* rootNode);

    // Logs "<namespace>.<name>" of obj's runtime class and the image (assembly) it lives in.
    void LogObjectClass(const char* label, void* obj);

    // Resolves a class across every loaded image (including runtime-loaded hot-update assemblies).
    void* FindClassByName(const char* ns, const char* className);

    // Reads / writes a static field by name. Fail (return false) for missing or non-static fields.
    // Get: `out` receives the raw field value (int32_t* for an enum, void** for a reference).
    // SetStaticFieldValue: value-type fields only; `value` points to the data (e.g. int32_t*, bool*).
    // SetStaticReference: reference-type fields; pass the object pointer itself (may be nullptr).
    bool GetStaticFieldValue(void* klass, const char* fieldName, void* out);
    bool SetStaticFieldValue(void* klass, const char* fieldName, void* value);
    bool SetStaticReference(void* klass, const char* fieldName, void* object);

    // First UnityEngine.Object of the given type in loaded scenes (including inactive); main thread only.
    void* FindFirstObjectOfType(const char* ns, const char* className);

    // Allocates a managed System.String (only safe to use immediately, e.g. as a call argument).
    void* NewString(const char* utf8);

    // Allocates a managed System.String from UTF-8 and keeps it alive for the process lifetime (strong GC handle).
    void* NewPinnedString(const char* utf8);

    // Invoke a static bool setter / getter (e.g. set_isLanMode). False when missing or it throws.
    bool InvokeStaticBoolSetter(void* klass, const char* methodName, bool value);
    bool InvokeStaticBoolGetter(void* klass, const char* methodName, bool* out);

    // Native code pointer of klass::methodName with argc parameters (nullptr if missing / not compiled).
    void* GetMethodCode(void* klass, const char* methodName, int argc);

    // UTF-8 text of a System.String; other objects as "<Namespace.Class> " + ToString().
    std::string ObjectToDisplayString(void* obj);

    // Allocates an instance of klass and runs its parameterless constructor (nullptr on failure).
    void* NewObject(void* klass);

    // Writes an instance field. SetFieldValue: value types, `value` points to the data (&int32, &bool...).
    // SetFieldReference: reference-type fields, pass the object pointer itself (may be nullptr).
    bool SetFieldValue(void* obj, const char* fieldName, void* value);
    bool SetFieldReference(void* obj, const char* fieldName, void* object);

    // Parent class (e.g. the generic Singleton<T> instantiation a type derives from).
    void* GetParentClass(void* klass);

    // Reads an instance field by name (false for missing or static fields).
    bool GetFieldValue(void* obj, const char* fieldName, void* out);

    // Invokes a parameterless instance getter; false (or nullptr) when it cannot be called or throws.
    void* CallObjectGetter(void* obj, const char* getter);
    bool CallBoolGetter(void* obj, const char* getter, bool* out);
    bool CallInt32Getter(void* obj, const char* getter, int32_t* out);

    // Logs the value (address + runtime class) of a static reference field, e.g. a registration hook.
    void LogStaticReference(const char* ns, const char* className, const char* fieldName);

    // Logs whether a class is resolvable in the domain and which image provides it.
    void LogClassPresence(const char* ns, const char* className);

    // Logs managed assemblies (AppDomain.GetAssemblies) that have no matching AOT image, i.e. loaded at runtime.
    void LogManagedAssemblies();

    // Converts a System.String (Il2CppString*) to UTF-8; returns "<null>" for nullptr.
    std::string StringToUtf8(void* il2cppString);

    // Logs the names of all assemblies currently loaded in the IL2CPP domain (includes hot-update DLLs).
    void LogLoadedAssemblies();
}
