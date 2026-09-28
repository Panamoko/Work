#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>
#include <string>

namespace uel {

    class SkinnedMeshComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<SkinnedMeshComponent>(*this); }

        std::string       assetPath;              // スキン付き .glb
        std::string       animPath;   // アニメ別ファイル(rig glb)。空ならassetPath内蔵アニメ
        int               clipIndex = 0;
        float             time = 0.0f;
        float             speed = 1.0f;
        bool              playing = true;
        bool              loop = true;
        bool              visible = true;
        DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

} // namespace uel