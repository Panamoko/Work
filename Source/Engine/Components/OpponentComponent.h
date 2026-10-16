#pragma once
#include "Engine/Components/Component.h"
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
        float attackHitDelay = 0.25f;   // 攻撃を始めてから、ダメージが入るまでの時間。グラフに "Hit" の通知が無いときだけ使う
        float attackMoveLock = 0.6f;    // 攻撃を始めてから、この秒数は移動しない（攻撃のクリップの長さに合わせる）
        float deathDelay = 1.5f;        // HP が 0 になってから、消えるまでの時間（死亡のクリップの長さに合わせる）

        // アニメーショングラフに渡すパラメータの名前。
        // 同じ Entity の AnimatorComponent のグラフに、この名前のパラメータがあれば値を書く（無ければ何もしない）。
        // この Component は「どのクリップを再生するか」を知らない。知っているのは、この名前だけ。
        static constexpr const char* kAnimSpeed = "Speed";      // Float  ：いまの移動の速さ（m/s）。止まっていれば 0
        static constexpr const char* kAnimAttack = "Attack";    // Trigger：攻撃を始めた
        static constexpr const char* kAnimDead = "Dead";        // Bool   ：HP が 0 になった（消えるまで true のまま）

        // アニメーショングラフから受け取る通知の名前。
        // 攻撃のクリップの「当たる瞬間」にこの名前の通知を置いておくと、その瞬間にダメージが入る。
        static constexpr const char* kNotifyHit = "Hit";

        // HP が 0 になって、倒れている最中か（弾が当たらない、攻撃しない、動かない）
        bool IsDying() const { return dying; }

        const std::vector<DirectX::XMFLOAT3>& GetPath() const { return path; }   // ★デバッグ描画用
    private:
        float velY = 0.0f;
        float attackTimer = 0.0f;
        float hitTimer = -1.0f;     // 攻撃のダメージを待っている残り時間（マイナス = 攻撃していない）
        bool  hitByNotify = false;  // true：通知 "Hit" を待つ（hitTimer は時間切れ用） / false：hitTimer が 0 になったら当てる
        float moveLockTimer = 0.0f; // 移動を止めている残り時間（0 以下 = 動いてよい）
        bool  dying = false;
        float deathTimer = 0.0f;    // 消えるまでの残り時間（dying のときだけ使う）
        float repathTimer = 0.0f;
        size_t wpIndex = 0;   // 現在向かっている経路インデックス（前進のみ）
        std::vector<DirectX::XMFLOAT3> path;
    };
}
