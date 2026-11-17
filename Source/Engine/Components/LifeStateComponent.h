#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"

namespace uel {

    // 生きているか・ダウンしているか・死んでいるか（段階 4）。
    // UE の Lyra では Health Component が「死にかけ・死亡」の状態を持ち、GAS では State.Downed のようなタグで持つ。
    // HPComponent は敵も使うので変えず、状態はこの別の部品に持たせる（0章「コードを調べて分かったこと」）。
    //
    //   Alive  ─(HP が 0)→  Downed  ─(downDuration 秒)→  Dead
    //                          └─(蘇生 reviveTime 秒)→ Alive（HP は最大の reviveHpRatio）
    //
    // 約束：Alive 以外の間は HP を 0 に保つ。だから「HP が残っている ＝ Alive」がいつも成り立ち、
    // 敵の狙い（HP が残っている相手）や Support の回復（HP が残っている味方）は、今のまま正しく動く。
    // 付けるのは PartySetup（Play の始めに、プレイヤーと AI 仲間の3人へ）。値は保存しない。
    // これが無い Entity は今までどおり（HP 0 でプレイヤーならゲームオーバー、AI 仲間なら戦闘不能）。
    //
    // 蘇生（4-2）：蘇生する側（プレイヤー・AI 仲間）が、続けている間は毎フレーム AddReviveProgress を呼ぶ。
    // 呼ばれなくなったら（離れた・やめた・蘇生する側が倒れた）、進み具合は 0 に戻る（UE の「長押しのインタラクション」）。
    // 蘇生されている間は、ダウンの制限時間が止まる（蘇生の途中で時間切れにならない）。
    // AI 仲間は、起こしに向かう前に ReserveRevive で予約する（4-3）。
    enum class LifeState { Alive, Downed, Dead };

    class LifeStateComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<LifeStateComponent>(*this); }
        void Update(GameContext& ctx) override;

        // ---- 値（ユーザーが決めた。2026-11-16）----
        float downDuration = 30.0f;     // ダウンの制限時間（秒）。過ぎると Dead（ステージの終わりまで戻らない）
        float reviveTime = 3.0f;        // 蘇生にかかる時間（秒）
        float reviveHpRatio = 0.5f;     // 蘇生で立ち上がったときの HP（最大の割合）

        LifeState GetState() const { return state; }
        bool IsAlive() const { return state == LifeState::Alive; }
        bool IsDowned() const { return state == LifeState::Downed; }
        bool IsDead() const { return state == LifeState::Dead; }

        // ダウンの残り時間（秒）。ダウンしていなければ 0
        float GetDownTimeLeft() const { return state == LifeState::Downed ? downTimer : 0.0f; }

        // 蘇生を dt 秒ぶん進める（蘇生する側が、続けている間は毎フレーム呼ぶ）。ダウンしていなければ何もしない。
        // reviver は蘇生している人（ほかの人が同じ相手を蘇生しに来ないように覚える。4-3 の AI が見る）。
        // 戻り値：蘇生を進めた（ダウン中だった）
        bool AddReviveProgress(EntityId reviver, float dt);

        // 蘇生の進み具合（0〜1）。蘇生されていなければ 0
        float GetReviveProgress() const { return (state == LifeState::Downed && reviveTime > 0.0f) ? reviveProgress / reviveTime : 0.0f; }
        bool IsBeingRevived() const { return state == LifeState::Downed && reviverId.IsValid(); }
        EntityId GetReviver() const { return reviverId; }

        // 蘇生の予約（4-3。AI 仲間が使う）。起こしに向かっている間は毎フレーム呼ぶ（呼ばれなくなったら少しして切れる）。
        // ほかの人の予約が生きている間は取れない（false）。2人の AI が同じ相手へ走らないようにする（UE の AI でよく使う「予約」）。
        // プレイヤーは予約を見ない（プレイヤーが先に起こし始めたら、予約していた AI のほうがあきらめる）
        bool ReserveRevive(EntityId who);

        // ほかの人が起こしている、または起こしに向かっている（who 自身なら false）
        bool IsTakenByOther(EntityId who) const
        {
            return (reviverId.IsValid() && reviverId != who) || (reservedId.IsValid() && reservedId != who);
        }

    private:
        LifeState state = LifeState::Alive;
        float     downTimer = 0.0f;         // ダウンの残り時間
        float     reviveProgress = 0.0f;    // 蘇生が進んだ時間（秒）
        float     reviveIdle = 0.0f;        // 最後に蘇生が進んでからの時間。少し空いたら、蘇生をやめたとみなす
        EntityId  reviverId;                // 今蘇生している人（無効 = 誰も蘇生していない）
        EntityId  reservedId;               // 起こしに向かっている人（予約。無効 = だれも向かっていない）
        float     reserveIdle = 0.0f;       // 最後に予約が更新されてからの時間。少し空いたら、予約を外す
    };

} // namespace uel
