#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Game/GameFlow.h"      // LoseReason
#include <string>

namespace uel {
    // ステージの勝敗判定（UEのGameMode的な役割）
    class GameRuleComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<GameRuleComponent>(*this); }
        void Update(GameContext& ctx) override;

        std::string winScene = "Result";
        std::string loseScene = "Result";
        float       endDelay = 2.0f;      // 決着してから遷移するまでの秒数

    private:
        bool  ended = false, won = false, seenOpponent = false;
        bool  seenBoss = false;           // ボス（BossComponent）が一度でも出てきたか。クリアのときの「ボス撃破」に使う（5-5a）
        float endTimer = 0.0f;
        void  WriteResult(GameContext& ctx, LoseReason reason);  // 決着したときに、結果を GameFlow に書く（5-5a）
    };
}