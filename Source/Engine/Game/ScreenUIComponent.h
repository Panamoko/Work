#pragma once
#include "Engine/Scene/Component.h"
#include <string>

namespace uel {
    // Title/Result画面（Play中にViewportへ重ねて描画）
    class ScreenUIComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<ScreenUIComponent>(*this); }
        std::string title = "TITLE";
        std::string subtitle;
        std::string buttonText = "START";
        std::string nextScene = "Stage";
        bool        showResult = false;   // ONならGameFlowの結果（CLEAR/GAME OVER・タイム）を表示
    };
}