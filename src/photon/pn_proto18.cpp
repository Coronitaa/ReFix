#include "pn_proto18.h"
#include <cstring>

namespace PnProto18 {

namespace {

constexpr int kMaxDepth = 32; // nested containers; guards against hostile input

void SkipBodyDepth(Reader& r, uint8_t type, int depth);

// Dictionary type header: [keyType][valueType] (+ nested header bytes for dictionary / array values).
void SkipDictHeader(Reader& r, uint8_t& keyType, uint8_t& valueType, int depth) {
    keyType = r.U8();
    valueType = r.U8();
    if (valueType == kDictionary) {
        uint8_t k, v;
        SkipDictHeader(r, k, v, depth + 1);
        valueType = kDictionary; // values are full dictionaries described by the nested header
    } else if (valueType == kArray) {
        uint8_t t = r.U8();     // element type of the array values
        while (r.ok && t == kArray) t = r.U8();
        valueType = kUnknown;   // client reads array values with their own type byte
    }
}

void SkipDictElements(Reader& r, uint8_t keyType, uint8_t valueType, int depth) {
    uint32_t count = r.VarU32();
    for (uint32_t i = 0; r.ok && i < count; i++) {
        if (keyType == kUnknown) r.ReadRaw(); else SkipBodyDepth(r, keyType, depth + 1);
        if (valueType == kUnknown) r.ReadRaw(); else SkipBodyDepth(r, valueType, depth + 1);
    }
}

void SkipString(Reader& r) {
    r.Skip(r.VarU32());
}

void SkipBodyDepth(Reader& r, uint8_t type, int depth) {
    if (!r.ok || depth > kMaxDepth) { r.ok = false; return; }
    if (type >= kCustomSlimFirst && type <= kCustomSlimLast) { r.Skip(r.VarU32()); return; }
    switch (type) {
        case kNull: case kBoolFalse: case kBoolTrue: case kShortZero: case kIntZero: case kLongZero:
        case kFloatZero: case kDoubleZero: case kByteZero: return;
        case kBoolean: case kByte: case kInt1: case kInt1Neg: case kL1: case kL1Neg: r.Skip(1); return;
        case kShort: case kInt2: case kInt2Neg: case kL2: case kL2Neg: r.Skip(2); return;
        case kFloat: r.Skip(4); return;
        case kDouble: r.Skip(8); return;
        case kString: SkipString(r); return;
        case kCompressedInt: r.VarU32(); return;
        case kCompressedLong: r.VarU64(); return;
        case kCustom: r.Skip(1); r.Skip(r.VarU32()); return;
        case kDictionary: { uint8_t k, v; SkipDictHeader(r, k, v, depth); SkipDictElements(r, k, v, depth); return; }
        case kHashtable: {
            uint32_t n = r.VarU32();
            for (uint32_t i = 0; r.ok && i < n; i++) { r.ReadRaw(); r.ReadRaw(); }
            return;
        }
        case kObjectArray: {
            uint32_t n = r.VarU32();
            for (uint32_t i = 0; r.ok && i < n; i++) r.ReadRaw();
            return;
        }
        case kOperationRequest: { r.Skip(1); Params p; ReadParams(r, p); return; }
        case kOperationResponse: { r.Skip(1); r.Skip(2); r.ReadRaw(); Params p; ReadParams(r, p); return; }
        case kEventData: { r.Skip(1); Params p; ReadParams(r, p); return; }
        case kArray: {
            uint32_t n = r.VarU32();
            uint8_t elem = r.U8();
            if (elem == kDictionary) {
                uint8_t k, v;
                SkipDictHeader(r, k, v, depth);
                for (uint32_t i = 0; r.ok && i < n; i++) SkipDictElements(r, k, v, depth);
            } else {
                for (uint32_t i = 0; r.ok && i < n; i++) SkipBodyDepth(r, elem, depth + 1);
            }
            return;
        }
        case kBooleanArray: { uint32_t n = r.VarU32(); r.Skip((n + 7) / 8); return; }
        case kByteArray: r.Skip(r.VarU32()); return;
        case kShortArray: r.Skip((size_t)r.VarU32() * 2); return;
        case kFloatArray: r.Skip((size_t)r.VarU32() * 4); return;
        case kDoubleArray: r.Skip((size_t)r.VarU32() * 8); return;
        case kStringArray: { uint32_t n = r.VarU32(); for (uint32_t i = 0; r.ok && i < n; i++) SkipString(r); return; }
        case kCompressedIntArray: { uint32_t n = r.VarU32(); for (uint32_t i = 0; r.ok && i < n; i++) r.VarU32(); return; }
        case kCompressedLongArray: { uint32_t n = r.VarU32(); for (uint32_t i = 0; r.ok && i < n; i++) r.VarU64(); return; }
        case kCustomTypeArray: {
            uint32_t n = r.VarU32();
            r.Skip(1);
            for (uint32_t i = 0; r.ok && i < n; i++) r.Skip(r.VarU32());
            return;
        }
        case kDictionaryArray: {
            uint8_t k, v;
            SkipDictHeader(r, k, v, depth);
            uint32_t n = r.VarU32();
            for (uint32_t i = 0; r.ok && i < n; i++) SkipDictElements(r, k, v, depth);
            return;
        }
        case kHashtableArray: {
            uint32_t n = r.VarU32();
            for (uint32_t i = 0; r.ok && i < n; i++) SkipBodyDepth(r, kHashtable, depth + 1);
            return;
        }
        default:
            r.ok = false; // unknown type: cannot find the value boundary
            return;
    }
}

int32_t ZigZag32(uint32_t v) { return (int32_t)(v >> 1) ^ -(int32_t)(v & 1); }
uint32_t ToZigZag32(int32_t v) { return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31); }
int64_t ZigZag64(uint64_t v) { return (int64_t)(v >> 1) ^ -(int64_t)(v & 1); }

} // namespace

// --- Reader ------------------------------------------------------------------------------------------------

uint8_t Reader::U8() {
    if (!Has(1)) { ok = false; return 0; }
    return data[pos++];
}

uint32_t Reader::VarU32() {
    uint32_t v = 0;
    for (int shift = 0; shift < 35 && ok; shift += 7) {
        uint8_t b = U8();
        v |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return v;
    }
    ok = false;
    return 0;
}

uint64_t Reader::VarU64() {
    uint64_t v = 0;
    for (int shift = 0; shift < 70 && ok; shift += 7) {
        uint8_t b = U8();
        v |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return v;
    }
    ok = false;
    return 0;
}

void Reader::Skip(size_t n) {
    if (!Has(n)) { ok = false; return; }
    pos += n;
}

void Reader::SkipBody(uint8_t type) {
    SkipBodyDepth(*this, type, 0);
}

Raw Reader::ReadRaw() {
    size_t start = pos;
    uint8_t type = U8();
    SkipBody(type);
    return ok ? Raw((const char*)data + start, pos - start) : Raw();
}

// --- Writers -----------------------------------------------------------------------------------------------

void PutVarU32(Bytes& out, uint32_t v) {
    while (v >= 0x80) { out += (char)((v & 0x7F) | 0x80); v >>= 7; }
    out += (char)v;
}

Raw Int(int32_t v) {
    if (v == 0) return Raw(1, (char)kIntZero);
    Raw r(1, (char)kCompressedInt);
    PutVarU32(r, ToZigZag32(v));
    return r;
}

Raw Bool(bool v) { return Raw(1, (char)(v ? kBoolTrue : kBoolFalse)); }
Raw Byte(uint8_t v) { return v ? Raw{ (char)kByte, (char)v } : Raw(1, (char)kByteZero); }
Raw Short(int16_t v) { return Raw{ (char)kShort, (char)(v & 0xFF), (char)((uint16_t)v >> 8) }; }
Raw Null() { return Raw(1, (char)kNull); }

Raw Str(const std::string& utf8) {
    Raw r(1, (char)kString);
    PutVarU32(r, (uint32_t)utf8.size());
    return r + utf8;
}

Raw IntArray(const std::vector<int32_t>& v) {
    Raw r(1, (char)kCompressedIntArray);
    PutVarU32(r, (uint32_t)v.size());
    for (int32_t x : v) PutVarU32(r, ToZigZag32(x));
    return r;
}

Raw StrArray(const std::vector<std::string>& v) {
    Raw r(1, (char)kStringArray);
    PutVarU32(r, (uint32_t)v.size());
    for (const auto& s : v) { PutVarU32(r, (uint32_t)s.size()); r += s; }
    return r;
}

// --- Interpreting raws -------------------------------------------------------------------------------------

bool AsInt(const Raw& raw, int64_t& out) {
    Reader r(raw);
    uint8_t t = r.U8();
    switch (t) {
        case kIntZero: case kShortZero: case kLongZero: case kByteZero: out = 0; break;
        case kByte: out = r.U8(); break;
        case kInt1: case kL1: out = r.U8(); break;
        case kInt1Neg: case kL1Neg: out = -(int64_t)r.U8(); break;
        case kInt2: case kL2: { uint32_t lo = r.U8(), hi = r.U8(); out = (int64_t)(lo | (hi << 8)); break; }
        case kInt2Neg: case kL2Neg: { uint32_t lo = r.U8(), hi = r.U8(); out = -(int64_t)(lo | (hi << 8)); break; }
        case kShort: { uint32_t lo = r.U8(), hi = r.U8(); out = (int16_t)(lo | (hi << 8)); break; }
        case kCompressedInt: out = ZigZag32(r.VarU32()); break;
        case kCompressedLong: out = ZigZag64(r.VarU64()); break;
        default: return false;
    }
    return r.ok;
}

bool AsString(const Raw& raw, std::string& out) {
    Reader r(raw);
    if (r.U8() != kString) return false;
    uint32_t n = r.VarU32();
    if (!r.Has(n)) return false;
    out.assign((const char*)r.data + r.pos, n);
    return true;
}

bool AsBool(const Raw& raw, bool& out) {
    Reader r(raw);
    uint8_t t = r.U8();
    if (t == kBoolTrue || t == kBoolFalse) { out = t == kBoolTrue; return true; }
    if (t == kBoolean) { out = r.U8() != 0; return r.ok; }
    return false;
}

bool IsNull(const Raw& raw) {
    return raw.size() == 1 && (uint8_t)raw[0] == kNull;
}

bool AsIntArray(const Raw& raw, std::vector<int32_t>& out) {
    Reader r(raw);
    uint8_t t = r.U8();
    out.clear();
    if (t == kCompressedIntArray) {
        uint32_t n = r.VarU32();
        for (uint32_t i = 0; r.ok && i < n; i++) out.push_back(ZigZag32(r.VarU32()));
        return r.ok;
    }
    if (t == kObjectArray || t == kArray) {
        uint32_t n = r.VarU32();
        uint8_t elem = t == kArray ? r.U8() : kUnknown;
        for (uint32_t i = 0; r.ok && i < n; i++) {
            Raw item;
            if (elem == kUnknown) {
                item = r.ReadRaw();
            } else { // typed array: elements carry no type byte of their own
                size_t start = r.pos;
                r.SkipBody(elem);
                item = Raw(1, (char)elem) + Raw((const char*)r.data + start, r.pos - start);
            }
            int64_t v;
            if (!AsInt(item, v)) return false;
            out.push_back((int32_t)v);
        }
        return r.ok;
    }
    return false;
}

// --- Table -------------------------------------------------------------------------------------------------

const Raw* Table::Find(const Raw& key) const {
    for (const auto& [k, v] : entries) if (k == key) return &v;
    return nullptr;
}

void Table::Set(const Raw& key, const Raw& value) {
    for (auto& [k, v] : entries) if (k == key) { v = value; return; }
    entries.emplace_back(key, value);
}

void Table::Erase(const Raw& key) {
    for (auto it = entries.begin(); it != entries.end(); ++it) if (it->first == key) { entries.erase(it); return; }
}

void Table::Merge(const Table& changes, bool deleteNulls) {
    for (const auto& [k, v] : changes.entries) {
        if (deleteNulls && IsNull(v)) Erase(k); else Set(k, v);
    }
}

bool Table::Contains(const Table& subset) const {
    for (const auto& [k, v] : subset.entries) {
        const Raw* mine = Find(k);
        if (!mine || *mine != v) return false;
    }
    return true;
}

Raw Table::Encode() const {
    Raw r(1, (char)kHashtable);
    PutVarU32(r, (uint32_t)entries.size());
    for (const auto& [k, v] : entries) r += k + v;
    return r;
}

bool AsTable(const Raw& raw, Table& out) {
    Reader r(raw);
    uint8_t t = r.U8();
    out.entries.clear();
    if (t == kHashtable) {
        uint32_t n = r.VarU32();
        for (uint32_t i = 0; r.ok && i < n; i++) {
            Raw k = r.ReadRaw();
            Raw v = r.ReadRaw();
            if (r.ok) out.entries.emplace_back(k, v);
        }
        return r.ok;
    }
    return t == kNull;
}

// --- Parameters and messages -------------------------------------------------------------------------------

bool ReadParams(Reader& r, Params& out) {
    uint8_t n = r.U8();
    for (uint8_t i = 0; r.ok && i < n; i++) {
        uint8_t code = r.U8();
        Raw v = r.ReadRaw();
        if (r.ok) out[code] = v;
    }
    return r.ok;
}

void WriteParams(Bytes& out, const Params& params) {
    out += (char)(uint8_t)params.size();
    for (const auto& [code, v] : params) { out += (char)code; out += v; }
}

bool ParseOperationRequest(const Bytes& body, uint8_t& opCode, Params& params) {
    Reader r(body);
    opCode = r.U8();
    return ReadParams(r, params);
}

Bytes OperationResponse(uint8_t opCode, int16_t returnCode, const std::string& debugMessage, const Params& params) {
    Bytes b;
    b += (char)opCode;
    b += (char)(returnCode & 0xFF);
    b += (char)((uint16_t)returnCode >> 8);
    if (debugMessage.empty()) {
        b += (char)kNull;
    } else {
        b += (char)kString;
        PutVarU32(b, (uint32_t)debugMessage.size());
        b += debugMessage;
    }
    WriteParams(b, params);
    return b;
}

Bytes EventData(uint8_t code, const Params& params) {
    Bytes b(1, (char)code);
    WriteParams(b, params);
    return b;
}

Bytes OperationRequest(uint8_t opCode, const Params& params) {
    Bytes b(1, (char)opCode);
    WriteParams(b, params);
    return b;
}

} // namespace PnProto18
