#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace uel {
    // 周りの景色（環境マップ）。Data/Sprite/IBL_PureSky/ の DDS を読む。
    // どのフォルダを使うかは EnvironmentMap.cpp の kIblFolder。
    //   Sky()      : 空の描画に使う画像（skybox.dds。キューブマップ・HDR）
    //   Specular() : 映り込み用。空を粗さごとにぼかしたもの（specular_pmrem.dds。キューブマップ・HDR・ミップ付き）
    //   BrdfLut()  : 映り込みの強さの表（lut_ggx.dds。2D）
    // 読めなかったものは nullptr を返す（空はグラデーション、映り込みは前の計算になる）。
    class EnvironmentMap {
    public:
        bool Initialize(ID3D11Device* device);
        ID3D11ShaderResourceView* Sky() const { return sky.Get(); }
        ID3D11ShaderResourceView* Specular() const { return specular.Get(); }
        ID3D11ShaderResourceView* BrdfLut() const { return brdfLut.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sky;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specular;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brdfLut;
    };
}