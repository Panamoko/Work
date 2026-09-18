#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>
#include <string>

namespace uel {

    class MeshRendererComponent : public Component {
    public:
        std::string       assetId;                        // «—ˆ‚ÌAssetQÆi¡‚Í‹ój
        DirectX::XMFLOAT4 color{ 0.8f, 0.8f, 0.8f, 1.0f };
        bool              visible = true;
    };

} // namespace uel