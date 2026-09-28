#pragma once
#include "Engine/Scene/Component.h"

namespace uel {

    class NavObstacleComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<NavObstacleComponent>(*this); }
        float halfX = 1.0f;   // XZ平面の半径（メートル）
        float halfZ = 1.0f;
    };

} // namespace uel