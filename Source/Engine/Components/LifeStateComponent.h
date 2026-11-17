#pragma once
#include "Engine/Components/Component.h"

namespace uel {

    // 生きているか・ダウンしているか・死んでいるか（段階 4）。
    // UE の Lyra では Health Component が「死にかけ・死亡」の状態を持ち、GAS では State.Downed のようなタグで持つ。
    // HPComponent は敵も使うので変えず、状態はこの別の部品に持たせる（0章「コードを調べて分かったこと」）。
    //
    //   Alive  ─(HP が 0)→  Downed  ─(downDuration 秒)→  Dead
    //                          └─(蘇生。4-2)→ Alive（HP は最大の reviveHpRatio）
    //
    // 約束：Alive 以外の間は HP を 0 に保つ。だから「HP が残っている ＝ Alive」がいつも成り立ち、
    // 敵の狙い（HP が残っている相手）や Support の回復（HP が残っている味方）は、今のまま正しく動く。
    // 付けるのは PartySetup（Play の始めに、プレイヤーと AI 仲間の3人へ）。値は保存しない。
    // これが無い Entity は今までどおり（HP 0 でプレイヤーならゲームオーバー、AI 仲間なら戦闘不能）。
    enum class LifeState { Alive, Downed, Dead };

    class LifeStateComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<LifeStateComponent>(*this); }
        void Update(GameContext& ctx) override;

        // ---- 値（ユーザーが決めた。2026-11-16）----
        float downDuration = 30.0f;     // ダウンの制限時間（秒）。過ぎると Dead（ステージの終わりまで戻らない）
        float reviveTime = 3.0f;        // 蘇生にかかる時間（秒。4-2 で使う）
        float reviveHpRatio = 0.5f;     // 蘇生で立ち上がったときの HP（最大の割合。4-2 で使う）

        LifeState GetState() const { return state; }
        bool IsAlive() const { return state == LifeState::Alive; }
        bool IsDowned() const { return state == LifeState::Downed; }
        bool IsDead() const { return state == LifeState::Dead; }

        // ダウンの残り時間（秒）。ダウンしていなければ 0
        float GetDownTimeLeft() const { return state == LifeState::Downed ? downTimer : 0.0f; }

    private:
        LifeState state = LifeState::Alive;
        float     downTimer = 0.0f;     // ダウンの残り時間
    };

} // namespace uel
