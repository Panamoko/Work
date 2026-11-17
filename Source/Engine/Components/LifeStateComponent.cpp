#include "Engine/Components/LifeStateComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Scene/World.h"

namespace uel {

    void LifeStateComponent::Update(GameContext& ctx)
    {
        HPComponent* hp = ctx.self.GetComponent<HPComponent>();
        if (!hp) return;    // HP の無い物は、いつまでも Alive

        switch (state) {
        case LifeState::Alive:
            if (hp->hp <= 0.0f) {                   // HP が 0：ダウン
                state = LifeState::Downed;
                downTimer = downDuration;
                hp->hp = 0.0f;                      // マイナスまで減っていても 0 にそろえる
            }
            break;

        case LifeState::Downed:
            hp->hp = 0.0f;                          // ダウンの間は HP を 0 に保つ（攻撃の出かかりでマイナスになっても戻す）
            downTimer -= ctx.dt;
            if (downTimer <= 0.0f) {                // 制限時間が過ぎた：Dead（ステージの終わりまで戻らない）
                downTimer = 0.0f;
                state = LifeState::Dead;
            }
            break;

        case LifeState::Dead:
            hp->hp = 0.0f;
            break;
        }
    }

} // namespace uel
