// =============================================================================
// ReFix Online v2 - Safe Binary Wire Serializer Implementation
// =============================================================================
#include "refix_wire.h"

namespace ReFixOnline {

ByteWriter::ByteWriter(size_t initialCapacity) {
    m_buffer.reserve(initialCapacity);
}

void ByteWriter::WriteUint8(uint8_t val) {
    m_buffer.push_back(val);
}

void ByteWriter::WriteUint16(uint16_t val) {
    m_buffer.push_back((uint8_t)(val & 0xFF));
    m_buffer.push_back((uint8_t)((val >> 8) & 0xFF));
}

void ByteWriter::WriteUint32(uint32_t val) {
    m_buffer.push_back((uint8_t)(val & 0xFF));
    m_buffer.push_back((uint8_t)((val >> 8) & 0xFF));
    m_buffer.push_back((uint8_t)((val >> 16) & 0xFF));
    m_buffer.push_back((uint8_t)((val >> 24) & 0xFF));
}

void ByteWriter::WriteUint64(uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        m_buffer.push_back((uint8_t)((val >> (i * 8)) & 0xFF));
    }
}

void ByteWriter::WriteInt32(int32_t val) {
    WriteUint32((uint32_t)val);
}

void ByteWriter::WriteInt64(int64_t val) {
    WriteUint64((uint64_t)val);
}

void ByteWriter::WriteString(const std::string& str) {
    uint16_t len = (uint16_t)str.length();
    WriteUint16(len);
    if (len > 0) {
        WriteBytes(str.data(), len);
    }
}

void ByteWriter::WriteBytes(const void* data, size_t len) {
    if (!data || len == 0) return;
    const uint8_t* p = (const uint8_t*)data;
    m_buffer.insert(m_buffer.end(), p, p + len);
}

ByteReader::ByteReader(const uint8_t* data, size_t size)
    : m_data(data), m_size(size), m_offset(0), m_overrun(false) {}

bool ByteReader::ReadUint8(uint8_t& outVal) {
    if (m_offset + 1 > m_size) {
        m_overrun = true;
        return false;
    }
    outVal = m_data[m_offset++];
    return true;
}

bool ByteReader::ReadUint16(uint16_t& outVal) {
    if (m_offset + 2 > m_size) {
        m_overrun = true;
        return false;
    }
    outVal = (uint16_t)m_data[m_offset] | ((uint16_t)m_data[m_offset + 1] << 8);
    m_offset += 2;
    return true;
}

bool ByteReader::ReadUint32(uint32_t& outVal) {
    if (m_offset + 4 > m_size) {
        m_overrun = true;
        return false;
    }
    outVal = (uint32_t)m_data[m_offset] |
             ((uint32_t)m_data[m_offset + 1] << 8) |
             ((uint32_t)m_data[m_offset + 2] << 16) |
             ((uint32_t)m_data[m_offset + 3] << 24);
    m_offset += 4;
    return true;
}

bool ByteReader::ReadUint64(uint64_t& outVal) {
    if (m_offset + 8 > m_size) {
        m_overrun = true;
        return false;
    }
    outVal = 0;
    for (int i = 0; i < 8; ++i) {
        outVal |= ((uint64_t)m_data[m_offset + i] << (i * 8));
    }
    m_offset += 8;
    return true;
}

bool ByteReader::ReadInt32(int32_t& outVal) {
    uint32_t uval = 0;
    if (!ReadUint32(uval)) return false;
    outVal = (int32_t)uval;
    return true;
}

bool ByteReader::ReadInt64(int64_t& outVal) {
    uint64_t uval = 0;
    if (!ReadUint64(uval)) return false;
    outVal = (int64_t)uval;
    return true;
}

bool ByteReader::ReadString(std::string& outStr, size_t maxLen) {
    uint16_t len = 0;
    if (!ReadUint16(len)) return false;
    if (len > maxLen || m_offset + len > m_size) {
        m_overrun = true;
        return false;
    }
    if (len == 0) {
        outStr.clear();
        return true;
    }
    outStr.assign((const char*)(m_data + m_offset), len);
    m_offset += len;
    return true;
}

bool ByteReader::ReadBytes(void* outData, size_t len) {
    if (len == 0) return true;
    if (!outData || m_offset + len > m_size) {
        m_overrun = true;
        return false;
    }
    std::memcpy(outData, m_data + m_offset, len);
    m_offset += len;
    return true;
}

} // namespace ReFixOnline
