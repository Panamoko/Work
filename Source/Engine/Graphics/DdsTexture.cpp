#include "Engine/Graphics/DdsTexture.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // ---- DDS ファイルの先頭の形（Microsoft の DDS の仕様どおり） ----
        // ファイルの並び： "DDS "（4 バイト） → DdsHeader（124 バイト） → [DdsHeaderDx10（20 バイト）] → 画素
        struct DdsPixelFormat {
            uint32_t size, flags, fourCC, rgbBitCount, rMask, gMask, bMask, aMask;
        };
        struct DdsHeader {
            uint32_t size, flags, height, width, pitchOrLinearSize, depth, mipMapCount;
            uint32_t reserved1[11];
            DdsPixelFormat ddspf;
            uint32_t caps, caps2, caps3, caps4, reserved2;
        };
        struct DdsHeaderDx10 {
            uint32_t dxgiFormat, resourceDimension, miscFlag, arraySize, miscFlags2;
        };
        static_assert(sizeof(DdsHeader) == 124, "DDS header must be 124 bytes");
        static_assert(sizeof(DdsHeaderDx10) == 20, "DX10 header must be 20 bytes");

        constexpr uint32_t kMagic = 0x20534444; // "DDS "
        constexpr uint32_t kFourCCDx10 = 0x30315844; // "DX10"（この後ろに DdsHeaderDx10 が続く）
        constexpr uint32_t kPfFourCC = 0x4;        // ddspf.fourCC で形式を表している
        constexpr uint32_t kPfRGB = 0x40;       // ddspf の色のマスクで形式を表している
        constexpr uint32_t kCaps2CubeAll = 0xFE00;     // キューブマップの6面がそろっている
        constexpr uint32_t kCaps2Volume = 0x200000;   // 3D テクスチャ（対応しない）
        constexpr uint32_t kMiscTextureCube = 0x4;       // DX10 ヘッダの miscFlag：キューブマップ

        void Report(const char* path, const char* why)
        {
            OutputDebugStringA((std::string("[DDS] ") + why + ": " + path + "\n").c_str());
        }

        // 旧形式のヘッダの「画素の形」を DXGI の形式に直す（対応していなければ UNKNOWN）
        DXGI_FORMAT FormatFromLegacy(const DdsPixelFormat& pf)
        {
            if (pf.flags & kPfFourCC) {
                switch (pf.fourCC) {   // 数字は D3DFORMAT の値
                case 113: return DXGI_FORMAT_R16G16B16A16_FLOAT; // A16B16G16R16F
                case 116: return DXGI_FORMAT_R32G32B32A32_FLOAT; // A32B32G32R32F
                case 112: return DXGI_FORMAT_R16G16_FLOAT;       // G16R16F
                case 115: return DXGI_FORMAT_R32G32_FLOAT;       // G32R32F
                case 114: return DXGI_FORMAT_R32_FLOAT;          // R32F
                default:  return DXGI_FORMAT_UNKNOWN;
                }
            }
            if ((pf.flags & kPfRGB) && pf.rgbBitCount == 32) {
                if (pf.rMask == 0x000000ff && pf.gMask == 0x0000ff00 && pf.bMask == 0x00ff0000) return DXGI_FORMAT_R8G8B8A8_UNORM;
                if (pf.rMask == 0x00ff0000 && pf.gMask == 0x0000ff00 && pf.bMask == 0x000000ff) return DXGI_FORMAT_B8G8R8A8_UNORM;
            }
            return DXGI_FORMAT_UNKNOWN;
        }

        // 1画素のバイト数（対応していない形式は 0）
        UINT BytesPerPixel(DXGI_FORMAT f)
        {
            switch (f) {
            case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
            case DXGI_FORMAT_R32G32_FLOAT:       return 8;
            case DXGI_FORMAT_R16G16_FLOAT:
            case DXGI_FORMAT_R32_FLOAT:
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_UNORM:     return 4;
            default:                             return 0;
            }
        }
    }

    bool LoadDdsTexture(ID3D11Device* device, const char* path,
        ComPtr<ID3D11ShaderResourceView>& srv, DdsTextureInfo* info)
    {
        // ---- ファイルを丸ごと読む ----
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);   // ate：最後に移動して大きさを知る
        if (!ifs) { Report(path, "not found"); return false; }
        const std::streamsize fileSize = ifs.tellg();
        if (fileSize < (std::streamsize)(4 + sizeof(DdsHeader))) { Report(path, "file is too small"); return false; }
        std::vector<uint8_t> data(static_cast<size_t>(fileSize));
        ifs.seekg(0, std::ios::beg);
        if (!ifs.read(reinterpret_cast<char*>(data.data()), fileSize)) { Report(path, "read failed"); return false; }

        // ---- ヘッダ ----
        uint32_t magic = 0;
        DdsHeader h = {};
        std::memcpy(&magic, data.data(), 4);
        std::memcpy(&h, data.data() + 4, sizeof(h));
        if (magic != kMagic || h.size != sizeof(DdsHeader)) { Report(path, "not a DDS file"); return false; }
        if (h.caps2 & kCaps2Volume) { Report(path, "volume textures are not supported"); return false; }

        size_t offset = 4 + sizeof(DdsHeader);
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool isCube = false;
        if ((h.ddspf.flags & kPfFourCC) && h.ddspf.fourCC == kFourCCDx10) {
            if (data.size() < offset + sizeof(DdsHeaderDx10)) { Report(path, "file is too small"); return false; }
            DdsHeaderDx10 d10 = {};
            std::memcpy(&d10, data.data() + offset, sizeof(d10));
            offset += sizeof(d10);
            if (d10.arraySize > 1) { Report(path, "texture arrays are not supported"); return false; }
            format = static_cast<DXGI_FORMAT>(d10.dxgiFormat);
            isCube = (d10.miscFlag & kMiscTextureCube) != 0;
        }
        else {
            format = FormatFromLegacy(h.ddspf);
            isCube = (h.caps2 & kCaps2CubeAll) == kCaps2CubeAll;
        }

        const UINT bpp = BytesPerPixel(format);
        if (bpp == 0) { Report(path, "unsupported format (only uncompressed RGBA16F / RGBA32F / RGBA8 etc.)"); return false; }
        if (h.width == 0 || h.height == 0) { Report(path, "invalid size"); return false; }

        const UINT faces = isCube ? 6u : 1u;
        const UINT mips = (std::max)(1u, h.mipMapCount);

        // ---- 画素：面ごとに、ミップ 0（いちばん大きい）→ 1 → … の順に並んでいる ----
        // D3D のサブリソース番号は「面 × ミップ数 ＋ ミップ」で、ファイルの並びと同じ順になる
        std::vector<D3D11_SUBRESOURCE_DATA> init(faces * mips);
        for (UINT face = 0; face < faces; ++face) {
            for (UINT mip = 0; mip < mips; ++mip) {
                const UINT w = (std::max)(1u, h.width >> mip);
                const UINT hh = (std::max)(1u, h.height >> mip);
                const size_t bytes = (size_t)w * hh * bpp;
                if (offset + bytes > data.size()) { Report(path, "file is too short for its header"); return false; }

                D3D11_SUBRESOURCE_DATA& s = init[face * mips + mip];
                s.pSysMem = data.data() + offset;
                s.SysMemPitch = w * bpp;   // 1行のバイト数
                s.SysMemSlicePitch = 0;
                offset += bytes;
            }
        }

        // ---- テクスチャと SRV を作る ----
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = h.width;
        td.Height = h.height;
        td.MipLevels = mips;
        td.ArraySize = faces;
        td.Format = format;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;   // 読み込んだ後は書き換えない
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags = isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device->CreateTexture2D(&td, init.data(), tex.GetAddressOf()))) { Report(path, "CreateTexture2D failed"); return false; }

        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = format;
        if (isCube) {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            sd.TextureCube.MostDetailedMip = 0;
            sd.TextureCube.MipLevels = mips;
        }
        else {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MostDetailedMip = 0;
            sd.Texture2D.MipLevels = mips;
        }
        if (FAILED(device->CreateShaderResourceView(tex.Get(), &sd, srv.ReleaseAndGetAddressOf()))) { Report(path, "CreateShaderResourceView failed"); return false; }

        if (info) {
            info->width = h.width;
            info->height = h.height;
            info->mipLevels = mips;
            info->isCube = isCube;
            info->format = format;
        }
        return true;
    }
}