#pragma once
#include "Engine/Scene/Component.h"
#include <string>

namespace uel {
    // ‚±‚ÌEntity‚ª‚Ç‚ÌPrefab‚©‚çì‚ç‚ê‚½‚©iPrefab‚Æ‚ÌƒŠƒ“ƒNj
    class PrefabInstanceComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<PrefabInstanceComponent>(*this); }
        std::string path;   // —á: Data/Prefabs/Enemy.prefab.json
    };
}