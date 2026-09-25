#pragma once
#include <DirectXMath.h>

namespace uel {
    struct SceneLighting {
        DirectX::XMFLOAT3 direction{ -0.4f, -1.0f, 0.35f }; // Œõ‚Ìi‚ŞŒü‚«
        DirectX::XMFLOAT3 color{ 1.0f, 0.98f, 0.92f };
        float             ambient = 0.25f;
        float             intensity = 1.0f;
    };
}