#pragma once
#include "Engine/Components/Component.h"
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
        bool        roleSelect = false;   // ONならボタン1つの代わりに、3つのロールのボタンを出す。選んだロールを GameFlow に入れて nextScene へ（1-3b）
    };
}