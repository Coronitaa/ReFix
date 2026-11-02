using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Text.Json;

namespace ReFixFusionShared;

// Reads JSON written by UnityEngine.JsonUtility (which needs the Unity engine at run time) with Unity's rules:
// public and [SerializeField] fields, no [NonSerialized] ones, enums as numbers, missing members keep defaults.
static class UnityJson {
    public static T FromJson<T>(string json) where T : new() {
        using JsonDocument doc = JsonDocument.Parse(json);
        object target = new T();
        Fill(target, doc.RootElement);
        return (T)target;
    }

    static void Fill(object target, JsonElement obj) {
        if (obj.ValueKind != JsonValueKind.Object) return;
        for (Type type = target.GetType(); type != null && type != typeof(object); type = type.BaseType) {
            const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;
            foreach (FieldInfo field in type.GetFields(flags)) {
                if (!IsSerialized(field) || !obj.TryGetProperty(field.Name, out JsonElement value)) continue;
                object current = field.GetValue(target);
                object converted = Convert(value, field.FieldType, current);
                if (converted != null || !field.FieldType.IsValueType) field.SetValue(target, converted);
            }
        }
    }

    static bool IsSerialized(FieldInfo field) {
        if (field.IsStatic || field.IsInitOnly || field.IsNotSerialized) return false;
        return field.IsPublic || field.GetCustomAttribute<UnityEngine.SerializeField>() != null;
    }

    static object Convert(JsonElement value, Type type, object current) {
        if (type == typeof(string)) return value.ValueKind == JsonValueKind.String ? value.GetString() : current;
        if (type == typeof(bool)) return value.ValueKind == JsonValueKind.True || (value.ValueKind == JsonValueKind.Number && value.GetDouble() != 0);
        if (type.IsEnum) return Enum.ToObject(type, value.GetInt64());
        if (type.IsPrimitive) return System.Convert.ChangeType(value.GetDouble(), type, CultureInfo.InvariantCulture);
        if (type.IsArray) {
            if (value.ValueKind != JsonValueKind.Array) return current;
            Type element = type.GetElementType();
            Array array = Array.CreateInstance(element, value.GetArrayLength());
            int i = 0;
            foreach (JsonElement item in value.EnumerateArray()) array.SetValue(Convert(item, element, Default(element)), i++);
            return array;
        }
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>)) {
            if (value.ValueKind != JsonValueKind.Array) return current;
            Type element = type.GetGenericArguments()[0];
            IList list = (IList)Activator.CreateInstance(type);
            foreach (JsonElement item in value.EnumerateArray()) list.Add(Convert(item, element, Default(element)));
            return list;
        }
        if (value.ValueKind != JsonValueKind.Object || typeof(UnityEngine.Object).IsAssignableFrom(type)) return current;
        object instance = current ?? Default(type);
        if (instance == null) return current;
        Fill(instance, value);
        return instance;
    }

    static object Default(Type type) {
        if (type == typeof(string)) return "";
        try {
            return Activator.CreateInstance(type, nonPublic: true);
        } catch (MissingMethodException) {
            return type.IsValueType ? Activator.CreateInstance(type) : null;
        }
    }
}
