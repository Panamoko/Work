#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace uel {
    // DDS ファイルを読み込んで、シェーダから読めるテクスチャ（SRV）にする。
    // 対応しているのは「圧縮していない形式」だけ：
    //   RGBA16F / RGBA32F / RG16F / RG32F / R32F / RGBA8 / BGRA8
    //   2D とキューブマップ、ミップマップ付き、旧形式のヘッダと DX10 ヘッダの両方
    // 失敗したときは false を返し、VS の「出力」に [DDS] で始まる理由を出す。
    struct DdsTextureInfo {
        UINT width = 0;
        UINT height = 0;
        UINT mipLevels = 0;
        bool isCube = false;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    };

    bool LoadDdsTexture(ID3D11Device* device, const char* path,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, DdsTextureInfo* info = nullptr);
}