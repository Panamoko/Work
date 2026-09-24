#pragma once
#include "Engine/Scene/Component.h"

namespace uel {
    class HPComponent : public Component {
    public:
        float hp = 100.0f;
        float maxHp = 100.0f;
    };
}