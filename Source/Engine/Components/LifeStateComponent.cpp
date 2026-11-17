#include "Engine/Components/LifeStateComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Scene/World.h"

namespace uel {
    namespace {

        // 蘇生が進まない時間がこれ（秒）を超えたら、蘇生をやめたとみなして進み具合を 0 に戻す。
        // 1フレームより少し長くしてあるのは、蘇生する側とされる側の Update の順番（どちらが先か）で、
        // 1フレーム呼ばれなかったように見えても途切れないようにするため
        constexpr float kReviveGrace = 0.1f;
        // 予約が更新されない時間がこれ（秒）を超えたら、予約を外す（予約した AI が倒れた・消えた・あきらめたとき）
        constexpr float kReserveGrace = 0.5f;

    } // namespace

    bool LifeStateComponent::ReserveRevive(EntityId who)
    {
        if (state != LifeState::Downed) return false;
        if (reservedId.IsValid() && reservedId != who) return false;    // ほかの人の予約が生きている
        reservedId = who;
        reserveIdle = 0.0f;
        return true;
    }

    bool LifeStateComponent::AddReviveProgress(EntityId reviver, float dt)
    {
        if (state != LifeState::Downed) return false;
        reviveProgress += dt;
        reviveIdle = 0.0f;
        reviverId = reviver;
        return true;
    }

    void LifeStateComponent::Update(GameContext& ctx)
    {
        HPComponent* hp = ctx.self.GetComponent<HPComponent>();
        if (!hp) return;    // HP の無い物は、いつまでも Alive

        switch (state) {
        case LifeState::Alive:
            if (hp->hp <= 0.0f) {                   // HP が 0：ダウン
                state = LifeState::Downed;
                downTimer = downDuration;
                reviveProgress = 0.0f;
                reviverId = EntityId{};
                reservedId = EntityId{};
                hp->hp = 0.0f;                      // マイナスまで減っていても 0 にそろえる
            }
            break;

        case LifeState::Downed:
            hp->hp = 0.0f;                          // ダウンの間は HP を 0 に保つ（攻撃の出かかりでマイナスになっても戻す）

            // 予約（4-3）：更新されなくなったら外す
            reserveIdle += ctx.dt;
            if (reserveIdle > kReserveGrace) reservedId = EntityId{};

            // 蘇生：だれかが続けている間は進む。途切れたら 0 に戻る
            reviveIdle += ctx.dt;
            if (reviveIdle > kReviveGrace) {
                reviveProgress = 0.0f;
                reviverId = EntityId{};
            }
            if (reviverId.IsValid() && reviveProgress >= reviveTime) {     // 蘇生できた：HP を最大の reviveHpRatio にして立ち上がる
                state = LifeState::Alive;
                hp->hp = hp->maxHp * reviveHpRatio;
                reviveProgress = 0.0f;
                reviverId = EntityId{};
                reservedId = EntityId{};
                break;
            }

            // 制限時間：蘇生されている間は止まる
            if (!reviverId.IsValid()) downTimer -= ctx.dt;
            if (downTimer <= 0.0f) {                // 制限時間が過ぎた：Dead（ステージの終わりまで戻らない）
                downTimer = 0.0f;
                state = LifeState::Dead;
                reservedId = EntityId{};
            }
            break;

        case LifeState::Dead:
            hp->hp = 0.0f;
            break;
        }
    }

} // namespace uel
