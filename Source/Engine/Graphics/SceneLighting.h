#pragma once
#include <DirectXMath.h>

namespace uel {
    struct SceneLighting {
        DirectX::XMFLOAT3 direction{ -0.4f, -1.0f, 0.35f }; // 光の進む向き
        DirectX::XMFLOAT3 color{ 1.0f, 0.98f, 0.92f };
        float             ambient = 0.25f;
        float             intensity = 1.0f;
        float             shadowRange = 30.0f;   // 影が出る範囲（カメラの前方に追従する正方形の一辺。m）。広げるほど影が粗くなる
    };
}