#pragma once
#include "Engine/Scene/Component.h"
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
        float endTimer = 0.0f;
    };
}