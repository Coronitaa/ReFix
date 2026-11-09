// Profile sync: package format, folder destination and cloud folder detection (profile_sync.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "profile_sync.h"
#include "overlay_text.h"

#include "../mods/crypto/monocypher.h"
#include "../mods/miniz/miniz.h"

#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")

namespace refix::overlay::sync {
namespace {

constexpr uint32_t kMagic = 0x50584652;      // "RFXP"
constexpr uint32_t kVersion = 1;
constexpr size_t kMaxPackage = 1024ull * 1024 * 1024;

#pragma pack(push, 1)
struct Header {
    uint32_t magic;
    uint32_t version;
    uint64_t steamId;
    uint64_t created;          // FILETIME
    uint8_t stateHash[32];     // files the package holds (profile_sync.cpp)
    uint8_t digest[32];        // BLAKE2b of the zip
    uint8_t nonce[24];
    uint64_t zipSize;
    uint8_t reserved[8];
};
#pragma pack(pop)
static_assert(sizeof(Header) == 128, "package header");

void Key(uint8_t key[32]) {
    static const char kLabel[] = "ReFix profile package v1";
    crypto_blake2b_general(key, 32, nullptr, 0, (const uint8_t*)kLabel, sizeof(kLabel) - 1);
}

uint64_t Now() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

std::wstring ReadJsonPath(const std::wstring& file) {
    // Dropbox info.json: {"personal": {"path": "C:\\Users\\x\\Dropbox", ...}}
    std::ifstream f(file, std::ios::binary);
    if (!f) return {};
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const size_t key = text.find("\"path\"");
    if (key == std::string::npos) return {};
    const size_t open = text.find('"', text.find(':', key) + 1);
    if (open == std::string::npos) return {};
    std::string path;
    for (size_t i = open + 1; i < text.size() && text[i] != '"'; ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) ++i;
        path += text[i];
    }
    return FromUtf8(path);
}

bool IsDir(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

class FolderTarget : public ISyncTarget {
public:
    explicit FolderTarget(std::wstring folder) : folder_(std::move(folder)) {}
    std::wstring Name() const override { return folder_; }
    bool Ready() const override {
        if (folder_.empty()) return false;
        if (IsDir(folder_)) return true;
        // The folder is ours to create, as long as its parent (cloud folder, USB drive) is there.
        const size_t slash = folder_.find_last_of(L"\\/");
        return slash != std::wstring::npos && IsDir(folder_.substr(0, slash)) && CreateDirectoryW(folder_.c_str(), nullptr);
    }
    bool Read(const std::wstring& file, std::vector<uint8_t>& out) override {
        std::ifstream f(folder_ + L"\\" + file, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        const std::streamoff size = f.tellg();
        if (size <= 0 || (uint64_t)size > kMaxPackage) return false;
        out.resize((size_t)size);
        f.seekg(0);
        return (bool)f.read((char*)out.data(), size);
    }
    bool Write(const std::wstring& file, const std::vector<uint8_t>& data) override {
        // Write beside it, then swap in one step: a cloud app never uploads half a package.
        const std::wstring final = folder_ + L"\\" + file, temp = final + L".part";
        {
            std::ofstream f(temp, std::ios::binary | std::ios::trunc);
            if (!f || !f.write((const char*)data.data(), (std::streamsize)data.size())) return false;
        }
        return MoveFileExW(temp.c_str(), final.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }

private:
    std::wstring folder_;
};

} // namespace

std::unique_ptr<ISyncTarget> MakeFolderTarget(const std::wstring& folder) { return std::make_unique<FolderTarget>(folder); }

std::vector<CloudFolder> CloudFolders() {
    std::vector<CloudFolder> out;
    wchar_t buf[MAX_PATH] = {};
    for (const wchar_t* var : { L"OneDriveConsumer", L"OneDrive" }) {
        if (GetEnvironmentVariableW(var, buf, MAX_PATH) && IsDir(buf)) {
            out.push_back({ L"OneDrive", buf });
            break;
        }
    }
    // Google Drive for desktop mounts a drive letter with "My Drive" ("Mi unidad" in Spanish Windows).
    const DWORD drives = GetLogicalDrives();
    for (wchar_t letter = L'D'; letter <= L'Z'; ++letter) {
        if (!(drives & (1u << (letter - L'A')))) continue;
        for (const wchar_t* name : { L"My Drive", L"Mi unidad" }) {
            const std::wstring p = std::wstring(1, letter) + L":\\" + name;
            if (IsDir(p)) out.push_back({ L"Google Drive", p });
        }
    }
    for (const wchar_t* var : { L"LOCALAPPDATA", L"APPDATA" }) {
        if (!GetEnvironmentVariableW(var, buf, MAX_PATH)) continue;
        const std::wstring p = ReadJsonPath(std::wstring(buf) + L"\\Dropbox\\info.json");
        if (!p.empty() && IsDir(p)) {
            out.push_back({ L"Dropbox", p });
            break;
        }
    }
    return out;
}

bool Pack(const FileList& files, uint64_t steamId, const uint8_t stateHash[32], std::vector<uint8_t>& out) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 1 << 20)) return false;
    bool ok = true;
    for (const auto& [name, data] : files) {
        if (!mz_zip_writer_add_mem(&zip, name.c_str(), data.data(), data.size(), MZ_DEFAULT_COMPRESSION)) {
            ok = false;
            break;
        }
    }
    void* buf = nullptr;
    size_t size = 0;
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
    if (ok && size > kMaxPackage) ok = false;
    if (ok) {
        Header h{};
        h.magic = kMagic;
        h.version = kVersion;
        h.steamId = steamId;
        h.created = Now();
        memcpy(h.stateHash, stateHash, 32);
        crypto_blake2b_general(h.digest, 32, nullptr, 0, (const uint8_t*)buf, size);
        BCryptGenRandom(nullptr, h.nonce, sizeof(h.nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        h.zipSize = size;
        out.resize(sizeof(Header) + size);
        memcpy(out.data(), &h, sizeof(h));
        uint8_t key[32];
        Key(key);
        crypto_xchacha20(out.data() + sizeof(Header), (const uint8_t*)buf, size, key, h.nonce);
    }
    mz_zip_writer_end(&zip);
    if (buf) mz_free(buf);
    return ok;
}

bool Unpack(const std::vector<uint8_t>& pkg, FileList& files, uint64_t* steamId, uint8_t stateHash[32]) {
    if (pkg.size() < sizeof(Header)) return false;
    Header h;
    memcpy(&h, pkg.data(), sizeof(h));
    if (h.magic != kMagic || h.version != kVersion || h.zipSize != pkg.size() - sizeof(Header)) return false;
    std::vector<uint8_t> zipBytes((size_t)h.zipSize);
    uint8_t key[32];
    Key(key);
    crypto_xchacha20(zipBytes.data(), pkg.data() + sizeof(Header), zipBytes.size(), key, h.nonce);
    uint8_t digest[32];
    crypto_blake2b_general(digest, 32, nullptr, 0, zipBytes.data(), zipBytes.size());
    if (memcmp(digest, h.digest, 32) != 0) return false;   // damaged or edited
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0)) return false;
    bool ok = true;
    files.clear();
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count && ok; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory) continue;
        size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (!data && st.m_uncomp_size != 0) { ok = false; break; }
        files.emplace_back(st.m_filename, std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + size));
        if (data) mz_free(data);
    }
    mz_zip_reader_end(&zip);
    if (ok && steamId) *steamId = h.steamId;
    if (ok && stateHash) memcpy(stateHash, h.stateHash, 32);
    return ok;
}

} // namespace refix::overlay::sync
