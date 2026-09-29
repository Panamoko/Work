#pragma once
#include "Engine/Scene/Component.h"
#include "Engine/Scene/TransformComponent.h"
#include <DirectXMath.h>

namespace uel {
    // キャラの当たり判定（胴体カプセル＋頭の球）。値は足元からの高さで、Transform の Scale.y で拡大縮小
    class HitboxComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<HitboxComponent>(*this); }

        float bodyBottom = 0.3f;      // 胴体カプセルの下端（中心線）
        float bodyTop = 1.3f;         // 胴体カプセルの上端（中心線）
        float bodyRadius = 0.35f;
        float headHeight = 1.6f;      // 頭の球の中心
        float headRadius = 0.22f;
        float headMultiplier = 2.0f;  // ヘッドショット倍率

        struct WorldShapes { DirectX::XMFLOAT3 bodyA, bodyB, headC; float bodyR, headR; };

        WorldShapes GetWorldShapes(const TransformComponent& t) const {
            float s = t.scale.y;
            WorldShapes w;
            w.bodyA = { t.position.x, t.position.y + bodyBottom * s, t.position.z };
            w.bodyB = { t.position.x, t.position.y + bodyTop * s,    t.position.z };
            w.headC = { t.position.x, t.position.y + headHeight * s, t.position.z };
            w.bodyR = bodyRadius * s;
            w.headR = headRadius * s;
            return w;
        }
    };
}