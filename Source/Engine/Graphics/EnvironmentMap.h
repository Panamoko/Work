#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace uel {
    // 周りの景色（環境マップ）。Data/Sprite/IBL_PureSky/ の DDS（キューブマップ・HDR）を読む。
    // どのフォルダを使うかは EnvironmentMap.cpp の kIblFolder。
    //   Sky() : 空の描画に使う画像（skybox.dds）
    // 読めなかったときは nullptr を返す（空はグラデーションで描かれる）。
    class EnvironmentMap {
    public:
        bool Initialize(ID3D11Device* device);
        ID3D11ShaderResourceView* Sky() const { return sky.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sky;
    };
}