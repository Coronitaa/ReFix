using System;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using Mono.Cecil;
using Mono.Cecil.Cil;

namespace ReFixFusionShared;

// Fusion.Runtime is built against UnityEngine.CoreModule, whose engine calls (InternalCall methods) only exist
// inside the Unity player. CoreModule is kept out of the application folder and loaded here instead, rewritten so
// those methods have managed bodies: the memory functions Fusion relies on get real implementations
// (UnityNative), every other engine call logs once and returns its default value.
static class UnityRuntime {
    const string CoreModule = "UnityEngine.CoreModule";

    public static void Install() {
        AssemblyLoadContext.Default.Resolving += (context, name) => {
            if (name.Name != CoreModule) return null;
            string path = Path.Combine(AppContext.BaseDirectory, "unity", CoreModule + ".dll");
            byte[] image = Rewrite(path);
            if (Environment.GetEnvironmentVariable("REFIX_DUMP_UNITY") is string dump) File.WriteAllBytes(dump, image);
            return context.LoadFromStream(new MemoryStream(image));
        };
    }

    static byte[] Rewrite(string path) {
        using var resolver = new DefaultAssemblyResolver();
        resolver.AddSearchDirectory(AppContext.BaseDirectory);
        resolver.AddSearchDirectory(Path.GetDirectoryName(path));
        using AssemblyDefinition assembly = AssemblyDefinition.ReadAssembly(path, new ReaderParameters { AssemblyResolver = resolver });
        ModuleDefinition module = assembly.MainModule;
        MethodReference unsupported = module.ImportReference(typeof(UnityNative).GetMethod(nameof(UnityNative.Unsupported)));
        foreach (TypeDefinition type in module.GetTypes()) {
            foreach (MethodDefinition method in type.Methods) {
                if (!method.IsInternalCall && !(method.IsPInvokeImpl)) continue;
                method.IsInternalCall = false;
                method.PInvokeInfo = null;
                method.Attributes &= ~Mono.Cecil.MethodAttributes.PInvokeImpl;
                method.ImplAttributes = Mono.Cecil.MethodImplAttributes.IL | Mono.Cecil.MethodImplAttributes.Managed;
                method.Body = new Mono.Cecil.Cil.MethodBody(method);
                ILProcessor il = method.Body.GetILProcessor();
                MethodInfo shim = FindShim(type.Name, method);
                if (shim != null) {
                    for (int i = 0; i < method.Parameters.Count; i++) il.Emit(OpCodes.Ldarg, method.Parameters[i]);
                    il.Emit(OpCodes.Call, module.ImportReference(shim));
                    il.Emit(OpCodes.Ret);
                    continue;
                }
                il.Emit(OpCodes.Ldstr, type.FullName + "::" + method.Name);
                il.Emit(OpCodes.Call, unsupported);
                EmitDefaultReturn(method, il);
            }
        }
        using var output = new MemoryStream();
        assembly.Write(output);
        return output.ToArray();
    }

    static MethodInfo FindShim(string typeName, MethodDefinition method) {
        if (typeName != "UnsafeUtility" || method.HasGenericParameters || !method.IsStatic) return null;
        return typeof(UnityNative).GetMethods(BindingFlags.Public | BindingFlags.Static)
            .FirstOrDefault(m => m.Name == method.Name && m.GetParameters().Length == method.Parameters.Count &&
                                 m.GetParameters().Select(p => p.ParameterType.Name)
                                  .SequenceEqual(method.Parameters.Select(p => ShimTypeName(p.ParameterType))));
    }

    // Unity's Allocator enum becomes an int in the shim signatures.
    static string ShimTypeName(TypeReference type) => type.Resolve()?.IsEnum == true ? nameof(Int32) : type.Name;

    static void EmitDefaultReturn(MethodDefinition method, ILProcessor il) {
        TypeReference ret = method.ReturnType;
        if (ret.MetadataType == MetadataType.Void) {
            il.Emit(OpCodes.Ret);
            return;
        }
        var local = new VariableDefinition(ret);
        method.Body.Variables.Add(local);
        method.Body.InitLocals = true;
        il.Emit(OpCodes.Ldloc, local);
        il.Emit(OpCodes.Ret);
    }
}

public static unsafe class UnityNative {
    static readonly ConcurrentDictionary<string, bool> s_reported = new ConcurrentDictionary<string, bool>();

    public static void Unsupported(string method) {
        if (s_reported.TryAdd(method, true)) Console.WriteLine($"[unity] engine call {method} not available, using default");
    }

    public static void MemCpy(void* destination, void* source, long size) => Buffer.MemoryCopy(source, destination, size, size);
    public static void MemMove(void* destination, void* source, long size) => Buffer.MemoryCopy(source, destination, size, size);
    public static void MemClear(void* destination, long size) => NativeMemory.Clear(destination, (nuint)size);
    public static void MemSet(void* destination, byte value, long size) => NativeMemory.Fill(destination, (nuint)size, value);
    public static int MemCmp(void* ptr1, void* ptr2, long size) =>
        new ReadOnlySpan<byte>(ptr1, checked((int)size)).SequenceCompareTo(new ReadOnlySpan<byte>(ptr2, checked((int)size)));
    public static void* Malloc(long size, int alignment, int allocator) =>
        NativeMemory.AlignedAlloc((nuint)size, (nuint)Math.Max(alignment, sizeof(void*)));
    public static void Free(void* memory, int allocator) => NativeMemory.AlignedFree(memory);
    public static int SizeOf(Type type) =>
        (int)typeof(Unsafe).GetMethod(nameof(Unsafe.SizeOf)).MakeGenericMethod(type).Invoke(null, null);
    public static int GetFieldOffset(FieldInfo field) => (int)Marshal.OffsetOf(field.DeclaringType, field.Name);
}
