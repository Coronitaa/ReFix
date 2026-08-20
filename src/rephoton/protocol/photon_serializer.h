#pragma once

#include "photon_constants.h"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>

namespace ReFix::Photon::Protocol {

    struct PhotonValue;

    using PhotonHashtable = std::map<PhotonValue, PhotonValue>;
    using PhotonArray = std::vector<PhotonValue>;

    struct PhotonValue {
        uint8_t type = GpType::Null;

        // Supported payload variants
        std::variant<
            std::monostate,                         // Null
            bool,                                   // Boolean
            uint8_t,                                // Byte
            int16_t,                                // Short
            int32_t,                                // Integer
            int64_t,                                // Long
            float,                                  // Float
            double,                                 // Double
            std::string,                            // String
            std::vector<uint8_t>,                   // ByteArray
            std::shared_ptr<PhotonArray>,           // Array / ObjectArray
            std::shared_ptr<PhotonHashtable>        // Hashtable / Dictionary
        > data;

        PhotonValue() : type(GpType::Null), data(std::monostate{}) {}
        PhotonValue(bool val) : type(GpType::Boolean), data(val) {}
        PhotonValue(uint8_t val) : type(GpType::Byte), data(val) {}
        PhotonValue(int16_t val) : type(GpType::Short), data(val) {}
        PhotonValue(int32_t val) : type(GpType::Integer), data(val) {}
        PhotonValue(int64_t val) : type(GpType::Long), data(val) {}
        PhotonValue(float val) : type(GpType::Float), data(val) {}
        PhotonValue(const char* val) : type(GpType::String), data(std::string(val ? val : "")) {}
        PhotonValue(const std::string& val) : type(GpType::String), data(val) {}
        PhotonValue(const std::vector<uint8_t>& val) : type(GpType::ByteArray), data(val) {}
        PhotonValue(const PhotonArray& val) : type(GpType::ObjectArray), data(std::make_shared<PhotonArray>(val)) {}
        PhotonValue(const PhotonHashtable& val) : type(GpType::Hashtable), data(std::make_shared<PhotonHashtable>(val)) {}

        bool IsNull() const { return type == GpType::Null; }
        bool AsBool(bool def = false) const;
        uint8_t AsByte(uint8_t def = 0) const;
        int16_t AsShort(int16_t def = 0) const;
        int32_t AsInt(int32_t def = 0) const;
        int64_t AsLong(int64_t def = 0) const;
        float AsFloat(float def = 0.0f) const;
        std::string AsString(const std::string& def = "") const;
        std::vector<uint8_t> AsByteArray() const;
        PhotonArray AsArray() const;
        PhotonHashtable AsHashtable() const;

        bool operator<(const PhotonValue& other) const;
        bool operator==(const PhotonValue& other) const;
    };

    class PhotonSerializer {
    public:
        // Encoding methods (Big-Endian wire format)
        static void WriteByte(std::vector<uint8_t>& buffer, uint8_t val);
        static void WriteShort(std::vector<uint8_t>& buffer, int16_t val);
        static void WriteInt(std::vector<uint8_t>& buffer, int32_t val);
        static void WriteLong(std::vector<uint8_t>& buffer, int64_t val);
        static void WriteFloat(std::vector<uint8_t>& buffer, float val);
        static void WriteString(std::vector<uint8_t>& buffer, const std::string& str);
        static void WriteByteArray(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& bytes);
        static void WriteValue(std::vector<uint8_t>& buffer, const PhotonValue& val, bool includeTypeHeader = true);
        static void WriteParameterDictionary(std::vector<uint8_t>& buffer, const std::map<uint8_t, PhotonValue>& params);

        // Decoding methods (Bounds-checked with byte offset)
        static bool ReadByte(const std::vector<uint8_t>& buffer, size_t& offset, uint8_t& outVal);
        static bool ReadShort(const std::vector<uint8_t>& buffer, size_t& offset, int16_t& outVal);
        static bool ReadInt(const std::vector<uint8_t>& buffer, size_t& offset, int32_t& outVal);
        static bool ReadLong(const std::vector<uint8_t>& buffer, size_t& offset, int64_t& outVal);
        static bool ReadFloat(const std::vector<uint8_t>& buffer, size_t& offset, float& outVal);
        static bool ReadString(const std::vector<uint8_t>& buffer, size_t& offset, std::string& outVal);
        static bool ReadByteArray(const std::vector<uint8_t>& buffer, size_t& offset, std::vector<uint8_t>& outVal);
        static bool ReadValue(const std::vector<uint8_t>& buffer, size_t& offset, PhotonValue& outVal, uint8_t expectedType = 0);
        static bool ReadParameterDictionary(const std::vector<uint8_t>& buffer, size_t& offset, std::map<uint8_t, PhotonValue>& outParams);
    };

} // namespace ReFix::Photon::Protocol
