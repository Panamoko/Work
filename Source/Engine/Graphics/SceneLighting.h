#pragma once
#include <DirectXMath.h>
#include <cmath>

namespace uel {
    struct SceneLighting {
        DirectX::XMFLOAT3 direction{ -0.4f, -1.0f, 0.35f }; // 光の進む向き
        DirectX::XMFLOAT3 color{ 1.0f, 0.98f, 0.92f };
        float             ambient = 0.25f;
        float             intensity = 1.0f;
        float             shadowRange = 30.0f;   // 影が出る範囲（カメラの前方に追従する正方形の一辺。m）。広げるほど影が粗くなる

        // 空（環境マップ）。空の描画と、金属などの映り込みの両方に効く
        float             skyExposure = 1.0f;    // 空の明るさ（露出の倍率）。1 で画像のまま
        float             skyRotation = 0.0f;    // 空の回転（Y 軸まわり。度）
    };

    // 空の設定を、シェーダに渡す形にする：x = 明るさ、y = cos(回転)、z = sin(回転)
    // （回転の cos / sin はピクセルごとに計算せず、ここで1回だけ求める）
    inline DirectX::XMFLOAT4 SkyShaderParams(const SceneLighting& light)
    {
        const float rad = DirectX::XMConvertToRadians(light.skyRotation);
        return DirectX::XMFLOAT4(light.skyExposure, std::cos(rad), std::sin(rad), 0.0f);
    }
}