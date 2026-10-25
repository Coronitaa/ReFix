#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Photon "GpBinaryV18" (Protocol18) codec for a server emulator.
//
// Values are kept as Raw: the exact encoded bytes including the leading type byte. The emulator only
// interprets the handful of values it needs (ints, strings, hashtables of properties) and forwards everything
// else byte-for-byte, so game-specific custom types never need to be understood.
namespace PnProto18 {

using Bytes = std::string;   // encoded bytes (binary-safe)
using Raw = std::string;     // one encoded value: [type byte][body]

enum Type : uint8_t {
    kUnknown = 0, kBoolean = 2, kByte = 3, kShort = 4, kFloat = 5, kDouble = 6, kString = 7, kNull = 8,
    kCompressedInt = 9, kCompressedLong = 10, kInt1 = 11, kInt1Neg = 12, kInt2 = 13, kInt2Neg = 14,
    kL1 = 15, kL1Neg = 16, kL2 = 17, kL2Neg = 18, kCustom = 19, kDictionary = 20, kHashtable = 21,
    kObjectArray = 23, kOperationRequest = 24, kOperationResponse = 25, kEventData = 26,
    kBoolFalse = 27, kBoolTrue = 28, kShortZero = 29, kIntZero = 30, kLongZero = 31, kFloatZero = 32,
    kDoubleZero = 33, kByteZero = 34, kArray = 64, kBooleanArray = 66, kByteArray = 67, kShortArray = 68,
    kFloatArray = 69, kDoubleArray = 70, kStringArray = 71, kCompressedIntArray = 73,
    kCompressedLongArray = 74, kCustomTypeArray = 83, kDictionaryArray = 84, kHashtableArray = 85,
    kCustomSlimFirst = 128, kCustomSlimLast = 228,
};

// Sequential reader over an encoded buffer. Any malformed input sets ok = false and stops consuming.
struct Reader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    bool ok = true;

    Reader(const void* d, size_t n) : data((const uint8_t*)d), size(n) {}
    explicit Reader(const Bytes& b) : Reader(b.data(), b.size()) {}
    bool Has(size_t n) const { return ok && pos + n <= size; }
    uint8_t U8();
    uint32_t VarU32();
    uint64_t VarU64();
    void Skip(size_t n);
    // Skips one value whose type byte was already consumed.
    void SkipBody(uint8_t type);
    // Reads one complete value (type byte + body) as Raw.
    Raw ReadRaw();
};

// --- Writers -----------------------------------------------------------------------------------------------
void PutVarU32(Bytes& out, uint32_t v);
Raw Int(int32_t v);                 // CompressedInt
Raw Bool(bool v);
Raw Byte(uint8_t v);
Raw Short(int16_t v);
Raw Str(const std::string& utf8);
Raw Null();
Raw IntArray(const std::vector<int32_t>& v);
Raw StrArray(const std::vector<std::string>& v);

// --- Interpreting raws -------------------------------------------------------------------------------------
bool AsInt(const Raw& raw, int64_t& out);         // any integer/byte/short encoding
bool AsString(const Raw& raw, std::string& out);
bool AsBool(const Raw& raw, bool& out);
bool AsIntArray(const Raw& raw, std::vector<int32_t>& out); // CompressedIntArray / Array of ints / ObjectArray
bool IsNull(const Raw& raw);

// Ordered property table (Photon Hashtable). Keys and values are raws; equal keys have equal encodings.
struct Table {
    std::vector<std::pair<Raw, Raw>> entries;

    const Raw* Find(const Raw& key) const;
    void Set(const Raw& key, const Raw& value);    // replace or append
    void Erase(const Raw& key);
    // Applies changes: null values delete keys when deleteNulls, otherwise they are stored.
    void Merge(const Table& changes, bool deleteNulls);
    bool Contains(const Table& subset) const;      // every entry of subset is present with an equal value
    Raw Encode() const;                            // as Hashtable
};
bool AsTable(const Raw& raw, Table& out);           // Hashtable or Dictionary

// Operation parameters: byte code -> raw value.
using Params = std::map<uint8_t, Raw>;
bool ReadParams(Reader& r, Params& out);           // [count:u8] ([code][raw])*
void WriteParams(Bytes& out, const Params& params);

// Message bodies after the [0xF3][msgType] prefix.
bool ParseOperationRequest(const Bytes& body, uint8_t& opCode, Params& params);
Bytes OperationResponse(uint8_t opCode, int16_t returnCode, const std::string& debugMessage, const Params& params);
Bytes EventData(uint8_t code, const Params& params);
Bytes OperationRequest(uint8_t opCode, const Params& params);

} // namespace PnProto18
