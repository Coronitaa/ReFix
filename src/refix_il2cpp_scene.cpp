#include "refix_il2cpp_scene.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixIL2CPPScene {

namespace {

// Il2CppObject header is { klass, monitor }; boxed value types and string/array payloads follow it.
constexpr size_t kObjectHeaderSize = sizeof(void*) * 2;
constexpr size_t kStringLengthOffset = kObjectHeaderSize;            // int32 length
constexpr size_t kStringCharsOffset = kObjectHeaderSize + 4;         // UTF-16 chars
constexpr size_t kArrayLengthOffset = kObjectHeaderSize + sizeof(void*);      // after 'bounds'
constexpr size_t kArrayDataOffset = kObjectHeaderSize + sizeof(void*) * 2;
constexpr int kMaxFlowTreeDepth = 8;
constexpr int kMaxFlowTreeNodes = 96;
constexpr int kMaxListItems = 32;
constexpr int kCullingMaskEverything = -1;
constexpr int kUiLayerMask = 1 << 5;          // Unity built-in "UI" layer
constexpr int kFieldAttributeStatic = 0x0010; // FIELD_ATTRIBUTE_STATIC

struct Api {
    void* (*domain_get)() = nullptr;
    void* (*thread_attach)(void*) = nullptr;
    void** (*domain_get_assemblies)(void*, size_t*) = nullptr;
    void* (*assembly_get_image)(void*) = nullptr;
    const char* (*image_get_name)(void*) = nullptr;
    void* (*class_from_name)(void*, const char*, const char*) = nullptr;
    void* (*class_get_method_from_name)(void*, const char*, int) = nullptr;
    void* (*runtime_invoke)(void*, void*, void**, void**) = nullptr;
    void* (*class_get_type)(void*) = nullptr;
    void* (*type_get_object)(void*) = nullptr;
    void* (*object_get_class)(void*) = nullptr;
    const char* (*class_get_name)(void*) = nullptr;
    const char* (*class_get_namespace)(void*) = nullptr;
    void* (*class_get_image)(void*) = nullptr;
    void* (*class_get_parent)(void*) = nullptr;
    void* (*class_get_field_from_name)(void*, const char*) = nullptr;
    void (*field_get_value)(void*, void*, void*) = nullptr;
    void (*field_static_get_value)(void*, void*) = nullptr;
    void (*field_static_set_value)(void*, void*) = nullptr;
    int (*field_get_flags)(void*) = nullptr;
    void* (*string_new)(const char*) = nullptr;
    uint32_t (*gchandle_new)(void*, bool) = nullptr;
    bool ready = false;
};

Api g_api;
std::once_flag g_apiOnce;

template <typename T>
void Resolve(HMODULE mod, T& out, const char* name) {
    out = reinterpret_cast<T>(GetProcAddress(mod, name));
}

const Api* GetApi() {
    std::call_once(g_apiOnce, [] {
        HMODULE ga = GetModuleHandleA("GameAssembly.dll");
        if (!ga) return;
        Resolve(ga, g_api.domain_get, "il2cpp_domain_get");
        Resolve(ga, g_api.thread_attach, "il2cpp_thread_attach");
        Resolve(ga, g_api.domain_get_assemblies, "il2cpp_domain_get_assemblies");
        Resolve(ga, g_api.assembly_get_image, "il2cpp_assembly_get_image");
        Resolve(ga, g_api.image_get_name, "il2cpp_image_get_name");
        Resolve(ga, g_api.class_from_name, "il2cpp_class_from_name");
        Resolve(ga, g_api.class_get_method_from_name, "il2cpp_class_get_method_from_name");
        Resolve(ga, g_api.runtime_invoke, "il2cpp_runtime_invoke");
        Resolve(ga, g_api.class_get_type, "il2cpp_class_get_type");
        Resolve(ga, g_api.type_get_object, "il2cpp_type_get_object");
        Resolve(ga, g_api.object_get_class, "il2cpp_object_get_class");
        Resolve(ga, g_api.class_get_name, "il2cpp_class_get_name");
        Resolve(ga, g_api.class_get_namespace, "il2cpp_class_get_namespace");
        Resolve(ga, g_api.class_get_image, "il2cpp_class_get_image");
        Resolve(ga, g_api.class_get_parent, "il2cpp_class_get_parent");
        Resolve(ga, g_api.class_get_field_from_name, "il2cpp_class_get_field_from_name");
        Resolve(ga, g_api.field_get_value, "il2cpp_field_get_value");
        Resolve(ga, g_api.field_static_get_value, "il2cpp_field_static_get_value");
        Resolve(ga, g_api.field_static_set_value, "il2cpp_field_static_set_value");
        Resolve(ga, g_api.field_get_flags, "il2cpp_field_get_flags");
        Resolve(ga, g_api.string_new, "il2cpp_string_new");
        Resolve(ga, g_api.gchandle_new, "il2cpp_gchandle_new");
        g_api.ready = g_api.domain_get && g_api.thread_attach && g_api.domain_get_assemblies &&
                      g_api.assembly_get_image && g_api.class_from_name && g_api.class_get_method_from_name &&
                      g_api.runtime_invoke && g_api.object_get_class && g_api.class_get_name &&
                      g_api.class_get_namespace;
    });
    if (!g_api.ready) return nullptr;
    void* domain = g_api.domain_get();
    if (!domain) return nullptr;
    g_api.thread_attach(domain);
    return &g_api;
}

// --- SEH-guarded primitives (no C++ objects with destructors allowed in these frames) ---

void* SafeInvoke(const Api* api, void* method, void* obj, void** args, bool* ok) {
    *ok = false;
    if (!method || !*reinterpret_cast<void**>(method)) return nullptr;
    void* exc = nullptr;
    void* res = nullptr;
    __try {
        res = api->runtime_invoke(method, obj, args, &exc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    *ok = (exc == nullptr);
    return *ok ? res : nullptr;
}

bool IsStaticField(const Api* api, void* field) {
    return api->field_get_flags && (api->field_get_flags(field) & kFieldAttributeStatic) != 0;
}

// Instance fields only: il2cpp_field_get_value on a static field reads garbage relative to obj.
bool SafeGetField(const Api* api, void* obj, const char* fieldName, void* out) {
    if (!obj || !api->class_get_field_from_name || !api->field_get_value || !api->field_get_flags) return false;
    void* klass = api->object_get_class(obj);
    if (!klass) return false;
    void* field = api->class_get_field_from_name(klass, fieldName);
    if (!field || IsStaticField(api, field)) return false;
    __try {
        api->field_get_value(obj, field, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool SafeGetStaticField(const Api* api, void* klass, const char* fieldName, void* out) {
    if (!klass || !api->class_get_field_from_name || !api->field_static_get_value || !api->field_get_flags) return false;
    void* field = api->class_get_field_from_name(klass, fieldName);
    if (!field || !IsStaticField(api, field)) return false;
    __try {
        api->field_static_get_value(field, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool SafeSetStaticField(const Api* api, void* klass, const char* fieldName, void* value) {
    if (!klass || !api->class_get_field_from_name || !api->field_static_set_value || !api->field_get_flags) return false;
    void* field = api->class_get_field_from_name(klass, fieldName);
    if (!field || !IsStaticField(api, field)) return false;
    __try {
        api->field_static_set_value(field, value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool SafeReadPtr(const void* addr, void** out) {
    __try {
        *out = *reinterpret_cast<void* const*>(addr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

// --- Typed call helpers ---

void* FindClass(const Api* api, const char* ns, const char* name) {
    void* domain = api->domain_get();
    size_t count = 0;
    void** assemblies = api->domain_get_assemblies(domain, &count);
    if (!assemblies) return nullptr;
    for (size_t i = 0; i < count; i++) {
        void* img = api->assembly_get_image(assemblies[i]);
        if (!img) continue;
        void* klass = api->class_from_name(img, ns, name);
        if (klass) return klass;
    }
    return nullptr;
}

void* MethodOf(const Api* api, void* obj, const char* name, int argc) {
    if (!obj) return nullptr;
    void* klass = api->object_get_class(obj);
    return klass ? api->class_get_method_from_name(klass, name, argc) : nullptr;
}

void* CallObject(const Api* api, void* obj, const char* name) {
    bool ok = false;
    return SafeInvoke(api, MethodOf(api, obj, name, 0), obj, nullptr, &ok);
}

// Returns false when the getter could not be invoked; boxed results are unboxed from the object payload.
template <typename T>
bool CallValue(const Api* api, void* obj, const char* name, T* out) {
    bool ok = false;
    void* boxed = SafeInvoke(api, MethodOf(api, obj, name, 0), obj, nullptr, &ok);
    if (!ok || !boxed) return false;
    std::memcpy(out, reinterpret_cast<uint8_t*>(boxed) + kObjectHeaderSize, sizeof(T));
    return true;
}

template <typename T>
bool CallSetter(const Api* api, void* obj, const char* name, T value) {
    bool ok = false;
    void* args[1] = { &value };
    SafeInvoke(api, MethodOf(api, obj, name, 1), obj, args, &ok);
    return ok;
}

std::string ToUtf8(void* il2cppString) {
    if (!il2cppString) return "<null>";
    const uint8_t* base = reinterpret_cast<const uint8_t*>(il2cppString);
    int32_t len = *reinterpret_cast<const int32_t*>(base + kStringLengthOffset);
    if (len <= 0) return "";
    const wchar_t* chars = reinterpret_cast<const wchar_t*>(base + kStringCharsOffset);
    int bytes = WideCharToMultiByte(CP_UTF8, 0, chars, len, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, chars, len, out.data(), bytes, nullptr, nullptr);
    return out;
}

std::string ClassName(const Api* api, void* obj) {
    if (!obj) return "<null>";
    void* klass = api->object_get_class(obj);
    if (!klass) return "<no-class>";
    const char* ns = api->class_get_namespace(klass);
    const char* nm = api->class_get_name(klass);
    std::string full = (ns && *ns) ? std::string(ns) + "." : std::string();
    return full + (nm ? nm : "?");
}

std::vector<void*> ReadObjectArray(void* array) {
    std::vector<void*> items;
    if (!array) return items;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(array);
    void* lenRaw = nullptr;
    if (!SafeReadPtr(base + kArrayLengthOffset, &lenRaw)) return items;
    size_t len = reinterpret_cast<size_t>(lenRaw);
    void* const* data = reinterpret_cast<void* const*>(base + kArrayDataOffset);
    for (size_t i = 0; i < len; i++) {
        void* item = nullptr;
        if (SafeReadPtr(&data[i], &item) && item) items.push_back(item);
    }
    return items;
}

struct CameraInfo {
    void* camera = nullptr;
    void* gameObject = nullptr;
    std::string name;
    std::string tag;
    bool enabled = false;
    bool activeInHierarchy = false;
    bool activeSelf = false;
    bool hasTargetTexture = false;
    float depth = 0.0f;
    int cullingMask = 0;
    int clearFlags = 0;

    bool RendersToScreen() const { return enabled && activeInHierarchy && !hasTargetTexture; }
    // A UI-only overlay camera reaches the backbuffer but cannot show a 3D scene.
    bool CanRenderWorld() const { return cullingMask == 0 || (cullingMask & ~kUiLayerMask) != 0; }
    bool RendersWorldToScreen() const { return RendersToScreen() && cullingMask != 0 && CanRenderWorld(); }
};

std::vector<void*> FindAllCameras(const Api* api) {
    void* klassCamera = FindClass(api, "UnityEngine", "Camera");
    void* klassObject = FindClass(api, "UnityEngine", "Object");
    if (!klassCamera || !klassObject || !api->class_get_type || !api->type_get_object) return {};
    void* typeObj = api->type_get_object(api->class_get_type(klassCamera));
    if (!typeObj) return {};

    // FindObjectsOfType(Type, bool includeInactive) also returns disabled components / inactive GameObjects,
    // unlike Camera.allCameras which only lists enabled cameras.
    bool includeInactive = true;
    void* args[2] = { typeObj, &includeInactive };
    bool ok = false;
    void* arr = SafeInvoke(api, api->class_get_method_from_name(klassObject, "FindObjectsOfType", 2),
                           nullptr, args, &ok);
    if (!ok) {
        void* args1[1] = { typeObj };
        arr = SafeInvoke(api, api->class_get_method_from_name(klassObject, "FindObjectsOfType", 1),
                         nullptr, args1, &ok);
    }
    return ok ? ReadObjectArray(arr) : std::vector<void*>{};
}

CameraInfo DescribeCamera(const Api* api, void* cam) {
    CameraInfo info;
    info.camera = cam;
    info.name = ToUtf8(CallObject(api, cam, "get_name"));
    info.gameObject = CallObject(api, cam, "get_gameObject");
    info.tag = ToUtf8(CallObject(api, info.gameObject, "get_tag"));
    CallValue(api, cam, "get_enabled", &info.enabled);
    CallValue(api, info.gameObject, "get_activeInHierarchy", &info.activeInHierarchy);
    CallValue(api, info.gameObject, "get_activeSelf", &info.activeSelf);
    CallValue(api, cam, "get_depth", &info.depth);
    CallValue(api, cam, "get_cullingMask", &info.cullingMask);
    CallValue(api, cam, "get_clearFlags", &info.clearFlags);
    info.hasTargetTexture = CallObject(api, cam, "get_targetTexture") != nullptr;
    return info;
}

std::vector<CameraInfo> CollectCameras(const Api* api) {
    std::vector<CameraInfo> out;
    for (void* cam : FindAllCameras(api)) out.push_back(DescribeCamera(api, cam));
    return out;
}

const CameraInfo* PickScreenCameraCandidate(const std::vector<CameraInfo>& cams) {
    const CameraInfo* best = nullptr;
    for (const CameraInfo& c : cams) {
        if (c.hasTargetTexture || !c.CanRenderWorld()) continue;
        if (!best) { best = &c; continue; }
        bool cMain = (c.tag == "MainCamera");
        bool bMain = (best->tag == "MainCamera");
        if (cMain != bMain) { if (cMain) best = &c; continue; }
        if (c.depth > best->depth) best = &c;
    }
    return best;
}

void LogFlowNodeRecursive(const Api* api, void* node, int depth, int* budget) {
    if (!node || depth > kMaxFlowTreeDepth || (*budget)-- <= 0) return;

    std::string indent(static_cast<size_t>(depth) * 2, ' ');
    bool isOk = false;
    bool hasIsOk = SafeGetField(api, node, "isOk", &isOk);
    std::string nodeName = ToUtf8(CallObject(api, node, "get_nodeName"));
    ReFixLog("[IL2CPP-Scene]   %s- %s name='%s'%s", indent.c_str(), ClassName(api, node).c_str(), nodeName.c_str(),
             hasIsOk ? (isOk ? " isOk=true" : " isOk=false") : "");

    void* child = nullptr;
    if (SafeGetField(api, node, "m_Child", &child) && child) {
        LogFlowNodeRecursive(api, child, depth + 1, budget);
    }

    void* list = nullptr;
    if (!SafeGetField(api, node, "m_Children", &list) || !list) return;
    void* items = nullptr;
    int size = 0;
    if (!SafeGetField(api, list, "_items", &items) || !SafeGetField(api, list, "_size", &size)) return;
    std::vector<void*> children = ReadObjectArray(items);
    int limit = (size < kMaxListItems) ? size : kMaxListItems;
    for (int i = 0; i < limit && i < static_cast<int>(children.size()); i++) {
        if (children[i] != child) LogFlowNodeRecursive(api, children[i], depth + 1, budget);
    }
}

} // namespace

void LogCameraInventory(const char* tag) {
    const Api* api = GetApi();
    if (!api) return;
    std::vector<CameraInfo> cams = CollectCameras(api);
    ReFixLog("[IL2CPP-Scene] Camera inventory (%s): %zu camera(s)", tag ? tag : "", cams.size());
    for (const CameraInfo& c : cams) {
        ReFixLog("[IL2CPP-Scene]   cam=%p name='%s' tag='%s' enabled=%d activeSelf=%d activeInHierarchy=%d "
                 "depth=%.1f cullingMask=0x%08X clearFlags=%d targetTexture=%s%s",
                 c.camera, c.name.c_str(), c.tag.c_str(), c.enabled, c.activeSelf, c.activeInHierarchy,
                 c.depth, static_cast<unsigned>(c.cullingMask), c.clearFlags,
                 c.hasTargetTexture ? "yes" : "no",
                 c.RendersWorldToScreen() ? "  <== WORLD->SCREEN" : (c.RendersToScreen() ? "  <== UI->SCREEN" : ""));
    }
}

bool EnsureScreenCamera() {
    const Api* api = GetApi();
    if (!api) return false;
    std::vector<CameraInfo> cams = CollectCameras(api);
    for (const CameraInfo& c : cams) {
        if (c.RendersWorldToScreen()) return true;
    }

    const CameraInfo* pick = PickScreenCameraCandidate(cams);
    if (!pick) {
        ReFixLog("[IL2CPP-Scene] No world camera renders to screen and no world-capable candidate exists (%zu camera(s) total) "
                 "- the 3D scene is most likely not loaded", cams.size());
        return false;
    }

    ReFixLog("[IL2CPP-Scene] No world camera renders to screen; activating candidate '%s' (tag='%s')",
             pick->name.c_str(), pick->tag.c_str());
    if (!pick->enabled) CallSetter(api, pick->camera, "set_enabled", true);
    if (!pick->activeSelf && pick->gameObject) CallSetter(api, pick->gameObject, "SetActive", true);
    if (pick->cullingMask == 0) CallSetter(api, pick->camera, "set_cullingMask", kCullingMaskEverything);

    CameraInfo after = DescribeCamera(api, pick->camera);
    ReFixLog("[IL2CPP-Scene] Candidate '%s' now enabled=%d activeInHierarchy=%d%s", after.name.c_str(),
             after.enabled, after.activeInHierarchy,
             after.activeInHierarchy ? "" : " (a parent GameObject is inactive)");
    return after.RendersWorldToScreen();
}

void LogFlowNodeTree(const char* label, void* rootNode) {
    const Api* api = GetApi();
    if (!api) return;
    ReFixLog("[IL2CPP-Scene] Flow tree '%s' root=%p", label ? label : "", rootNode);
    int budget = kMaxFlowTreeNodes;
    LogFlowNodeRecursive(api, rootNode, 0, &budget);
}

void LogObjectClass(const char* label, void* obj) {
    const Api* api = GetApi();
    if (!api || !obj) return;
    void* klass = api->object_get_class(obj);
    void* image = (klass && api->class_get_image) ? api->class_get_image(klass) : nullptr;
    const char* imageName = (image && api->image_get_name) ? api->image_get_name(image) : "?";
    ReFixLog("[IL2CPP-Scene] %s: %p is %s (image=%s)", label ? label : "object", obj,
             ClassName(api, obj).c_str(), imageName ? imageName : "?");
}

void* FindClassByName(const char* ns, const char* className) {
    const Api* api = GetApi();
    return api ? FindClass(api, ns, className) : nullptr;
}

bool GetStaticFieldValue(void* klass, const char* fieldName, void* out) {
    const Api* api = GetApi();
    return api && SafeGetStaticField(api, klass, fieldName, out);
}

bool SetStaticFieldValue(void* klass, const char* fieldName, void* value) {
    const Api* api = GetApi();
    return api && SafeSetStaticField(api, klass, fieldName, value);
}

bool SetStaticReference(void* klass, const char* fieldName, void* object) {
    // il2cpp_field_static_set_value stores a reference-type value as given (the object pointer itself),
    // unlike value types where `value` points to the data to copy.
    const Api* api = GetApi();
    return api && SafeSetStaticField(api, klass, fieldName, object);
}

void* FindFirstObjectOfType(const char* ns, const char* className) {
    const Api* api = GetApi();
    if (!api) return nullptr;
    void* klass = FindClass(api, ns, className);
    void* klassObject = FindClass(api, "UnityEngine", "Object");
    if (!klass || !klassObject || !api->class_get_type || !api->type_get_object) return nullptr;
    void* typeObj = api->type_get_object(api->class_get_type(klass));
    bool includeInactive = true;
    void* args[2] = { typeObj, &includeInactive };
    bool ok = false;
    void* arr = SafeInvoke(api, api->class_get_method_from_name(klassObject, "FindObjectsOfType", 2), nullptr, args, &ok);
    std::vector<void*> objects = ok ? ReadObjectArray(arr) : std::vector<void*>{};
    return objects.empty() ? nullptr : objects.front();
}

void* NewPinnedString(const char* utf8) {
    const Api* api = GetApi();
    if (!api || !api->string_new || !api->gchandle_new || !utf8) return nullptr;
    void* str = api->string_new(utf8);
    // Strong GC handle, intentionally never freed: native code stores the string in a static, and the
    // incremental GC must never reclaim it regardless of how that store is tracked.
    if (str) api->gchandle_new(str, false);
    return str;
}

bool InvokeStaticBoolSetter(void* klass, const char* methodName, bool value) {
    const Api* api = GetApi();
    if (!api || !klass) return false;
    void* args[1] = { &value };
    bool ok = false;
    SafeInvoke(api, api->class_get_method_from_name(klass, methodName, 1), nullptr, args, &ok);
    return ok;
}

bool InvokeStaticBoolGetter(void* klass, const char* methodName, bool* out) {
    const Api* api = GetApi();
    if (!api || !klass) return false;
    bool ok = false;
    void* boxed = SafeInvoke(api, api->class_get_method_from_name(klass, methodName, 0), nullptr, nullptr, &ok);
    if (!ok || !boxed) return false;
    *out = *(reinterpret_cast<uint8_t*>(boxed) + kObjectHeaderSize) != 0;
    return true;
}

void* GetMethodCode(void* klass, const char* methodName, int argc) {
    const Api* api = GetApi();
    if (!api || !klass) return nullptr;
    void* method = api->class_get_method_from_name(klass, methodName, argc);
    return method ? *reinterpret_cast<void**>(method) : nullptr;
}

std::string ObjectToDisplayString(void* obj) {
    const Api* api = GetApi();
    if (!api || !obj) return "<null>";
    std::string cls = ClassName(api, obj);
    if (cls == "System.String") return ToUtf8(obj);
    // Exceptions and other objects: managed ToString() (message + stack trace for exceptions).
    void* text = CallObject(api, obj, "ToString");
    return text ? "<" + cls + "> " + ToUtf8(text) : "<" + cls + ">";
}

void* GetParentClass(void* klass) {
    const Api* api = GetApi();
    return (api && klass && api->class_get_parent) ? api->class_get_parent(klass) : nullptr;
}

bool GetFieldValue(void* obj, const char* fieldName, void* out) {
    const Api* api = GetApi();
    return api && SafeGetField(api, obj, fieldName, out);
}

void* CallObjectGetter(void* obj, const char* getter) {
    const Api* api = GetApi();
    return api ? CallObject(api, obj, getter) : nullptr;
}

bool CallBoolGetter(void* obj, const char* getter, bool* out) {
    const Api* api = GetApi();
    return api && CallValue(api, obj, getter, out);
}

bool CallInt32Getter(void* obj, const char* getter, int32_t* out) {
    const Api* api = GetApi();
    return api && CallValue(api, obj, getter, out);
}

void LogStaticReference(const char* ns, const char* className, const char* fieldName) {
    const Api* api = GetApi();
    if (!api) return;
    void* klass = FindClass(api, ns, className);
    if (!klass) {
        ReFixLog("[IL2CPP-Scene] static %s.%s::%s -> class not loaded", ns, className, fieldName);
        return;
    }
    void* value = nullptr;
    if (!SafeGetStaticField(api, klass, fieldName, &value)) {
        ReFixLog("[IL2CPP-Scene] static %s.%s::%s -> not a readable static field", ns, className, fieldName);
        return;
    }
    ReFixLog("[IL2CPP-Scene] static %s.%s::%s = %p (%s)", ns, className, fieldName, value, ClassName(api, value).c_str());
}

void LogClassPresence(const char* ns, const char* className) {
    const Api* api = GetApi();
    if (!api) return;
    void* klass = FindClass(api, ns, className);
    void* image = (klass && api->class_get_image) ? api->class_get_image(klass) : nullptr;
    const char* imageName = (image && api->image_get_name) ? api->image_get_name(image) : nullptr;
    ReFixLog("[IL2CPP-Scene] class %s.%s -> %s%s%s", ns, className, klass ? "present in " : "NOT FOUND",
             imageName ? imageName : "", "");
}

void LogManagedAssemblies() {
    const Api* api = GetApi();
    if (!api) return;
    void* klassDomain = FindClass(api, "System", "AppDomain");
    if (!klassDomain) return;
    bool ok = false;
    void* domain = SafeInvoke(api, api->class_get_method_from_name(klassDomain, "get_CurrentDomain", 0), nullptr, nullptr, &ok);
    void* arr = CallObject(api, domain, "GetAssemblies");
    std::vector<void*> assemblies = ReadObjectArray(arr);
    size_t imageCount = 0;
    void** images = api->domain_get_assemblies(api->domain_get(), &imageCount);
    ReFixLog("[IL2CPP-Scene] AppDomain.GetAssemblies(): %zu managed assemblies, %zu IL2CPP images", assemblies.size(), imageCount);
    // Assemblies without a matching AOT image were loaded at runtime (e.g. HybridCLR hot-update DLLs).
    for (void* a : assemblies) {
        std::string fullName = ToUtf8(CallObject(api, a, "get_FullName"));
        std::string shortName = fullName.substr(0, fullName.find(',')) + ".dll";
        bool isAot = false;
        for (size_t i = 0; i < imageCount && !isAot && images; i++) {
            void* img = api->assembly_get_image(images[i]);
            const char* n = (img && api->image_get_name) ? api->image_get_name(img) : nullptr;
            isAot = n && shortName == n;
        }
        if (!isAot) ReFixLog("[IL2CPP-Scene]   runtime-loaded assembly: %s", fullName.c_str());
    }
}

std::string StringToUtf8(void* il2cppString) {
    return ToUtf8(il2cppString);
}

void LogLoadedAssemblies() {
    const Api* api = GetApi();
    if (!api || !api->image_get_name) return;
    size_t count = 0;
    void** assemblies = api->domain_get_assemblies(api->domain_get(), &count);
    if (!assemblies) return;
    ReFixLog("[IL2CPP-Scene] %zu assemblies loaded in domain:", count);
    for (size_t i = 0; i < count; i++) {
        void* img = api->assembly_get_image(assemblies[i]);
        const char* name = img ? api->image_get_name(img) : nullptr;
        ReFixLog("[IL2CPP-Scene]   [%zu] %s", i, name ? name : "?");
    }
}

} // namespace ReFixIL2CPPScene
