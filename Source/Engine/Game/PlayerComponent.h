#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>

namespace uel {
    class PlayerComponent : public Component {
    public:
        void Update(GameContext& ctx) override;
        const DirectX::XMFLOAT4X4& GetView() const { return view; }

        float moveSpeed = 4.0f;
        float eyeHeight = 1.6f;
        float eyeForward = 0.5f;
        float groundY = 0.0f;
        float radius = 0.35f;

    private:
        float pitch = 0.0f;
        float velY = 0.0f;
        DirectX::XMFLOAT4X4 view{};
    };
}