// ReFix EOS v3 - wire encoding.
//
// A tiny length-prefixed binary format. It is deliberately not a general
// serialiser: every field is written and read explicitly, bounds are checked on
// every read, and a truncated or hostile datagram makes the reader fail rather
// than walk off the end. Nothing arriving from the network is trusted.
#pragma once
#include "../core/refix_common.h"

namespace refix {

class Writer {
public:
    void U8(uint8_t v)   { m_buf.push_back(v); }
    void U16(uint16_t v) { Raw(&v, 2); }
    void U32(uint32_t v) { Raw(&v, 4); }
    void U64(uint64_t v) { Raw(&v, 8); }
    void I64(int64_t v)  { Raw(&v, 8); }
    void Bool(bool v)    { U8(v ? 1 : 0); }

    void Str(const std::string& s) {
        uint32_t n = (uint32_t)s.size();
        U32(n);
        if (n) Raw(s.data(), n);
    }

    void Bytes(const void* p, size_t n) { U32((uint32_t)n); if (n) Raw(p, n); }

    const std::vector<uint8_t>& Data() const { return m_buf; }
    size_t Size() const { return m_buf.size(); }

private:
    void Raw(const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        m_buf.insert(m_buf.end(), b, b + n);
    }
    std::vector<uint8_t> m_buf;
};

class Reader {
public:
    Reader(const void* data, size_t len) : m_p((const uint8_t*)data), m_n(len) {}

    bool Ok() const { return !m_failed; }
    size_t Remaining() const { return m_failed ? 0 : m_n - m_at; }

    uint8_t  U8()   { uint8_t v = 0;  Raw(&v, 1); return v; }
    uint16_t U16()  { uint16_t v = 0; Raw(&v, 2); return v; }
    uint32_t U32()  { uint32_t v = 0; Raw(&v, 4); return v; }
    uint64_t U64()  { uint64_t v = 0; Raw(&v, 8); return v; }
    int64_t  I64()  { int64_t v = 0;  Raw(&v, 8); return v; }
    bool     Bool() { return U8() != 0; }

    std::string Str(uint32_t maxLen = 64 * 1024) {
        uint32_t n = U32();
        if (m_failed || n > maxLen || Remaining() < n) { m_failed = true; return {}; }
        std::string s((const char*)(m_p + m_at), n);
        m_at += n;
        return s;
    }

    std::vector<uint8_t> Bytes(uint32_t maxLen = 64 * 1024) {
        uint32_t n = U32();
        if (m_failed || n > maxLen || Remaining() < n) { m_failed = true; return {}; }
        std::vector<uint8_t> v(m_p + m_at, m_p + m_at + n);
        m_at += n;
        return v;
    }

private:
    void Raw(void* out, size_t n) {
        if (m_failed || Remaining() < n) { m_failed = true; std::memset(out, 0, n); return; }
        std::memcpy(out, m_p + m_at, n);
        m_at += n;
    }
    const uint8_t* m_p;
    size_t         m_n;
    size_t         m_at = 0;
    bool           m_failed = false;
};

} // namespace refix
