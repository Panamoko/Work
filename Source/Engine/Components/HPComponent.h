#pragma once
#include "Engine/Components/Component.h"

namespace uel {
    class HPComponent : public Component {
    public:

        std::unique_ptr<Component> Clone() const override { return std::make_unique<HPComponent>(*this); }

        float hp = 100.0f;
        float maxHp = 100.0f;
    };
}