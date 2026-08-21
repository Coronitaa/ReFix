// =============================================================================
// ReFix Online v2 - Safe Binary Wire Serializer & Deserializer
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <cstring>

namespace ReFixOnline {

class ByteWriter {
public:
    explicit ByteWriter(size_t initialCapacity = 256);

    void WriteUint8(uint8_t val);
    void WriteUint16(uint16_t val);
    void WriteUint32(uint32_t val);
    void WriteUint64(uint64_t val);
    void WriteInt32(int32_t val);
    void WriteInt64(int64_t val);
    void WriteString(const std::string& str);
    void WriteBytes(const void* data, size_t len);

    const uint8_t* GetData() const { return m_buffer.data(); }
    size_t GetSize() const { return m_buffer.size(); }
    const std::vector<uint8_t>& GetBuffer() const { return m_buffer; }

private:
    std::vector<uint8_t> m_buffer;
};

class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size);

    bool ReadUint8(uint8_t& outVal);
    bool ReadUint16(uint16_t& outVal);
    bool ReadUint32(uint32_t& outVal);
    bool ReadUint64(uint64_t& outVal);
    bool ReadInt32(int32_t& outVal);
    bool ReadInt64(int64_t& outVal);
    bool ReadString(std::string& outStr, size_t maxLen = 1024);
    bool ReadBytes(void* outData, size_t len);

    size_t GetRemaining() const { return (m_offset <= m_size) ? (m_size - m_offset) : 0; }
    size_t GetOffset() const { return m_offset; }
    bool HasOverrun() const { return m_overrun; }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_offset;
    bool m_overrun;
};

} // namespace ReFixOnline
