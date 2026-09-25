#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>

namespace uel {
    class PlayerComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<PlayerComponent>(*this); }

        void Update(GameContext& ctx) override;
        const DirectX::XMFLOAT4X4& GetView() const { return view; }
        float GetHitMarker() const { return hitMarker; }   // ★

        float moveSpeed = 4.0f;
        float eyeHeight = 1.6f;
        float eyeForward = 0.5f;
        float groundY = 0.0f;
        float radius = 0.35f;
    private:
        float pitch = 0.0f;
        float velY = 0.0f;
        float hitMarker = 0.0f;   // ★命中演出タイマー
        DirectX::XMFLOAT4X4 view{};
    };
}