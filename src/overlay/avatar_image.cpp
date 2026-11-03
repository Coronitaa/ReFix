#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "avatar_image.h"
#include "overlay_i18n.h"

#include <windows.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <algorithm>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::image {
namespace {

constexpr float kJpegQuality = 0.85f;

template <typename T>
struct Com {
    T* p = nullptr;
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    ~Com() { Reset(); }
    void Reset() { if (p) { p->Release(); p = nullptr; } }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
    operator T*() const { return p; }
};

IWICImagingFactory* Factory() {
    static IWICImagingFactory* factory = nullptr;
    if (!factory) {
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    }
    return factory;
}

WICBitmapTransformOptions OrientationTransform(IWICBitmapFrameDecode* frame) {
    Com<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(&reader))) return WICBitmapTransformRotate0;
    PROPVARIANT v;
    PropVariantInit(&v);
    WICBitmapTransformOptions out = WICBitmapTransformRotate0;
    const wchar_t* queries[] = { L"/app1/ifd/{ushort=274}", L"/ifd/{ushort=274}" };
    for (const wchar_t* q : queries) {
        if (SUCCEEDED(reader->GetMetadataByName(q, &v)) && v.vt == VT_UI2) {
            switch (v.uiVal) {
            case 2: out = WICBitmapTransformFlipHorizontal; break;
            case 3: out = WICBitmapTransformRotate180; break;
            case 4: out = WICBitmapTransformFlipVertical; break;
            case 5: out = (WICBitmapTransformOptions)(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal); break;
            case 6: out = WICBitmapTransformRotate90; break;
            case 7: out = (WICBitmapTransformOptions)(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal); break;
            case 8: out = WICBitmapTransformRotate270; break;
            default: break;
            }
            PropVariantClear(&v);
            break;
        }
        PropVariantClear(&v);
    }
    return out;
}

// Square, oriented and scaled source in 32bpp BGRA (or RGBA when `rgba`).
bool BuildSquare(IWICBitmapSource* decoded, IWICBitmapFrameDecode* frame, int size, bool rgba,
                 IWICBitmapSource** result) {
    IWICImagingFactory* f = Factory();
    Com<IWICBitmapFlipRotator> rotator;
    IWICBitmapSource* src = decoded;
    if (frame) {
        WICBitmapTransformOptions t = OrientationTransform(frame);
        if (t != WICBitmapTransformRotate0 && SUCCEEDED(f->CreateBitmapFlipRotator(&rotator)) &&
            SUCCEEDED(rotator->Initialize(decoded, t))) {
            src = rotator;
        }
    }
    UINT w = 0, h = 0;
    if (FAILED(src->GetSize(&w, &h)) || w == 0 || h == 0) return false;

    Com<IWICBitmapClipper> clipper;
    if (size > 0 && w != h) {
        const UINT side = std::min(w, h);
        WICRect rc = { (INT)((w - side) / 2), (INT)((h - side) / 2), (INT)side, (INT)side };
        if (FAILED(f->CreateBitmapClipper(&clipper)) || FAILED(clipper->Initialize(src, &rc))) return false;
        src = clipper;
        w = h = side;
    }
    Com<IWICBitmapScaler> scaler;
    if (size > 0 && (w != (UINT)size || h != (UINT)size)) {
        if (FAILED(f->CreateBitmapScaler(&scaler))) return false;
        HRESULT hr = scaler->Initialize(src, size, size, WICBitmapInterpolationModeHighQualityCubic);
        if (FAILED(hr)) hr = scaler->Initialize(src, size, size, WICBitmapInterpolationModeFant);
        if (FAILED(hr)) return false;
        src = scaler;
    }
    IWICFormatConverter* conv = nullptr;
    if (FAILED(f->CreateFormatConverter(&conv))) return false;
    const GUID fmt = rgba ? GUID_WICPixelFormat32bppRGBA : GUID_WICPixelFormat32bppBGRA;
    if (FAILED(conv->Initialize(src, fmt, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        conv->Release();
        return false;
    }
    // Materialise so the intermediate objects can go out of scope.
    IWICBitmap* bmp = nullptr;
    HRESULT hr = f->CreateBitmapFromSource(conv, WICBitmapCacheOnLoad, &bmp);
    conv->Release();
    if (FAILED(hr)) return false;
    *result = bmp;
    return true;
}

bool OpenFrame(const std::wstring& path, IWICBitmapFrameDecode** frame) {
    IWICImagingFactory* f = Factory();
    if (!f) return false;
    Com<IWICBitmapDecoder> decoder;
    if (FAILED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
        return false;
    return SUCCEEDED(decoder->GetFrame(0, frame));
}

bool CopyRgba(IWICBitmapSource* src, Rgba& out) {
    UINT w = 0, h = 0;
    if (FAILED(src->GetSize(&w, &h))) return false;
    out.width = (int)w;
    out.height = (int)h;
    out.pixels.resize((size_t)w * h * 4);
    return SUCCEEDED(src->CopyPixels(nullptr, w * 4, (UINT)out.pixels.size(), out.pixels.data()));
}

bool Encode(IWICBitmapSource* src, const GUID& container, IStream* stream, bool jpeg) {
    IWICImagingFactory* f = Factory();
    Com<IWICBitmapEncoder> encoder;
    if (FAILED(f->CreateEncoder(container, nullptr, &encoder))) return false;
    if (FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache))) return false;
    Com<IWICBitmapFrameEncode> frame;
    Com<IPropertyBag2> props;
    if (FAILED(encoder->CreateNewFrame(&frame, &props))) return false;
    if (jpeg && props) {
        PROPBAG2 opt = {};
        opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_R4;
        v.fltVal = kJpegQuality;
        props->Write(1, &opt, &v);
    }
    if (FAILED(frame->Initialize(props))) return false;
    UINT w = 0, h = 0;
    src->GetSize(&w, &h);
    if (FAILED(frame->SetSize(w, h))) return false;
    WICPixelFormatGUID fmt = jpeg ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&fmt))) return false;
    Com<IWICFormatConverter> conv;
    IWICBitmapSource* toWrite = src;
    if (jpeg) {
        if (FAILED(f->CreateFormatConverter(&conv)) ||
            FAILED(conv->Initialize(src, fmt, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            return false;
        toWrite = conv;
    }
    if (FAILED(frame->WriteSource(toWrite, nullptr))) return false;
    if (FAILED(frame->Commit())) return false;
    return SUCCEEDED(encoder->Commit());
}

} // namespace

bool ImportAvatar(const std::wstring& source, const std::wstring& destPng, std::wstring& error) {
    Com<IWICBitmapFrameDecode> frame;
    if (!OpenFrame(source, &frame)) {
        error = i18n::TW(L"Could not read the image. Try a PNG, JPG or BMP file.",
                         L"No se pudo leer la imagen. Prueba con un archivo PNG, JPG o BMP.");
        return false;
    }
    Com<IWICBitmapSource> square;
    if (!BuildSquare(frame, frame, kAvatarSize, false, &square)) {
        error = i18n::TW(L"Could not convert the image.", L"No se pudo convertir la imagen.");
        return false;
    }
    const std::wstring tmp = destPng + L".tmp";
    Com<IWICStream> stream;
    IWICImagingFactory* f = Factory();
    bool ok = SUCCEEDED(f->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromFilename(tmp.c_str(), GENERIC_WRITE)) &&
              Encode(square, GUID_ContainerFormatPng, stream, false);
    stream.Reset(); // close the file before the rename
    if (!ok || !MoveFileExW(tmp.c_str(), destPng.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        error = i18n::TW(L"Could not save the converted image.", L"No se pudo guardar la imagen convertida.");
        return false;
    }
    ReFixLog("[Overlay] Avatar imported and converted to %dx%d PNG", kAvatarSize, kAvatarSize);
    return true;
}

bool LoadFile(const std::wstring& path, Rgba& out, int size) {
    Com<IWICBitmapFrameDecode> frame;
    if (!OpenFrame(path, &frame)) return false;
    Com<IWICBitmapSource> src;
    if (!BuildSquare(frame, nullptr, size, true, &src)) return false;
    return CopyRgba(src, out);
}

bool DecodeMemory(const uint8_t* data, size_t len, Rgba& out) {
    IWICImagingFactory* f = Factory();
    if (!f || !data || len == 0) return false;
    Com<IWICStream> stream;
    if (FAILED(f->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data), (DWORD)len)))
        return false;
    Com<IWICBitmapDecoder> decoder;
    if (FAILED(f->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) return false;
    Com<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    Com<IWICBitmapSource> src;
    if (!BuildSquare(frame, nullptr, 0, true, &src)) return false;
    return CopyRgba(src, out);
}

bool EncodeJpeg(const std::wstring& path, int size, std::vector<uint8_t>& out) {
    Com<IWICBitmapFrameDecode> frame;
    if (!OpenFrame(path, &frame)) return false;
    Com<IWICBitmapSource> src;
    if (!BuildSquare(frame, nullptr, size, false, &src)) return false;
    Com<IStream> mem;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &mem))) return false;
    if (!Encode(src, GUID_ContainerFormatJpeg, mem, true)) return false;
    HGLOBAL hg = nullptr;
    if (FAILED(GetHGlobalFromStream(mem, &hg))) return false;
    STATSTG st = {};
    mem->Stat(&st, STATFLAG_NONAME);
    const void* p = GlobalLock(hg);
    if (!p) return false;
    out.assign((const uint8_t*)p, (const uint8_t*)p + st.cbSize.QuadPart);
    GlobalUnlock(hg);
    return !out.empty();
}

} // namespace refix::overlay::image
