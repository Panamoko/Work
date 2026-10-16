#include "Engine/Components/GameRuleComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/GameFlow.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/OpponentComponent.h"
#include "Engine/Components/SpawnerComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Scene/World.h"

namespace uel {

    void GameRuleComponent::Update(GameContext& ctx)
    {
        if (!ctx.flow) return;
        GameFlow& flow = *ctx.flow;

        if (!ended) {
            flow.playTime += ctx.dt;

            Entity* player = nullptr;
            int  opponents = 0;
            bool anySpawner = false, spawnersDone = true;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot) continue;
                if (!player && slot->GetComponent<PlayerComponent>()) player = slot.get();
                if (slot->GetComponent<OpponentComponent>()) ++opponents;
                if (auto* sp = slot->GetComponent<SpawnerComponent>()) {
                    anySpawner = true;
                    if (!sp->IsFinished()) spawnersDone = false;   // まだウェーブが残っている
                }
            }
            if (opponents > 0) seenOpponent = true;   // 敵ゼロのステージで即クリアにならないように

            bool dead = false;
            if (player) if (auto* hp = player->GetComponent<HPComponent>()) dead = (hp->hp <= 0.0f);

            if (dead) { ended = true; won = false; }
            else if ((seenOpponent || anySpawner) && spawnersDone && opponents == 0) { ended = true; won = true; }

            if (ended) {
                flow.lastResult = won ? "CLEAR" : "GAME OVER";
                flow.lastTime = flow.playTime;
                endTimer = endDelay;
            }
        }
        else {
            endTimer -= ctx.dt;                      // 少し余韻（YOU DIED等）を見せてから遷移
            if (endTimer <= 0.0f && flow.requestedScene.empty())
                flow.requestedScene = won ? winScene : loseScene;
        }
    }

} // namespace uel