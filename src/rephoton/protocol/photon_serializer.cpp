#include "photon_serializer.h"
#include <cstring>
#include <algorithm>

namespace ReFix::Photon::Protocol {

    bool PhotonValue::AsBool(bool def) const {
        if (std::holds_alternative<bool>(data)) return std::get<bool>(data);
        if (std::holds_alternative<uint8_t>(data)) return std::get<uint8_t>(data) != 0;
        if (std::holds_alternative<int32_t>(data)) return std::get<int32_t>(data) != 0;
        return def;
    }

    uint8_t PhotonValue::AsByte(uint8_t def) const {
        if (std::holds_alternative<uint8_t>(data)) return std::get<uint8_t>(data);
        if (std::holds_alternative<int32_t>(data)) return static_cast<uint8_t>(std::get<int32_t>(data));
        return def;
    }

    int16_t PhotonValue::AsShort(int16_t def) const {
        if (std::holds_alternative<int16_t>(data)) return std::get<int16_t>(data);
        if (std::holds_alternative<int32_t>(data)) return static_cast<int16_t>(std::get<int32_t>(data));
        if (std::holds_alternative<uint8_t>(data)) return static_cast<int16_t>(std::get<uint8_t>(data));
        return def;
    }

    int32_t PhotonValue::AsInt(int32_t def) const {
        if (std::holds_alternative<int32_t>(data)) return std::get<int32_t>(data);
        if (std::holds_alternative<int16_t>(data)) return static_cast<int32_t>(std::get<int16_t>(data));
        if (std::holds_alternative<uint8_t>(data)) return static_cast<int32_t>(std::get<uint8_t>(data));
        if (std::holds_alternative<int64_t>(data)) return static_cast<int32_t>(std::get<int64_t>(data));
        return def;
    }

    int64_t PhotonValue::AsLong(int64_t def) const {
        if (std::holds_alternative<int64_t>(data)) return std::get<int64_t>(data);
        if (std::holds_alternative<int32_t>(data)) return static_cast<int64_t>(std::get<int32_t>(data));
        return def;
    }

    float PhotonValue::AsFloat(float def) const {
        if (std::holds_alternative<float>(data)) return std::get<float>(data);
        if (std::holds_alternative<double>(data)) return static_cast<float>(std::get<double>(data));
        return def;
    }

    std::string PhotonValue::AsString(const std::string& def) const {
        if (std::holds_alternative<std::string>(data)) return std::get<std::string>(data);
        return def;
    }

    std::vector<uint8_t> PhotonValue::AsByteArray() const {
        if (std::holds_alternative<std::vector<uint8_t>>(data)) return std::get<std::vector<uint8_t>>(data);
        return {};
    }

    PhotonArray PhotonValue::AsArray() const {
        if (std::holds_alternative<std::shared_ptr<PhotonArray>>(data)) {
            auto ptr = std::get<std::shared_ptr<PhotonArray>>(data);
            if (ptr) return *ptr;
        }
        return {};
    }

    PhotonHashtable PhotonValue::AsHashtable() const {
        if (std::holds_alternative<std::shared_ptr<PhotonHashtable>>(data)) {
            auto ptr = std::get<std::shared_ptr<PhotonHashtable>>(data);
            if (ptr) return *ptr;
        }
        return {};
    }

    bool PhotonValue::operator<(const PhotonValue& other) const {
        if (type != other.type) return type < other.type;
        switch (type) {
            case GpType::Boolean: return AsBool() < other.AsBool();
            case GpType::Byte:    return AsByte() < other.AsByte();
            case GpType::Short:   return AsShort() < other.AsShort();
            case GpType::Integer: return AsInt() < other.AsInt();
            case GpType::Long:    return AsLong() < other.AsLong();
            case GpType::Float:   return AsFloat() < other.AsFloat();
            case GpType::String:  return AsString() < other.AsString();
            case GpType::ByteArray: return AsByteArray() < other.AsByteArray();
            default:              return this < &other;
        }
    }

    bool PhotonValue::operator==(const PhotonValue& other) const {
        if (type != other.type) return false;
        switch (type) {
            case GpType::Null:    return true;
            case GpType::Boolean: return AsBool() == other.AsBool();
            case GpType::Byte:    return AsByte() == other.AsByte();
            case GpType::Short:   return AsShort() == other.AsShort();
            case GpType::Integer: return AsInt() == other.AsInt();
            case GpType::Long:    return AsLong() == other.AsLong();
            case GpType::Float:   return AsFloat() == other.AsFloat();
            case GpType::String:  return AsString() == other.AsString();
            case GpType::ByteArray: return AsByteArray() == other.AsByteArray();
            default:              return false;
        }
    }

    // =========================================================================
    // ENCODING
    // =========================================================================

    void PhotonSerializer::WriteByte(std::vector<uint8_t>& buffer, uint8_t val) {
        buffer.push_back(val);
    }

    void PhotonSerializer::WriteShort(std::vector<uint8_t>& buffer, int16_t val) {
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void PhotonSerializer::WriteInt(std::vector<uint8_t>& buffer, int32_t val) {
        buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void PhotonSerializer::WriteLong(std::vector<uint8_t>& buffer, int64_t val) {
        for (int i = 7; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }

    void PhotonSerializer::WriteFloat(std::vector<uint8_t>& buffer, float val) {
        uint32_t intBits = 0;
        std::memcpy(&intBits, &val, sizeof(float));
        WriteInt(buffer, static_cast<int32_t>(intBits));
    }

    void PhotonSerializer::WriteString(std::vector<uint8_t>& buffer, const std::string& str) {
        WriteShort(buffer, static_cast<int16_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    void PhotonSerializer::WriteByteArray(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& bytes) {
        WriteInt(buffer, static_cast<int32_t>(bytes.size()));
        buffer.insert(buffer.end(), bytes.begin(), bytes.end());
    }

    void PhotonSerializer::WriteValue(std::vector<uint8_t>& buffer, const PhotonValue& val, bool includeTypeHeader) {
        if (includeTypeHeader) {
            WriteByte(buffer, val.type);
        }

        switch (val.type) {
            case GpType::Null:
                break;
            case GpType::Boolean:
                WriteByte(buffer, val.AsBool() ? 1 : 0);
                break;
            case GpType::Byte:
                WriteByte(buffer, val.AsByte());
                break;
            case GpType::Short:
                WriteShort(buffer, val.AsShort());
                break;
            case GpType::Integer:
                WriteInt(buffer, val.AsInt());
                break;
            case GpType::Long:
                WriteLong(buffer, val.AsLong());
                break;
            case GpType::Float:
                WriteFloat(buffer, val.AsFloat());
                break;
            case GpType::String:
                WriteString(buffer, val.AsString());
                break;
            case GpType::ByteArray:
                WriteByteArray(buffer, val.AsByteArray());
                break;
            case GpType::Hashtable: {
                auto ht = val.AsHashtable();
                WriteShort(buffer, static_cast<int16_t>(ht.size()));
                for (const auto& [k, v] : ht) {
                    WriteValue(buffer, k, true);
                    WriteValue(buffer, v, true);
                }
                break;
            }
            case GpType::Array: { // 'y' = 0x79
                auto arr = val.AsArray();
                WriteShort(buffer, static_cast<int16_t>(arr.size()));
                uint8_t itemType = arr.empty() ? GpType::String : arr[0].type;
                WriteByte(buffer, itemType);
                for (const auto& item : arr) {
                    WriteValue(buffer, item, false); // without extra type header per item
                }
                break;
            }
            case GpType::StringArray: { // 'a' = 0x61
                auto arr = val.AsArray();
                WriteShort(buffer, static_cast<int16_t>(arr.size()));
                for (const auto& item : arr) {
                    WriteString(buffer, item.AsString());
                }
                break;
            }
            case GpType::IntegerArray: { // 'n' = 0x6E
                auto arr = val.AsArray();
                WriteInt(buffer, static_cast<int32_t>(arr.size()));
                for (const auto& item : arr) {
                    WriteInt(buffer, item.AsInt());
                }
                break;
            }
            case GpType::Dictionary: { // 'd' = 0x64 / 'D' = 0x44
                auto dict = val.AsHashtable();
                uint8_t keyType = dict.empty() ? GpType::String : dict.begin()->first.type;
                uint8_t valType = dict.empty() ? GpType::String : dict.begin()->second.type;
                WriteByte(buffer, keyType);
                WriteByte(buffer, valType);
                WriteShort(buffer, static_cast<int16_t>(dict.size()));
                for (const auto& [k, v] : dict) {
                    WriteValue(buffer, k, false);
                    WriteValue(buffer, v, false);
                }
                break;
            }
            case GpType::ObjectArray: {
                auto arr = val.AsArray();
                WriteShort(buffer, static_cast<int16_t>(arr.size()));
                for (const auto& item : arr) {
                    WriteValue(buffer, item, true);
                }
                break;
            }
            default:
                break;
        }
    }

    void PhotonSerializer::WriteParameterDictionary(std::vector<uint8_t>& buffer, const std::map<uint8_t, PhotonValue>& params) {
        WriteShort(buffer, static_cast<int16_t>(params.size()));
        for (const auto& [paramCode, paramVal] : params) {
            WriteByte(buffer, paramCode);
            WriteValue(buffer, paramVal, true);
        }
    }

    // =========================================================================
    // DECODING
    // =========================================================================

    bool PhotonSerializer::ReadByte(const std::vector<uint8_t>& buffer, size_t& offset, uint8_t& outVal) {
        if (offset >= buffer.size()) return false;
        outVal = buffer[offset++];
        return true;
    }

    bool PhotonSerializer::ReadShort(const std::vector<uint8_t>& buffer, size_t& offset, int16_t& outVal) {
        if (offset + 2 > buffer.size()) return false;
        outVal = static_cast<int16_t>((buffer[offset] << 8) | buffer[offset + 1]);
        offset += 2;
        return true;
    }

    bool PhotonSerializer::ReadInt(const std::vector<uint8_t>& buffer, size_t& offset, int32_t& outVal) {
        if (offset + 4 > buffer.size()) return false;
        outVal = static_cast<int32_t>((buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                                      (buffer[offset + 2] << 8) | buffer[offset + 3]);
        offset += 4;
        return true;
    }

    bool PhotonSerializer::ReadLong(const std::vector<uint8_t>& buffer, size_t& offset, int64_t& outVal) {
        if (offset + 8 > buffer.size()) return false;
        int64_t val = 0;
        for (int i = 0; i < 8; ++i) {
            val = (val << 8) | buffer[offset + i];
        }
        outVal = val;
        offset += 8;
        return true;
    }

    bool PhotonSerializer::ReadFloat(const std::vector<uint8_t>& buffer, size_t& offset, float& outVal) {
        int32_t intBits = 0;
        if (!ReadInt(buffer, offset, intBits)) return false;
        uint32_t uBits = static_cast<uint32_t>(intBits);
        std::memcpy(&outVal, &uBits, sizeof(float));
        return true;
    }

    bool PhotonSerializer::ReadString(const std::vector<uint8_t>& buffer, size_t& offset, std::string& outVal) {
        int16_t len = 0;
        if (!ReadShort(buffer, offset, len) || len < 0) return false;
        if (offset + static_cast<size_t>(len) > buffer.size()) return false;
        outVal = std::string(reinterpret_cast<const char*>(&buffer[offset]), static_cast<size_t>(len));
        offset += static_cast<size_t>(len);
        return true;
    }

    bool PhotonSerializer::ReadByteArray(const std::vector<uint8_t>& buffer, size_t& offset, std::vector<uint8_t>& outVal) {
        int32_t len = 0;
        if (!ReadInt(buffer, offset, len) || len < 0) return false;
        if (offset + static_cast<size_t>(len) > buffer.size()) return false;
        outVal.assign(buffer.begin() + offset, buffer.begin() + offset + len);
        offset += static_cast<size_t>(len);
        return true;
    }

    bool PhotonSerializer::ReadValue(const std::vector<uint8_t>& buffer, size_t& offset, PhotonValue& outVal, uint8_t expectedType) {
        uint8_t typeCode = expectedType;
        if (typeCode == 0) {
            if (!ReadByte(buffer, offset, typeCode)) return false;
        }

        switch (typeCode) {
            case GpType::Null:
                outVal = PhotonValue();
                return true;
            case GpType::Boolean: {
                uint8_t b = 0;
                if (!ReadByte(buffer, offset, b)) return false;
                outVal = PhotonValue(b != 0);
                return true;
            }
            case GpType::Byte: {
                uint8_t b = 0;
                if (!ReadByte(buffer, offset, b)) return false;
                outVal = PhotonValue(b);
                return true;
            }
            case GpType::Short: {
                int16_t s = 0;
                if (!ReadShort(buffer, offset, s)) return false;
                outVal = PhotonValue(s);
                return true;
            }
            case GpType::Integer: {
                int32_t i = 0;
                if (!ReadInt(buffer, offset, i)) return false;
                outVal = PhotonValue(i);
                return true;
            }
            case GpType::Long: {
                int64_t l = 0;
                if (!ReadLong(buffer, offset, l)) return false;
                outVal = PhotonValue(l);
                return true;
            }
            case GpType::Float: {
                float f = 0.0f;
                if (!ReadFloat(buffer, offset, f)) return false;
                outVal = PhotonValue(f);
                return true;
            }
            case GpType::String: {
                std::string s;
                if (!ReadString(buffer, offset, s)) return false;
                outVal = PhotonValue(s);
                return true;
            }
            case GpType::ByteArray: {
                std::vector<uint8_t> bytes;
                if (!ReadByteArray(buffer, offset, bytes)) return false;
                outVal = PhotonValue(bytes);
                return true;
            }
            case GpType::Hashtable: {
                int16_t count = 0;
                if (!ReadShort(buffer, offset, count) || count < 0) return false;
                PhotonHashtable ht;
                for (int16_t i = 0; i < count; ++i) {
                    PhotonValue k, v;
                    if (!ReadValue(buffer, offset, k) || !ReadValue(buffer, offset, v)) return false;
                    ht[k] = v;
                }
                outVal = PhotonValue(ht);
                return true;
            }
            case GpType::Custom:
            case 0x43: { // 'C' / 'c' CustomType
                uint8_t customCode = 0;
                if (!ReadByte(buffer, offset, customCode)) return false;
                int16_t len = 0;
                if (!ReadShort(buffer, offset, len) || len < 0) return false;
                if (offset + len > buffer.size()) return false;
                std::vector<uint8_t> customBytes(buffer.begin() + offset, buffer.begin() + offset + len);
                offset += len;
                outVal = PhotonValue(customBytes);
                return true;
            }
            case GpType::Dictionary: { // 'D' = 0x44
                uint8_t keyType = 0, valType = 0;
                if (!ReadByte(buffer, offset, keyType) || !ReadByte(buffer, offset, valType)) return false;
                int16_t count = 0;
                if (!ReadShort(buffer, offset, count) || count < 0) return false;
                PhotonHashtable dict;
                for (int16_t i = 0; i < count; ++i) {
                    PhotonValue k, v;
                    if (!ReadValue(buffer, offset, k, keyType) || !ReadValue(buffer, offset, v, valType)) return false;
                    dict[k] = v;
                }
                outVal = PhotonValue(dict);
                return true;
            }
            case GpType::StringArray: { // 'a' = 0x61
                int16_t count = 0;
                if (!ReadShort(buffer, offset, count) || count < 0) return false;
                PhotonArray arr;
                for (int16_t i = 0; i < count; ++i) {
                    std::string s;
                    if (!ReadString(buffer, offset, s)) return false;
                    arr.push_back(PhotonValue(s));
                }
                outVal = PhotonValue(arr);
                outVal.type = GpType::StringArray;
                return true;
            }
            case GpType::IntegerArray: { // 'n' = 0x6E
                int32_t count = 0;
                if (!ReadInt(buffer, offset, count) || count < 0) return false;
                PhotonArray arr;
                for (int32_t i = 0; i < count; ++i) {
                    int32_t val = 0;
                    if (!ReadInt(buffer, offset, val)) return false;
                    arr.push_back(PhotonValue(val));
                }
                outVal = PhotonValue(arr);
                outVal.type = GpType::IntegerArray;
                return true;
            }
            case GpType::Array: { // 'y' = 0x79
                int16_t count = 0;
                if (!ReadShort(buffer, offset, count) || count < 0) return false;
                uint8_t itemType = 0;
                if (!ReadByte(buffer, offset, itemType)) return false;
                PhotonArray arr;
                for (int16_t i = 0; i < count; ++i) {
                    PhotonValue item;
                    if (!ReadValue(buffer, offset, item, itemType)) return false;
                    arr.push_back(item);
                }
                outVal = PhotonValue(arr);
                outVal.type = GpType::Array;
                return true;
            }
            case GpType::ObjectArray: {
                int16_t count = 0;
                if (!ReadShort(buffer, offset, count) || count < 0) return false;
                PhotonArray arr;
                for (int16_t i = 0; i < count; ++i) {
                    PhotonValue item;
                    if (!ReadValue(buffer, offset, item)) return false;
                    arr.push_back(item);
                }
                outVal = PhotonValue(arr);
                return true;
            }
            default:
                return false;
        }
    }

    bool PhotonSerializer::ReadParameterDictionary(const std::vector<uint8_t>& buffer, size_t& offset, std::map<uint8_t, PhotonValue>& outParams) {
        int16_t count = 0;
        if (!ReadShort(buffer, offset, count) || count < 0) return false;
        for (int16_t i = 0; i < count; ++i) {
            uint8_t code = 0;
            if (!ReadByte(buffer, offset, code)) return false;
            PhotonValue val;
            if (!ReadValue(buffer, offset, val)) return false;
            outParams[code] = val;
        }
        return true;
    }

} // namespace ReFix::Photon::Protocol
