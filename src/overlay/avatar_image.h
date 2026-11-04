// Avatar images through WIC (Windows Imaging Component): any format Windows can decode (PNG, JPEG, BMP,
// GIF, TIFF, ICO, and HEIC/WebP when their codecs are installed) is cropped to a square and converted.
// All functions need COM initialised on the calling thread.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace refix::overlay::image {

constexpr int kAvatarSize = 184;     // Steam's large avatar size; the stored avatar.png
constexpr int kNetAvatarSize = 128;  // JPEG sent to other LAN players

struct Rgba {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;     // RGBA8, straight alpha, rows top-down
};

// Decodes `source`, applies its EXIF orientation, crops the centred square and writes a kAvatarSize PNG.
bool ImportAvatar(const std::wstring& source, const std::wstring& destPng, std::wstring& error);

// Decodes a file / memory buffer into RGBA, scaled to `size` x `size` when size > 0.
bool LoadFile(const std::wstring& path, Rgba& out, int size = 0);
bool DecodeMemory(const uint8_t* data, size_t len, Rgba& out, int size = 0);

// Square JPEG of an image file, for the LAN avatar exchange.
bool EncodeJpeg(const std::wstring& path, int size, std::vector<uint8_t>& out);

} // namespace refix::overlay::image
