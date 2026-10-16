#pragma once
#include "Engine/Components/Component.h"
#include <DirectXMath.h>

namespace uel {
    class PlayerComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<PlayerComponent>(*this); }

        void Update(GameContext& ctx) override;
        const DirectX::XMFLOAT4X4& GetView() const { return view; }
        float GetHitMarker() const { return hitMarker; }   // ★
        bool  GetHeadshot() const { return lastHeadshot; }

        float moveSpeed = 4.0f;
        float eyeHeight = 1.6f;
        float eyeForward = 0.5f;
        float groundY = 0.0f;
        float radius = 0.35f;

        // アニメーショングラフに渡すパラメータの名前。
        // 同じ Entity の AnimatorComponent のグラフに、この名前のパラメータがあれば値を書く（無ければ何もしない）。
        // Speed / Attack / Dead は、敵（OpponentComponent）と同じ名前にしてある。
        static constexpr const char* kAnimSpeed = "Speed";          // Float  ：いまの移動の速さ（m/s）。壁に止められていれば 0
        static constexpr const char* kAnimMoveX = "MoveX";          // Float  ：体の向きから見た横の動き（右 +1 / 左 -1）
        static constexpr const char* kAnimMoveY = "MoveY";          // Float  ：体の向きから見た前後の動き（前 +1 / 後ろ -1）
        static constexpr const char* kAnimGrounded = "Grounded";    // Bool   ：地面に立っている
        static constexpr const char* kAnimJump = "Jump";            // Trigger：跳んだ
        static constexpr const char* kAnimAttack = "Attack";        // Trigger：撃った
        static constexpr const char* kAnimDead = "Dead";            // Bool   ：HP が 0
    private:
        float pitch = 0.0f;
        float velY = 0.0f;
        float hitMarker = 0.0f;   // ★命中演出タイマー
        bool  lastHeadshot = false;
        DirectX::XMFLOAT4X4 view{};
    };
}