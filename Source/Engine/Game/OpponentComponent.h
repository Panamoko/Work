#pragma once
#include "Engine/Scene/Component.h"

namespace uel {
    class OpponentComponent : public Component {
    public:
        void Update(GameContext& ctx) override;
        float moveSpeed = 2.0f;
        float stopRange = 1.2f;
    private:
        float velY = 0.0f;
    };
}