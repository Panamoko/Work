#pragma once
#include "Engine/Scene/Component.h"
#include <DirectXMath.h>
#include <vector>

namespace uel {
    class OpponentComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<OpponentComponent>(*this); }
        void Update(GameContext& ctx) override;

        float moveSpeed = 2.0f;
        float stopRange = 1.2f;
        float attackRange = 1.5f;
        float attackDamage = 10.0f;
        float attackInterval = 1.0f;

        const std::vector<DirectX::XMFLOAT3>& GetPath() const { return path; }   // ★デバッグ描画用
    private:
        float velY = 0.0f;
        float attackTimer = 0.0f;
        float repathTimer = 0.0f;
        size_t wpIndex = 0;   // 現在向かっている経路インデックス（前進のみ）
        std::vector<DirectX::XMFLOAT3> path;
    };
}