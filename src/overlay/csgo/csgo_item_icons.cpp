#include "csgo_item_icons.h"

#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../overlay_host.h"
#include "../overlay_theme.h"
#include "../avatar_image.h"
#include "../../csgo_schema.h"
#include "../../unreal_steam_emu.h"
#include "../../steam_gc_emu.h"
#include <nlohmann/json.hpp>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::csgo_icons {

namespace {

constexpr int kLoadsPerFrame = 4;  // decoding is synchronous on the render thread: keep scrolling smooth
constexpr int kSwatchMaxSize = 256;

struct VpkEntry {
    uint16_t archive = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    std::vector<uint8_t> preload;
};

struct VpkIndex {
    std::wstring gameDir;  // ...\csgo\ (loose files)
    std::wstring dirFile;  // ...\csgo\pak01_dir.vpk
    uint32_t dataOffset = 0;
    std::unordered_map<std::string, VpkEntry> files;      // lowercase "path/name.ext"
    std::unordered_map<std::string, std::string> paints;  // pattern name -> vtf path
    // Glove finishes are materials (customcharacter .vmt: palette colours + pattern), not paint textures.
    std::unordered_map<std::string, std::string> glovePaints;  // pattern name -> vmt path
};

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

constexpr uint32_t kMaxFileBytes = 32u << 20;  // econ PNGs and paint VTFs are far smaller; guards corrupt entries

bool ReadRange(const std::wstring& file, uint64_t offset, uint32_t length, std::vector<uint8_t>& out) {
    if (length > kMaxFileBytes) return false;
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    f.seekg((std::streamoff)offset);
    size_t old = out.size();
    out.resize(old + length);
    f.read((char*)out.data() + old, length);
    return (uint32_t)f.gcount() == length;
}

// VPK directory tree (v1 and v2): ext { path { name entry } }, each list ended by an empty string.
void LoadIndex(VpkIndex& idx) {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1) + L"csgo\\";
    idx.gameDir = dir;
    idx.dirFile = dir + L"pak01_dir.vpk";

    std::ifstream f(idx.dirFile, std::ios::binary);
    if (!f) { ReFixLog("[CSGOIcons] pak01_dir.vpk not found, only loose files will be used"); return; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() < 12) return;
    uint32_t sig, version, treeSize;
    memcpy(&sig, data.data(), 4);
    memcpy(&version, data.data() + 4, 4);
    memcpy(&treeSize, data.data() + 8, 4);
    if (sig != 0x55AA1234 || (version != 1 && version != 2)) return;
    size_t p = version == 1 ? 12 : 28;
    size_t end = (std::min)(data.size(), p + treeSize);
    idx.dataOffset = (uint32_t)end;

    auto readStr = [&](std::string& s) {
        size_t z = p;
        while (z < end && data[z]) ++z;
        if (z >= end) return false;
        s.assign((const char*)data.data() + p, z - p);
        p = z + 1;
        return true;
    };
    std::string ext, path, name;
    while (readStr(ext) && !ext.empty()) {
        while (readStr(path) && !path.empty()) {
            std::string lpath = Lower(path);
            bool econPng = ext == "png" && (lpath.rfind("resource/flash/econ", 0) == 0 || lpath.rfind("panorama/images/econ", 0) == 0);
            bool paintVtf = ext == "vtf" && (lpath.find("customization/paints") != std::string::npos ||
                                             lpath.find("customization/materials") != std::string::npos ||
                                             lpath.find("customization/glove") != std::string::npos);
            bool gloveVmt = ext == "vmt" && lpath.find("customization/paints_gloves") != std::string::npos;
            while (readStr(name) && !name.empty()) {
                if (p + 18 > end) return;
                uint16_t preload, archive, term;
                uint32_t offset, length;
                memcpy(&preload, data.data() + p + 4, 2);
                memcpy(&archive, data.data() + p + 6, 2);
                memcpy(&offset, data.data() + p + 8, 4);
                memcpy(&length, data.data() + p + 12, 4);
                memcpy(&term, data.data() + p + 16, 2);
                p += 18;
                if (term != 0xFFFF || p + preload > end) return;
                if (econPng || paintVtf || gloveVmt) {
                    std::string key = lpath + "/" + Lower(name) + "." + ext;
                    VpkEntry& e = idx.files[key];
                    e.archive = archive;
                    e.offset = offset;
                    e.length = length;
                    e.preload.assign(data.begin() + p, data.begin() + p + preload);
                    if (paintVtf) {
                        std::string lname = Lower(name);
                        if (lname.find("_normal") == std::string::npos) {
                            idx.paints.emplace(lname, key);
                            size_t ppos = key.find("/paints/");
                            if (ppos != std::string::npos) {
                                std::string sub = key.substr(ppos + 8);
                                if (sub.size() > 4) sub = sub.substr(0, sub.size() - 4);
                                idx.paints.emplace(Lower(sub), key);
                                size_t wpos = sub.find("workshop/");
                                if (wpos != std::string::npos) {
                                    idx.paints.emplace(Lower(sub.substr(wpos)), key);
                                }
                            }
                        }
                    }
                    if (gloveVmt) idx.glovePaints.emplace(Lower(name), key);
                }
                p += preload;
            }
        }
    }
    ReFixLog("[CSGOIcons] Indexed %zu econ images and %zu paint textures from pak01_dir.vpk", idx.files.size() - idx.paints.size(),
             idx.paints.size());
}

VpkIndex& Index() {
    static VpkIndex idx;
    static bool loaded = false;
    if (!loaded) { loaded = true; LoadIndex(idx); }
    return idx;
}

bool Exists(const std::string& path) {
    static std::unordered_map<std::string, bool> cache;  // render thread only; asked every frame per visible card
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    VpkIndex& idx = Index();
    bool found = idx.files.count(path) != 0;
    if (!found) {
        std::wstring loose = idx.gameDir + std::wstring(path.begin(), path.end());
        std::replace(loose.begin(), loose.end(), L'/', L'\\');
        found = GetFileAttributesW(loose.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    return cache[path] = found;
}

bool ReadGameFile(const std::string& path, std::vector<uint8_t>& out) {
    VpkIndex& idx = Index();
    std::wstring loose = idx.gameDir + std::wstring(path.begin(), path.end());
    std::replace(loose.begin(), loose.end(), L'/', L'\\');
    std::ifstream lf(loose, std::ios::binary);
    if (lf) {
        out.assign((std::istreambuf_iterator<char>(lf)), std::istreambuf_iterator<char>());
        return !out.empty();
    }
    auto it = idx.files.find(path);
    if (it == idx.files.end()) return false;
    const VpkEntry& e = it->second;
    out = e.preload;
    if (e.length == 0) return !out.empty();
    if (e.archive == 0x7FFF) return ReadRange(idx.dirFile, (uint64_t)idx.dataOffset + e.offset, e.length, out);
    wchar_t archive[32];
    swprintf(archive, 32, L"pak01_%03u.vpk", (unsigned)e.archive);
    return ReadRange(idx.gameDir + archive, e.offset, e.length, out);
}

// ---- VTF (paint patterns) ---------------------------------------------------------------------------------

enum VtfFormat { RGBA8888 = 0, ABGR8888 = 1, RGB888 = 2, BGR888 = 3, ARGB8888 = 11, BGRA8888 = 12, DXT1 = 13, DXT3 = 14,
                 DXT5 = 15, BGRX8888 = 16, DXT1A = 20 };

uint64_t VtfImageSize(int fmt, int w, int h) {
    uint64_t bw = (uint64_t)(std::max)(1, (w + 3) / 4), bh = (uint64_t)(std::max)(1, (h + 3) / 4);
    switch (fmt) {
    case DXT1: case DXT1A: return bw * bh * 8;
    case DXT3: case DXT5: return bw * bh * 16;
    case RGB888: case BGR888: return (uint64_t)w * h * 3;
    case RGBA8888: case ABGR8888: case ARGB8888: case BGRA8888: case BGRX8888: return (uint64_t)w * h * 4;
    default: return 0;
    }
}

void Rgb565(uint16_t c, uint8_t* o) {
    o[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
    o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
    o[2] = (uint8_t)((c & 31) * 255 / 31);
}

void DecodeDxtColor(const uint8_t* b, uint8_t pal[4][4], bool dxt1) {
    uint16_t c0 = (uint16_t)(b[0] | b[1] << 8), c1 = (uint16_t)(b[2] | b[3] << 8);
    Rgb565(c0, pal[0]);
    Rgb565(c1, pal[1]);
    pal[0][3] = pal[1][3] = 255;
    for (int k = 0; k < 3; ++k) {
        if (!dxt1 || c0 > c1) {
            pal[2][k] = (uint8_t)((2 * pal[0][k] + pal[1][k]) / 3);
            pal[3][k] = (uint8_t)((pal[0][k] + 2 * pal[1][k]) / 3);
        } else {
            pal[2][k] = (uint8_t)((pal[0][k] + pal[1][k]) / 2);
            pal[3][k] = 0;
        }
    }
    pal[2][3] = 255;
    pal[3][3] = (!dxt1 || c0 > c1) ? 255 : 0;
}

bool DecodeVtfLevel(const uint8_t* src, int fmt, int w, int h, image::Rgba& out) {
    out.width = w;
    out.height = h;
    out.pixels.assign((size_t)w * h * 4, 255);
    uint8_t* px = out.pixels.data();
    if (fmt == DXT1 || fmt == DXT1A || fmt == DXT3 || fmt == DXT5) {
        int blockBytes = (fmt == DXT1 || fmt == DXT1A) ? 8 : 16;
        for (int by = 0; by < (h + 3) / 4; ++by) {
            for (int bx = 0; bx < (w + 3) / 4; ++bx, src += blockBytes) {
                const uint8_t* color = blockBytes == 16 ? src + 8 : src;
                uint8_t pal[4][4];
                DecodeDxtColor(color, pal, blockBytes == 8);
                uint8_t alpha[16];
                for (int i = 0; i < 16; ++i) alpha[i] = 255;
                if (fmt == DXT3) {
                    for (int i = 0; i < 16; ++i) alpha[i] = (uint8_t)(((src[i / 2] >> ((i & 1) * 4)) & 15) * 17);
                } else if (fmt == DXT5) {
                    uint8_t a[8] = { src[0], src[1] };
                    for (int i = 2; i < 8; ++i)
                        a[i] = a[0] > a[1] ? (uint8_t)(((8 - i) * a[0] + (i - 1) * a[1]) / 7)
                                           : (i < 6 ? (uint8_t)(((6 - i) * a[0] + (i - 1) * a[1]) / 5) : (uint8_t)(i == 6 ? 0 : 255));
                    uint64_t bits = 0;
                    for (int i = 0; i < 6; ++i) bits |= (uint64_t)src[2 + i] << (8 * i);
                    for (int i = 0; i < 16; ++i) alpha[i] = a[(bits >> (3 * i)) & 7];
                }
                uint32_t idxBits = (uint32_t)(color[4] | color[5] << 8 | color[6] << 16 | (uint32_t)color[7] << 24);
                for (int i = 0; i < 16; ++i) {
                    int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                    if (x >= w || y >= h) continue;
                    const uint8_t* c = pal[(idxBits >> (2 * i)) & 3];
                    uint8_t* d = px + ((size_t)y * w + x) * 4;
                    d[0] = c[0]; d[1] = c[1]; d[2] = c[2];
                    d[3] = (uint8_t)(c[3] * alpha[i] / 255);
                }
            }
        }
        return true;
    }
    int bpp = (fmt == RGB888 || fmt == BGR888) ? 3 : 4;
    for (int i = 0; i < w * h; ++i, src += bpp) {
        uint8_t* d = px + (size_t)i * 4;
        switch (fmt) {
        case RGBA8888: d[0] = src[0]; d[1] = src[1]; d[2] = src[2]; d[3] = src[3]; break;
        case ABGR8888: d[0] = src[3]; d[1] = src[2]; d[2] = src[1]; d[3] = src[0]; break;
        case ARGB8888: d[0] = src[1]; d[1] = src[2]; d[2] = src[3]; d[3] = src[0]; break;
        case BGRA8888: d[0] = src[2]; d[1] = src[1]; d[2] = src[0]; d[3] = src[3]; break;
        case BGRX8888: d[0] = src[2]; d[1] = src[1]; d[2] = src[0]; break;
        case RGB888: d[0] = src[0]; d[1] = src[1]; d[2] = src[2]; break;
        case BGR888: d[0] = src[2]; d[1] = src[1]; d[2] = src[0]; break;
        default: return false;
        }
    }
    return true;
}

// Decodes the first mip level no larger than kSwatchMaxSize of a 2D VTF (7.0 to 7.5).
bool DecodeVtf(const std::vector<uint8_t>& d, image::Rgba& out) {
    if (d.size() < 64 || memcmp(d.data(), "VTF\0", 4) != 0) return false;
    uint32_t verMinor, headerSize, flags, hiFmt, loFmt;
    uint16_t width, height, frames;
    uint8_t mipCount, loW, loH;
    memcpy(&verMinor, &d[8], 4);
    memcpy(&headerSize, &d[12], 4);
    memcpy(&width, &d[16], 2);
    memcpy(&height, &d[18], 2);
    memcpy(&flags, &d[20], 4);
    memcpy(&frames, &d[24], 2);
    memcpy(&hiFmt, &d[52], 4);
    mipCount = d[56];
    memcpy(&loFmt, &d[57], 4);
    loW = d[61];
    loH = d[62];
    if (frames == 0) frames = 1;
    int faces = (flags & 0x4000) ? 6 : 1;  // TEXTUREFLAGS_ENVMAP
    if (mipCount == 0 || width == 0 || height == 0 || width > 16384 || height > 16384 || frames > 64) return false;
    if (VtfImageSize((int)hiFmt, width, height) == 0) return false;

    uint64_t hiOffset = headerSize + (loFmt == 0xFFFFFFFF ? 0 : VtfImageSize((int)loFmt, loW, loH));
    if (verMinor >= 3 && d.size() >= 80) {  // 7.3+: resource directory, tag 0x30 = high-res image data
        uint32_t numRes;
        memcpy(&numRes, &d[68], 4);
        for (uint32_t r = 0; r < numRes && r < 32 && 80 + (size_t)r * 8 + 8 <= d.size(); ++r) {
            const uint8_t* e = &d[80 + r * 8];
            uint32_t off;
            memcpy(&off, e + 4, 4);
            if (e[0] == 0x30 && e[1] == 0 && e[2] == 0) hiOffset = off;
        }
    }

    int level = 0;
    while (level + 1 < mipCount && ((width >> level) > kSwatchMaxSize || (height >> level) > kSwatchMaxSize)) ++level;
    uint64_t offset = hiOffset;
    for (int m = mipCount - 1; m > level; --m)
        offset += VtfImageSize((int)hiFmt, (std::max)(1, width >> m), (std::max)(1, height >> m)) * frames * faces;
    int w = (std::max)(1, width >> level), h = (std::max)(1, height >> level);
    uint64_t need = VtfImageSize((int)hiFmt, w, h);
    if (offset > d.size() || need > d.size() - offset) return false;
    return DecodeVtfLevel(&d[(size_t)offset], (int)hiFmt, w, h, out);
}

// ---- Texture cache -----------------------------------------------------------------------------------------

int g_budget = kLoadsPerFrame;
std::unordered_set<std::string> g_attempted;
std::unordered_map<std::string, ImVec2> g_sizes;

void EnsureCom() {
    thread_local bool done = false;
    if (!done) { done = true; CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
}

ImTextureID Load(const std::string& path, bool vtf) {
    std::string key = "csgo:" + path;
    if (!g_attempted.count(key)) {
        if (g_budget <= 0) return 0;
        --g_budget;
        g_attempted.insert(key);
        EnsureCom();
    }
    return host::Texture(key, [&](image::Rgba& img) {
        try {
            std::vector<uint8_t> bytes;
            if (!ReadGameFile(path, bytes)) return false;
            bool ok = vtf ? DecodeVtf(bytes, img) : image::DecodeMemory(bytes.data(), bytes.size(), img, 0);
            if (ok) g_sizes[key] = ImVec2((float)img.width, (float)img.height);
            return ok;
        } catch (...) {  // a corrupt game file must not take the overlay thread down
            return false;
        }
    });
}

ImVec2 SizeOf(const std::string& path) {
    auto it = g_sizes.find("csgo:" + path);
    return it != g_sizes.end() ? it->second : ImVec2(1, 1);
}

std::string RenderedPath(const refix::csgo::MarketItem& item) {
    if (item.paintKit == 0 || item.weaponName.empty() || item.skinName.empty()) return {};
    std::string base = Lower(item.weaponName + "_" + item.skinName);
    for (const std::string& p : { "resource/flash/econ/default_generated/" + base + "_light_large.png",
                                  "panorama/images/econ/default_generated/" + base + "_light_large_png.png" })
        if (Exists(p)) return p;
    return {};
}

std::string BasePath(const std::string& imageInventory) {
    if (imageInventory.empty()) return {};
    std::string img = Lower(imageInventory);
    for (const std::string& p : { "resource/flash/" + img + ".png", "panorama/images/" + img + "_png.png" })
        if (Exists(p)) return p;
    return {};
}

void FitImage(ImDrawList* dl, ImTextureID tex, ImVec2 texSize, ImVec2 min, ImVec2 max) {
    float bw = max.x - min.x, bh = max.y - min.y;
    float scale = (std::min)(bw / texSize.x, bh / texSize.y);
    ImVec2 sz(texSize.x * scale, texSize.y * scale);
    ImVec2 p(min.x + (bw - sz.x) * 0.5f, min.y + (bh - sz.y) * 0.5f);
    dl->AddImage(tex, p, ImVec2(p.x + sz.x, p.y + sz.y));
}

} // namespace

void BeginFrame() { g_budget = kLoadsPerFrame; }

bool HasRenderedSkin(const refix::csgo::MarketItem& item) { return !RenderedPath(item).empty(); }

ImTextureID BaseImage(const std::string& imageInventory) {
    std::string p = BasePath(imageInventory);
    return p.empty() ? 0 : Load(p, false);
}

ImTextureID PaintSwatch(const std::string& pattern) {
    if (pattern.empty() || pattern == "none") return 0;
    VpkIndex& idx = Index();
    auto it = idx.paints.find(Lower(pattern));
    return it == idx.paints.end() ? 0 : Load(it->second, true);
}

std::string GloveFallbackPath(const refix::csgo::MarketItem& item);
std::string KnifeFallbackPath(const refix::csgo::MarketItem& item);
ImTextureID CompositeSkinImage(const refix::csgo::MarketItem& item);

static std::unordered_map<uint32_t, std::string> s_iicCache;
static std::unordered_map<uint64_t, uint32_t> s_accountLowIdToKey; // ((uint64_t)accountId << 32 | lowId) -> (defIndex << 16 | paintKit)
static std::unordered_map<uint32_t, uint32_t> s_lowIdToKey;        // fallback lowId -> key
static uint64_t s_lastInventoryScanMs = 0;

void RefreshInventoryIndex(const std::wstring& gameDir) {
    uint64_t now = GetTickCount64();
    if (now - s_lastInventoryScanMs < 3000 && !s_accountLowIdToKey.empty()) return;
    s_lastInventoryScanMs = now;

    // 1. Query live in-memory inventory from GC emulator
    uint32_t localAccount = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
    auto liveItems = ReFixGC::GetLocalInventory();
    for (const auto& it : liveItems) {
        if (it.defIndex != 0) {
            uint32_t key = (it.defIndex << 16) | (it.paintKit & 0xFFFF);
            uint32_t lowId = (uint32_t)(it.id & 0xFFFFFFFF);
            if (lowId != 0) {
                s_lowIdToKey[lowId] = key;
                if (localAccount != 0) s_accountLowIdToKey[((uint64_t)localAccount << 32) | lowId] = key;
            }
        }
    }

    // 2. Inspect on-disk inventory.json files across all profiles
    wchar_t appData[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH) > 0) {
        std::wstring refixBase = std::wstring(appData) + L"\\ReFix\\";
        std::vector<std::pair<uint32_t, std::wstring>> invPaths;
        invPaths.push_back({ 0, refixBase + L"Saves\\_pc\\Inventory\\730\\inventory.json" });

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((refixBase + L"Profiles\\*").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                    uint32_t pAcc = (uint32_t)wcstoul(fd.cFileName, nullptr, 16);
                    invPaths.push_back({ pAcc, refixBase + L"Profiles\\" + fd.cFileName + L"\\Inventory\\730\\inventory.json" });
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        for (const auto& entry : invPaths) {
            std::ifstream f(entry.second, std::ios::binary);
            if (!f) continue;
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            if (content.size() >= 3 && (uint8_t)content[0] == 0xEF && (uint8_t)content[1] == 0xBB && (uint8_t)content[2] == 0xBF) {
                content.erase(0, 3);
            }
            try {
                nlohmann::json j = nlohmann::json::parse(content);
                if (j.contains("items") && j["items"].is_array()) {
                    for (const auto& it : j["items"]) {
                        uint32_t def = it.value("defIndex", 0u);
                        uint32_t paint = it.value("paintKit", 0u);
                        uint64_t id = 0;
                        if (it.contains("id")) {
                            if (it["id"].is_number()) id = it["id"].get<uint64_t>();
                            else if (it["id"].is_string()) id = _strtoui64(it["id"].get<std::string>().c_str(), nullptr, 10);
                        }
                        if (def != 0 && id != 0) {
                            uint32_t key = (def << 16) | (paint & 0xFFFF);
                            uint32_t lowId = (uint32_t)(id & 0xFFFFFFFF);
                            uint32_t itemAcc = (uint32_t)(id >> 32);
                            if (itemAcc == 0) itemAcc = entry.first;
                            s_lowIdToKey[lowId] = key;
                            if (itemAcc != 0) s_accountLowIdToKey[((uint64_t)itemAcc << 32) | lowId] = key;
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // 3. Scan cached/ folder for all .iic files
    std::wstring cachedDir = gameDir + L"resource\\flash\\econ\\weapons\\cached\\";
    WIN32_FIND_DATAW cfd;
    HANDLE hCached = FindFirstFileW((cachedDir + L"*.iic").c_str(), &cfd);
    if (hCached != INVALID_HANDLE_VALUE) {
        do {
            if (!(cfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char name[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, cfd.cFileName, -1, name, sizeof(name), nullptr, nullptr);
                char part1[32] = {}, part2[32] = {}, part3[32] = {};
                // format: 10-<acc>-<id>.iic
                if (sscanf_s(name, "%31[^-]-%31[^-]-%31[^.]", part1, (unsigned)sizeof(part1),
                             part2, (unsigned)sizeof(part2), part3, (unsigned)sizeof(part3)) >= 3) {
                    if (_stricmp(part2, "ffffffff") == 0) {
                        uint32_t val = (uint32_t)strtoul(part3, nullptr, 16);
                        if (val != 0) {
                            s_iicCache[val] = std::string("resource/flash/econ/weapons/cached/") + name;
                        }
                    } else {
                        uint32_t acc = (uint32_t)strtoul(part2, nullptr, 16);
                        uint32_t lowId = (uint32_t)strtoul(part3, nullptr, 16);
                        uint64_t accKey = ((uint64_t)acc << 32) | lowId;
                        auto itAcc = s_accountLowIdToKey.find(accKey);
                        if (itAcc != s_accountLowIdToKey.end()) {
                            s_iicCache[itAcc->second] = std::string("resource/flash/econ/weapons/cached/") + name;
                        } else {
                            auto itLow = s_lowIdToKey.find(lowId);
                            if (itLow != s_lowIdToKey.end()) {
                                s_iicCache[itLow->second] = std::string("resource/flash/econ/weapons/cached/") + name;
                            }
                        }
                    }
                }
            }
        } while (FindNextFileW(hCached, &cfd));
        FindClose(hCached);
    }
}

std::string CachedIicPath(const refix::csgo::MarketItem& item) {
    if (item.defIndex == 0) return {};
    VpkIndex& idx = Index();
    RefreshInventoryIndex(idx.gameDir);

    uint32_t key = (item.defIndex << 16) | (item.paintKit & 0xFFFF);

    // 1. Look up in scanned / indexed .iic cache
    auto it = s_iicCache.find(key);
    if (it != s_iicCache.end()) {
        return it->second;
    }

    char buf[128];
    // 2. Direct check for unowned Scaleform preview: 10-ffffffff-<val>.iic
    snprintf(buf, sizeof(buf), "resource/flash/econ/weapons/cached/10-ffffffff-%x.iic", key);
    if (Exists(buf)) return buf;
    for (int wear = 0; wear <= 2; ++wear) {
        snprintf(buf, sizeof(buf), "resource/flash/econ/weapons/cached/10-ffffffff-%x-%x.iic", key, wear);
        if (Exists(buf)) return buf;
    }

    // 3. Direct check for local account inventory items
    uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
    if (accountId != 0) {
        snprintf(buf, sizeof(buf), "resource/flash/econ/weapons/cached/10-%x-%x.iic", accountId, key);
        if (Exists(buf)) return buf;
    }

    return {};
}

ImTextureID LoadIic(const std::string& path) {
    std::string key = "csgo:iic:" + path;
    if (!g_attempted.count(key)) {
        if (g_budget <= 0) return 0;
        --g_budget;
        g_attempted.insert(key);
        EnsureCom();
    }
    return host::Texture(key, [&](image::Rgba& img) {
        std::vector<uint8_t> bytes;
        if (!ReadGameFile(path, bytes) || bytes.size() < 512 * 384 * 4) return false;
        img.width = 512;
        img.height = 384;
        img.pixels.assign(bytes.begin(), bytes.begin() + (512 * 384 * 4));
        // .iic is BGRA32 (D3DFMT_A8R8G8B8) -> swap Red and Blue channels for DXGI R8G8B8A8
        for (size_t i = 0; i < 512 * 384 * 4; i += 4) {
            std::swap(img.pixels[i], img.pixels[i + 2]);
        }
        g_sizes[key] = ImVec2(512.0f, 384.0f);
        return true;
    });
}

// Loads item skin art: prefers official pre-rendered PNG or the game's native ItemIconCache (512x384 RGBA),
// falling back cleanly to the weapon/knife/glove silhouette.
ImTextureID ItemImage(const refix::csgo::MarketItem& item) {
    std::string rendered = RenderedPath(item);
    if (!rendered.empty()) return Load(rendered, false);
    if (item.paintKit != 0) {
        std::string iic = CachedIicPath(item);
        if (!iic.empty()) {
            if (ImTextureID t = LoadIic(iic)) return t;
        }
        if (ImTextureID t = CompositeSkinImage(item)) return t;
    }
    if (ImTextureID t = BaseImage(item.imageInventory)) return t;
    std::string knife = KnifeFallbackPath(item);
    if (!knife.empty()) {
        if (ImTextureID t = Load(knife, false)) return t;
    }
    const std::string glove = GloveFallbackPath(item);
    return glove.empty() ? 0 : Load(glove, false);
}

std::string KnifeFallbackPath(const refix::csgo::MarketItem& item) {
    if (item.category != "Cuchillos" && item.weaponName.find("knife") == std::string::npos && item.weaponName.find("bayonet") == std::string::npos) return {};
    std::string name = Lower(item.weaponName);
    std::string p = "resource/flash/econ/weapons/base_weapons/" + name + ".png";
    if (Exists(p)) return p;
    p = "panorama/images/econ/weapons/base_weapons/" + name + "_png.png";
    if (Exists(p)) return p;
    return BasePath("econ/weapons/base_weapons/weapon_knife");
}

// Base glove silhouette for glove finishes
std::string GloveFallbackPath(const refix::csgo::MarketItem& item) {
    if (item.category != "Guantes" && item.weaponName.find("glove") == std::string::npos) return {};
    std::string name = Lower(item.weaponName);
    if (name.find("bloodhound") != std::string::npos || name.find("hydra") != std::string::npos) {
        if (Exists("resource/flash/econ/weapons/base_weapons/t_gloves.png")) return "resource/flash/econ/weapons/base_weapons/t_gloves.png";
    }
    for (const std::string& p : { "resource/flash/econ/weapons/base_weapons/ct_gloves.png",
                                  "resource/flash/econ/weapons/base_weapons/t_gloves.png",
                                  "resource/flash/econ/wearables/gloves/ct_defaultgloves.png",
                                  "resource/flash/econ/wearables/gloves/t_defaultgloves.png" }) {
        if (Exists(p)) return p;
    }
    return BasePath("econ/weapons/base_weapons/ct_gloves");
}

static bool ParseGloveVmt(const std::vector<uint8_t>& bytes, std::string& outPat, uint32_t& outC0, uint32_t& outC1, uint32_t& outC2, uint32_t& outC3) {
    if (bytes.empty()) return false;
    std::string str(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    size_t ppos = str.find("$pattern");
    if (ppos != std::string::npos) {
        size_t q1 = str.find_first_of("\"'", ppos + 8);
        if (q1 != std::string::npos) {
            size_t q2 = str.find_first_of("\"'\r\n", q1 + 1);
            if (q2 != std::string::npos) {
                std::string p = str.substr(q1 + 1, q2 - q1 - 1);
                size_t ls = p.find_last_of("/\\");
                if (ls != std::string::npos) p = p.substr(ls + 1);
                if (p.size() > 4 && p.substr(p.size() - 4) == ".vtf") p = p.substr(0, p.size() - 4);
                outPat = Lower(p);
            }
        }
    }
    auto parseColor = [&](const char* tag) -> uint32_t {
        size_t pos = str.find(tag);
        if (pos == std::string::npos) return 0;
        size_t b1 = str.find('[', pos);
        if (b1 == std::string::npos) return 0;
        float r = 0, g = 0, b = 0;
        if (sscanf(str.c_str() + b1 + 1, "%f %f %f", &r, &g, &b) >= 3) {
            return ((static_cast<uint32_t>(std::clamp((int)r, 0, 255)) << 16) |
                    (static_cast<uint32_t>(std::clamp((int)g, 0, 255)) << 8)  |
                    (static_cast<uint32_t>(std::clamp((int)b, 0, 255))));
        }
        return 0;
    };
    outC0 = parseColor("$palettecolor1");
    outC1 = parseColor("$palettecolor2");
    outC2 = parseColor("$palettecolor3");
    outC3 = parseColor("$palettecolor4");
    return !outPat.empty() || outC0 != 0 || outC1 != 0;
}

ImTextureID CompositeSkinImage(const refix::csgo::MarketItem& item) {
    if (item.paintKit == 0) return 0;

    std::string basePath = BasePath(item.imageInventory);
    if (basePath.empty()) basePath = KnifeFallbackPath(item);
    if (basePath.empty()) basePath = GloveFallbackPath(item);
    if (basePath.empty()) return 0;

    std::string key = "composite:" + std::to_string(item.defIndex) + ":" + std::to_string(item.paintKit);

    if (!g_attempted.count(key)) {
        if (g_budget <= 0) return 0;
        --g_budget;
        g_attempted.insert(key);
        EnsureCom();
    }

    return host::Texture(key, [&](image::Rgba& out) {
        try {
            std::vector<uint8_t> baseBytes;
            if (!ReadGameFile(basePath, baseBytes)) return false;

            image::Rgba baseImg;
            if (!image::DecodeMemory(baseBytes.data(), baseBytes.size(), baseImg, 0)) return false;
            if (baseImg.width == 0 || baseImg.height == 0) return false;

            const int W = baseImg.width;
            const int H = baseImg.height;

            // Find weapon bounding box
            int minX = W, maxX = 0, minY = H, maxY = 0;
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    uint8_t a = baseImg.pixels[((size_t)y * W + x) * 4 + 3];
                    if (a > 20) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }

            if (maxX <= minX || maxY <= minY) {
                out = std::move(baseImg);
                g_sizes[key] = ImVec2((float)W, (float)H);
                return true;
            }

            int spanX = (std::max)(1, maxX - minX);
            int spanY = (std::max)(1, maxY - minY);

            // Pattern texture
            VpkIndex& idx = Index();
            std::string patPath;
            uint32_t c0 = item.color0;
            uint32_t c1 = item.color1;

            if (item.category == "Guantes" || item.weaponName.find("glove") != std::string::npos) {
                auto itG = idx.glovePaints.find(Lower(item.skinName));
                if (itG == idx.glovePaints.end()) itG = idx.glovePaints.find(Lower(item.pattern));
                if (itG != idx.glovePaints.end()) {
                    std::vector<uint8_t> vmtBytes;
                    if (ReadGameFile(itG->second, vmtBytes)) {
                        std::string gPat;
                        uint32_t gc0 = 0, gc1 = 0, gc2 = 0, gc3 = 0;
                        if (ParseGloveVmt(vmtBytes, gPat, gc0, gc1, gc2, gc3)) {
                            if (gc0 != 0) c0 = gc0;
                            if (gc1 != 0) c1 = gc1;
                            if (!gPat.empty()) {
                                auto itP = idx.paints.find(gPat);
                                if (itP != idx.paints.end()) patPath = itP->second;
                            }
                        }
                    }
                }
            }

            if (patPath.empty() && !item.pattern.empty() && item.pattern != "none") {
                auto itP = idx.paints.find(Lower(item.pattern));
                if (itP != idx.paints.end()) patPath = itP->second;
            }

            image::Rgba patImg;
            bool hasPat = false;
            if (!patPath.empty()) {
                std::vector<uint8_t> patBytes;
                if (ReadGameFile(patPath, patBytes)) {
                    hasPat = DecodeVtf(patBytes, patImg) && patImg.width > 0 && patImg.height > 0;
                }
            }

            // If we have neither a pattern texture nor a finish color, return the crisp base weapon
            if (!hasPat && c0 == 0 && c1 == 0) {
                out = std::move(baseImg);
                g_sizes[key] = ImVec2((float)W, (float)H);
                return true;
            }

            out.width = W;
            out.height = H;
            out.pixels = baseImg.pixels; // start with base weapon

            const int pw = hasPat ? patImg.width : 1;
            const int ph = hasPat ? patImg.height : 1;

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    size_t idx4 = ((size_t)y * W + x) * 4;
                    uint8_t ba = baseImg.pixels[idx4 + 3];
                    if (ba < 20) continue;

                    uint8_t br = baseImg.pixels[idx4 + 0];
                    uint8_t bg = baseImg.pixels[idx4 + 1];
                    uint8_t bb = baseImg.pixels[idx4 + 2];

                    float lum = (0.299f * br + 0.587f * bg + 0.114f * bb) / 255.0f;

                    // Protect dark metal (scope, barrel, bipod, trigger, bolt, dark hardware)
                    int maxC = (std::max)({br, bg, bb});
                    int minC = (std::min)({br, bg, bb});
                    int diff = maxC - minC;

                    if (lum < 0.15f || (lum < 0.22f && diff < 15)) {
                        continue;
                    }

                    // Smooth transition from hardware metal to paintable body
                    float paintFactor = std::clamp((lum - 0.15f) / 0.12f, 0.0f, 1.0f);

                    float pr = 0, pg = 0, pb = 0;
                    if (hasPat) {
                        float u = (float)(x - minX) / (float)spanX;
                        float v = (float)(y - minY) / (float)spanY;
                        int px = std::clamp((int)(u * pw), 0, pw - 1);
                        int py = std::clamp((int)(v * ph), 0, ph - 1);
                        size_t pidx = ((size_t)py * pw + px) * 4;
                        pr = (float)patImg.pixels[pidx + 0];
                        pg = (float)patImg.pixels[pidx + 1];
                        pb = (float)patImg.pixels[pidx + 2];
                    } else {
                        uint32_t useColor = c0 != 0 ? c0 : c1;
                        pr = (float)((useColor >> 16) & 0xFF);
                        pg = (float)((useColor >> 8) & 0xFF);
                        pb = (float)(useColor & 0xFF);
                    }

                    // 3D lighting modulation preserving weapon form, highlights and shadows
                    float lighting = 0.35f + lum * 0.85f;
                    float cr = std::clamp(pr * lighting, 0.0f, 255.0f);
                    float cg = std::clamp(pg * lighting, 0.0f, 255.0f);
                    float cb = std::clamp(pb * lighting, 0.0f, 255.0f);

                    out.pixels[idx4 + 0] = (uint8_t)(br * (1.0f - paintFactor) + cr * paintFactor);
                    out.pixels[idx4 + 1] = (uint8_t)(bg * (1.0f - paintFactor) + cg * paintFactor);
                    out.pixels[idx4 + 2] = (uint8_t)(bb * (1.0f - paintFactor) + cb * paintFactor);
                }
            }

            g_sizes[key] = ImVec2((float)W, (float)H);
            return true;
        } catch (...) {
            return false;
        }
    });
}

void DrawItemArt(ImDrawList* dl, const refix::csgo::MarketItem& item, ImVec2 min, ImVec2 max, float rounding) {
    (void)rounding;
    std::string rendered = RenderedPath(item);
    if (!rendered.empty()) {
        if (ImTextureID t = Load(rendered, false)) { FitImage(dl, t, SizeOf(rendered), min, max); return; }
    }
    if (item.paintKit != 0) {
        std::string iic = CachedIicPath(item);
        if (!iic.empty()) {
            if (ImTextureID t = LoadIic(iic)) { FitImage(dl, t, ImVec2(512.0f, 384.0f), min, max); return; }
        }
        if (ImTextureID t = CompositeSkinImage(item)) { FitImage(dl, t, ImVec2(512.0f, 384.0f), min, max); return; }
    }
    std::string base = BasePath(item.imageInventory);
    if (base.empty()) base = KnifeFallbackPath(item);
    if (base.empty()) base = GloveFallbackPath(item);
    if (!base.empty()) {
        if (ImTextureID t = Load(base, false)) { FitImage(dl, t, SizeOf(base), min, max); return; }
    }
}

} // namespace refix::overlay::csgo_icons
