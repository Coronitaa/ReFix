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
#include "../avatar_image.h"
#include "../../csgo_schema.h"

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
            bool paintVtf = ext == "vtf" && lpath.find("customization/paints") != std::string::npos;
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
                if (econPng || paintVtf) {
                    std::string key = lpath + "/" + Lower(name) + "." + ext;
                    VpkEntry& e = idx.files[key];
                    e.archive = archive;
                    e.offset = offset;
                    e.length = length;
                    e.preload.assign(data.begin() + p, data.begin() + p + preload);
                    if (paintVtf) idx.paints.emplace(Lower(name), key);
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

ImTextureID ItemImage(const refix::csgo::MarketItem& item) {
    std::string rendered = RenderedPath(item);
    if (!rendered.empty()) return Load(rendered, false);
    return BaseImage(item.imageInventory);
}

void DrawItemArt(ImDrawList* dl, const refix::csgo::MarketItem& item, ImVec2 min, ImVec2 max, float rounding) {
    std::string rendered = RenderedPath(item);
    if (!rendered.empty()) {
        if (ImTextureID t = Load(rendered, false)) FitImage(dl, t, SizeOf(rendered), min, max);
        return;
    }
    if (item.paintKit != 0) {
        // No pre-rendered icon in this build: show the paint pattern behind the base weapon.
        auto it = Index().paints.find(Lower(item.pattern));
        if (it != Index().paints.end()) {
            if (ImTextureID sw = Load(it->second, true)) {
                dl->AddImageRounded(sw, min, max, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f), IM_COL32(255, 255, 255, 150), rounding);
                dl->AddRectFilledMultiColor(min, max, IM_COL32(0, 0, 0, 40), IM_COL32(0, 0, 0, 40), IM_COL32(0, 0, 0, 150),
                                            IM_COL32(0, 0, 0, 150));
            }
        }
    }
    std::string base = BasePath(item.imageInventory);
    if (!base.empty())
        if (ImTextureID t = Load(base, false)) FitImage(dl, t, SizeOf(base), min, max);
}

} // namespace refix::overlay::csgo_icons
