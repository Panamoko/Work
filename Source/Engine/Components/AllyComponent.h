#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"
#include <DirectXMath.h>
#include <vector>

namespace uel {

    // AI 仲間の「頭」（UE の AIController に当たる）。どのロールでも共通の判断をここに書く。
    // 「何ができるか」（武器・アビリティ）は、ロールの Component（Tank / DPS / Support）に持たせる予定。
    //
    // 今できること（1-1）：プレイヤーを経路探索で追いかけ、近くまで来たら止まって、プレイヤーのほうを向く。
    // まだ無いこと：攻撃・回復・蘇生、陣営（敵はまだ AI 仲間を狙わない）、壁との当たり判定（経路だけで避ける）。
    class AllyComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<AllyComponent>(*this); }
        void Update(GameContext& ctx) override;

        // ---- 設定 ----
        float moveSpeed = 4.5f;         // プレイヤー（4.0）より少し速くして、離されても追いつけるようにする
        float followDistance = 3.0f;    // 追いかけている間、これより近づいたら止まる
        float resumeDistance = 4.5f;    // 止まっている間、これより離れたら動き出す（止まる距離との差で、動く・止まるを繰り返さない）
        float repathInterval = 0.5f;    // 経路を計算し直す間隔（秒）

        // アニメーショングラフに渡すパラメータの名前（Player / Opponent と同じ名前）
        static constexpr const char* kAnimSpeed = "Speed";      // Float：いまの移動の速さ（m/s）。止まっていれば 0

    private:
        // ---- 実行時の状態（保存しない）----
        EntityId leader;                            // 追いかける相手（プレイヤー）。毎フレーム World を探さないように覚えておく
        std::vector<DirectX::XMFLOAT3> path;        // 今の経路（マスの中心の並び）
        size_t   wpIndex = 0;                       // 次に向かう点
        float    repathTimer = 0.0f;
        DirectX::XMFLOAT3 pathGoal{ 0.0f, 0.0f, 0.0f };    // 今の経路を作ったときの相手の位置
        bool     following = false;                 // いま追いかけている（動いている）
        bool     grounded = false;                  // 地面に立っている（止まって立っている間は、地面を調べ直さない）
        float    velY = 0.0f;
    };

} // namespace uel
